/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2020 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @file PlatformImplementation.cpp
 *
 * @brief DisplayInfo platform implementation using COM-RPC DeviceSettings plugin.
 *
 *
 * Architecture:
 *   - Inherits DSHelper (single COM-RPC link on root IDeviceSettings).
 *   - Inherits Exchange::IConfiguration so the DisplayInfo proxy can pass IShell*
 *     via Configure(service) and trigger DSHelper::Open().
 *   - Caches video-port, display and video-device handles in OnDeviceSettingsActivated().
 *   - Inner notification delegate (DSVideoPortNotification) routes resolution
 *     pre/post change events to all registered IConnectionProperties::INotification sinks.
 *   - GPU RAM queries still go through SoC_abstraction (displayinfo-soc library).
 *   - EDID parsing uses edid-parser.hpp (still available in the displayinfo sysroot).
 */

#include "../Module.h"
#include "../DisplayInfoTracing.h"
#include "SoC_abstraction.h"

#include <interfaces/IDisplayInfo.h>
#include <interfaces/IConfiguration.h>
#include "DeviceSettingsInterface.h"               // DSHelper + config stores + VP/Audio/VideoDevice sub-interfaces
#include <interfaces/IDeviceSettingsDisplay.h>     // Exchange::IDeviceSettingsDisplay (GetDisplay, GetDisplayEdidBytes)

#include "UtilsLogging.h"

#define EDID_MAX_HORIZONTAL_SIZE 21
#define EDID_MAX_VERTICAL_SIZE   22

namespace WPEFramework {
namespace Plugin {

class DisplayInfoImplementation
    : public Exchange::IGraphicsProperties
    , public Exchange::IConnectionProperties
    , public Exchange::IHDRProperties
    , public Exchange::IDisplayProperties
    , public Exchange::IConfiguration
    , public DSHelper
{
private:
    using HdrteratorImplementation          = RPC::IteratorType<Exchange::IHDRProperties::IHDRIterator>;
    using ColorimetryIteratorImplementation = RPC::IteratorType<Exchange::IDisplayProperties::IColorimetryIterator>;

    std::list<Exchange::IHDRProperties::HDRType> BuildHDRCapabilities(const int capabilities) const
    {
        std::list<Exchange::IHDRProperties::HDRType> hdrCapabilities;

        if (!capabilities) hdrCapabilities.push_back(HDR_OFF);
        if (capabilities & static_cast<int32_t>(Exchange::IDeviceSettingsVideoPort::DS_HDRSTANDARD_HDR10)){
            hdrCapabilities.push_back(HDR_10);
        }
        if (capabilities & static_cast<int32_t>(Exchange::IDeviceSettingsVideoPort::DS_HDRSTANDARD_HDR10PLUS)){
            hdrCapabilities.push_back(HDR_10PLUS);
        }
        if (capabilities & static_cast<int32_t>(Exchange::IDeviceSettingsVideoPort::DS_HDRSTANDARD_HLG)){
            hdrCapabilities.push_back(HDR_HLG);
        }
        if (capabilities & static_cast<int32_t>(Exchange::IDeviceSettingsVideoPort::DS_HDRSTANDARD_DOLBYVISION)){
            hdrCapabilities.push_back(HDR_DOLBYVISION);
        }
        if (capabilities & static_cast<int32_t>(Exchange::IDeviceSettingsVideoPort::DS_HDRSTANDARD_TECHNICOLORPRIME)){
            hdrCapabilities.push_back(HDR_TECHNICOLOR);
        }
        if (capabilities & static_cast<int32_t>(Exchange::IDeviceSettingsVideoPort::DS_HDRSTANDARD_INVALID)){
            hdrCapabilities.push_back(HDR_OFF);
        }
        if (capabilities & static_cast<int32_t>(Exchange::IDeviceSettingsVideoPort::DS_HDRSTANDARD_SDR)){
            hdrCapabilities.push_back(HDR_SDR);
        }

        return hdrCapabilities;
    }

    // -------------------------------------------------------------------------
    // Inner notification delegate: IDeviceSettingsVideoPort::INotification
    //
    // Receives OnResolutionPreChange / OnResolutionPostChange from the
    // DeviceSettings plugin and dispatches them to all registered
    // IConnectionProperties::INotification sinks.
    //
    // A separate delegate class (not 'this') is used so that the COM-RPC
    // framework marshals it correctly and DisplayInfoImplementation does not
    // need to publicly expose the DS-internal notification interface.
    // -------------------------------------------------------------------------
    class DSVideoPortNotification
        : public Exchange::IDeviceSettingsVideoPort::INotification
    {
    public:
        explicit DSVideoPortNotification(DisplayInfoImplementation& parent)
            : _parent(parent)
        {
        }

        DSVideoPortNotification(const DSVideoPortNotification&)            = delete;
        DSVideoPortNotification& operator=(const DSVideoPortNotification&) = delete;

        void OnResolutionPreChange(
            const Exchange::IDeviceSettingsVideoPort::ResolutionChange& /* resolution */) override
        {
            _parent.DispatchResolutionChange(
                IConnectionProperties::INotification::Source::PRE_RESOLUTION_CHANGE);
        }

        void OnResolutionPostChange(
            const Exchange::IDeviceSettingsVideoPort::ResolutionChange& /* resolution */) override
        {
            _parent.DispatchResolutionChange(
                IConnectionProperties::INotification::Source::POST_RESOLUTION_CHANGE);
        }

        BEGIN_INTERFACE_MAP(DSVideoPortNotification)
            INTERFACE_ENTRY(Exchange::IDeviceSettingsVideoPort::INotification)
        END_INTERFACE_MAP

    private:
        DisplayInfoImplementation& _parent;
    };

    // Job submitted to the worker pool by DispatchResolutionChange.
    // Holds a reference to the implementation + source; calls Dispatch(source)
    // on the worker thread so the DS notification thread returns immediately.
    class DispatchJob : public Core::IDispatch {
    public:
        DispatchJob(DisplayInfoImplementation* impl,
                           IConnectionProperties::INotification::Source src)
            : _impl(impl), _src(src) { if (_impl != nullptr) _impl->AddRef(); }
        ~DispatchJob() { if (_impl != nullptr) _impl->Release(); }
        static Core::ProxyType<Core::IDispatch> Create(
            DisplayInfoImplementation* impl,
            IConnectionProperties::INotification::Source src)
        {
            return Core::ProxyType<Core::IDispatch>(
                Core::ProxyType<DispatchJob>::Create(impl, src));
        }
        void Dispatch() override { _impl->Dispatch(_src); }
    private:
        DisplayInfoImplementation* _impl;
        IConnectionProperties::INotification::Source _src;
    };

public:
    DisplayInfoImplementation()
        : _adminLock()
        , _observers()
        , _defaultPortType(VideoPortType::DS_VIDEO_PORT_TYPE_HDMI)
        , _displayHandle(INVALID_DS_HANDLE)
        , _DSVideoPortNotification(*this)
    {
        DisplayInfoImplementation::_instance = this;
    }

    DisplayInfoImplementation(const DisplayInfoImplementation&)            = delete;
    DisplayInfoImplementation& operator=(const DisplayInfoImplementation&) = delete;

    ~DisplayInfoImplementation() override
    {
        // Unregister resolution change notifications before severing the COM-RPC link.
        auto* vp = DSHelper::AcquireSubInterface<Exchange::IDeviceSettingsVideoPort>();
        if (vp != nullptr) {
            vp->Unregister(&_DSVideoPortNotification);
            vp->Release();
        }
        DSHelper::Close();
        DisplayInfoImplementation::_instance = nullptr;
    }

    // -------------------------------------------------------------------------
    // IConfiguration
    // Called by the DisplayInfo proxy (DisplayInfo.cpp) after instantiating the
    // implementation object.  Opens the COM-RPC link to the DeviceSettings plugin;
    // OnDeviceSettingsActivated() fires once DeviceSettings is running.
    // -------------------------------------------------------------------------
    Core::hresult Configure(PluginHost::IShell* service) override
    {
        DSHelper::Open(service, "DisplayInfo");
        return Core::ERROR_NONE;
    }

    // -------------------------------------------------------------------------
    // DSHelper lifecycle overrides
    // -------------------------------------------------------------------------

    /**
     * Called when the DeviceSettings plugin activates (or re-activates after a
     * crash/restart).  Caches the default port type, registers the resolution
     * change notification delegate, and acquires the display handle needed for
     * EDID queries.  All config stores are loaded lazily by DSHelper on the
     * first accessor call — no explicit LoadVideoPortConfig/LoadAudioConfig/
     * LoadVideoDeviceConfig calls are needed here.
     */
    void OnDeviceSettingsActivated() override
    {
        LOGINFO("DisplayInfo: DeviceSettings activated — caching handles and registering notifications");

        // ---- 1. Set default port type from DSHelper config ----
        const std::string defaultVP = DSHelper::getDefaultVideoPortName();
        VideoPortEntry defaultEntry{};
        const bool entryResolved = DSHelper::resolveVideoPortByName(defaultVP, defaultEntry);
        if (entryResolved) {
            _defaultPortType = defaultEntry.type;
            LOGINFO("Default video port: '%s' type=%d handle=%d",
                    defaultVP.c_str(), static_cast<int>(_defaultPortType),
                    DSHelper::getCachedVideoPortHandle(defaultVP));
        } else {
            LOGERR("OnDeviceSettingsActivated: failed to resolve default video port '%s'", defaultVP.c_str());
        }

        // ---- 2. Register resolution change notifications ----
        {
            auto* vp = DSHelper::AcquireSubInterface<Exchange::IDeviceSettingsVideoPort>();
            if (vp != nullptr) {
                vp->Register("DisplayInfo", &_DSVideoPortNotification);
                vp->Release();
            } else {
                LOGERR("OnDeviceSettingsActivated: IDeviceSettingsVideoPort not available");
            }
        }

        // ---- 3. Acquire display handle for the default port ----
        // Display handles are not cached by DSHelper::LoadAllConfigs — acquire explicitly.
        _displayHandle = INVALID_DS_HANDLE;
        if (entryResolved && DSHelper::getCachedVideoPortHandle(defaultVP) != INVALID_DS_HANDLE) {
            auto* disp = DSHelper::AcquireSubInterface<Exchange::IDeviceSettingsDisplay>();
            if (disp != nullptr) {
                Exchange::IDeviceSettingsDisplay::DisplayPortType dpType =
                    static_cast<Exchange::IDeviceSettingsDisplay::DisplayPortType>(defaultEntry.type);
                Core::hresult rc = disp->GetDisplay(dpType, defaultEntry.index, _displayHandle);
                disp->Release();
                if (rc != Core::ERROR_NONE) {
                    LOGERR("OnDeviceSettingsActivated: GetDisplay failed: %u", rc);
                    _displayHandle = INVALID_DS_HANDLE;
                } else {
                    LOGINFO("Cached display handle: %d", _displayHandle);
                }
            } else {
                LOGERR("OnDeviceSettingsActivated: IDeviceSettingsDisplay not available");
            }
        }
    }

    /**
     * Called when the DeviceSettings plugin deactivates.
     * The COM-RPC connection is already severed — must NOT call any interface methods here.
     * DSHelper::Operational(false) already clears all config stores and port/device handles.
     */
    void OnDeviceSettingsDeactivated() override
    {
        LOGINFO("DisplayInfo: DeviceSettings deactivated — clearing cached handles");
        _displayHandle   = INVALID_DS_HANDLE;
        _defaultPortType = VideoPortType::DS_VIDEO_PORT_TYPE_HDMI;
    }

    // -------------------------------------------------------------------------
    // IGraphicsProperties
    // -------------------------------------------------------------------------

    Core::hresult TotalGpuRam(uint64_t& total) const override
    {
        total = SoC_GetTotalGpuRam();
        return Core::ERROR_NONE;
    }

    Core::hresult FreeGpuRam(uint64_t& free) const override
    {
        free = SoC_GetFreeGpuRam();
        return Core::ERROR_NONE;
    }

    // -------------------------------------------------------------------------
    // IConnectionProperties — observer management
    // -------------------------------------------------------------------------

    Core::hresult Register(INotification* notification) override
    {
        _adminLock.Lock();
        ASSERT(std::find(_observers.begin(), _observers.end(), notification) == _observers.end());
        _observers.push_back(notification);
        notification->AddRef();
        _adminLock.Unlock();
        return Core::ERROR_NONE;
    }

    Core::hresult Unregister(INotification* notification) override
    {
        _adminLock.Lock();
        auto index = std::find(_observers.begin(), _observers.end(), notification);
        ASSERT(index != _observers.end());
        if (index != _observers.end()) {
            (*index)->Release();
            _observers.erase(index);
        }
        _adminLock.Unlock();
        return Core::ERROR_NONE;
    }

    // -------------------------------------------------------------------------
    // IConnectionProperties — query methods
    // -------------------------------------------------------------------------

    Core::hresult IsAudioPassthrough(bool& value) const override
    {
        value = false;
        std::vector<AudioPortEntry> audioEntries;
        if (!DSHelper::getAudioPortEntries(audioEntries)) {
            LOGERR("IsAudioPassthrough: audio config not available");
            return Core::ERROR_UNAVAILABLE;
        }
        const std::string audioPortName = DSHelper::getDefaultAudioPortName();

        auto* audio = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
        if (audio == nullptr) {
            LOGERR("IsAudioPassthrough: IDeviceSettingsAudio not available");
            return Core::ERROR_UNAVAILABLE;
        }

        // isAudioOutputPortConnected() fills audioHandle from the cache AND
        // verifies the port is physically connected (HDMI: display present,
        // ARC: HDMI-In status, HEADPHONE: IsAudioOutputConnected, others: always true).
        int32_t audioHandle = INVALID_DS_HANDLE;
        if (!const_cast<DisplayInfoImplementation*>(this)->isAudioOutputPortConnected(
                audio, audioPortName, audioHandle)) {
            LOGWARN("IsAudioPassthrough: audio port '%s' not connected — passthrough false",
                    audioPortName.c_str());
            audio->Release();
            return Core::ERROR_NONE;  // not connected: passthrough is implicitly false
        }

        Exchange::IDeviceSettingsAudio::StereoMode mode =
            Exchange::IDeviceSettingsAudio::AUDIO_STEREO_UNKNOWN;
        Core::hresult rc = audio->GetStereoMode(audioHandle, mode, true);
        if (rc == Core::ERROR_NONE) {
            value = (mode == Exchange::IDeviceSettingsAudio::AUDIO_STEREO_PASSTHROUGH);
        }
        audio->Release();
        return rc;
    }

    Core::hresult Connected(bool& connected) const override
    {
        connected = false;
        const int32_t vpHandle = DSHelper::getCachedVideoPortHandle(DSHelper::getDefaultVideoPortName());
        if (vpHandle == INVALID_DS_HANDLE) {
            LOGERR("Connected: video port handle not available");
            return Core::ERROR_UNAVAILABLE;
        }
        auto* vp = AcquireSubInterface<Exchange::IDeviceSettingsVideoPort>();
        if (vp == nullptr) {
            LOGERR("Connected: IDeviceSettingsVideoPort not available");
            return Core::ERROR_UNAVAILABLE;
        }
        Core::hresult rc = vp->IsVideoPortDisplayConnected(vpHandle, connected);
        vp->Release();
        return rc;
    }

    Core::hresult Width(uint32_t& value) const override
    {
        value = 0;
        std::vector<uint8_t> edidVec;
        if (GetEdidBytes(edidVec) == Core::ERROR_NONE && edidVec.size() > 62) {
            // EDID preferred detailed timing: bytes 54–71
            // Byte 56: H active [7:0]   Byte 58 [7:4]: H active [11:8]
            value = ((static_cast<uint32_t>(edidVec[58]) >> 4) << 8) | edidVec[56];
            LOGINFO("Width from EDID = %u", value);
        } else {
            LOGERR("Width: EDID not available or too short");
        }
        return Core::ERROR_NONE;
    }

    Core::hresult Height(uint32_t& value) const override
    {
        value = 0;
        std::vector<uint8_t> edidVec;
        if (GetEdidBytes(edidVec) == Core::ERROR_NONE && edidVec.size() > 62) {
            // EDID preferred detailed timing: bytes 54–71
            // Byte 59: V active [7:0]   Byte 61 [7:4]: V active [11:8]
            value = ((static_cast<uint32_t>(edidVec[61]) >> 4) << 8) | edidVec[59];
            LOGINFO("Height from EDID = %u", value);
        } else {
            LOGERR("Height: EDID not available or too short");
        }
        return Core::ERROR_NONE;
    }

    Core::hresult VerticalFreq(uint32_t& value) const override
    {
        value = 0;
        std::vector<uint8_t> edidVec;
        if (GetEdidBytes(edidVec) != Core::ERROR_NONE || edidVec.size() <= 62) {
            LOGERR("VerticalFreq: EDID not available or too short");
            return Core::ERROR_GENERAL;
        }
        // EDID preferred detailed timing: bytes 54–71
        // Pixel clock: bytes 54–55 (units of 10 kHz, little-endian)
        // H active [11:8]=byte58[7:4], H active [7:0]=byte56
        // H blanking [11:8]=byte58[3:0], H blanking [7:0]=byte57
        // V active [11:8]=byte61[7:4], V active [7:0]=byte59
        // V blanking [11:8]=byte61[3:0], V blanking [7:0]=byte60
        uint32_t pixelClock = (static_cast<uint32_t>(edidVec[55]) << 8 | edidVec[54]) * 10000u;
        uint32_t hActive    = ((static_cast<uint32_t>(edidVec[58]) >> 4) << 8) | edidVec[56];
        uint32_t hBlanking  = ((static_cast<uint32_t>(edidVec[58]) & 0x0F) << 8) | edidVec[57];
        uint32_t vActive    = ((static_cast<uint32_t>(edidVec[61]) >> 4) << 8) | edidVec[59];
        uint32_t vBlanking  = ((static_cast<uint32_t>(edidVec[61]) & 0x0F) << 8) | edidVec[60];
        uint32_t hTotal = hActive + hBlanking;
        uint32_t vTotal = vActive + vBlanking;
        if (hTotal > 0 && vTotal > 0 && pixelClock > 0) {
            value = pixelClock / (hTotal * vTotal);
            TRACE(Trace::Information, (_T("Vertical frequency = %u"), value));
        } else {
            LOGERR("VerticalFreq: invalid timing values in EDID");
            return Core::ERROR_GENERAL;
        }
        return Core::ERROR_NONE;
    }

    Core::hresult HDCPProtection(HDCPProtectionType& value) const override  // get
    {
        value = IConnectionProperties::HDCPProtectionType::HDCP_AUTO;
        const int32_t vpHandle = DSHelper::getCachedVideoPortHandle(DSHelper::getDefaultVideoPortName());
        if (vpHandle == INVALID_DS_HANDLE) {
            LOGERR("HDCPProtection(get): video port handle not available");
            return Core::ERROR_UNAVAILABLE;
        }
        auto* vp = AcquireSubInterface<Exchange::IDeviceSettingsVideoPort>();
        if (vp == nullptr) {
            LOGERR("HDCPProtection(get): IDeviceSettingsVideoPort not available");
            return Core::ERROR_UNAVAILABLE;
        }
        Exchange::IDeviceSettingsVideoPort::HDCPProtocolVersion version =
            Exchange::IDeviceSettingsVideoPort::DS_HDCP_VERSION_MAX;
        Core::hresult rc = vp->GetHDMIPreference(vpHandle, version);
        vp->Release();
        if (rc == Core::ERROR_NONE) {
            switch (version) {
            case Exchange::IDeviceSettingsVideoPort::DS_HDCP_VERSION_1X:
                value = IConnectionProperties::HDCPProtectionType::HDCP_1X;   break;
            case Exchange::IDeviceSettingsVideoPort::DS_HDCP_VERSION_2X:
                value = IConnectionProperties::HDCPProtectionType::HDCP_2X;   break;
            default:
                value = IConnectionProperties::HDCPProtectionType::HDCP_AUTO; break;
            }
        }
        return rc;
    }

    Core::hresult HDCPProtection(const HDCPProtectionType value) override  // set
    {
        const int32_t vpHandle = DSHelper::getCachedVideoPortHandle(DSHelper::getDefaultVideoPortName());
        if (vpHandle == INVALID_DS_HANDLE) {
            LOGERR("HDCPProtection(set): video port handle not available");
            return Core::ERROR_UNAVAILABLE;
        }
        auto* vp = AcquireSubInterface<Exchange::IDeviceSettingsVideoPort>();
        if (vp == nullptr) {
            LOGERR("HDCPProtection(set): IDeviceSettingsVideoPort not available");
            return Core::ERROR_UNAVAILABLE;
        }
        Exchange::IDeviceSettingsVideoPort::HDCPProtocolVersion version =
            Exchange::IDeviceSettingsVideoPort::DS_HDCP_VERSION_MAX;
        switch (value) {
        case IConnectionProperties::HDCPProtectionType::HDCP_1X:
            version = Exchange::IDeviceSettingsVideoPort::DS_HDCP_VERSION_1X; break;
        case IConnectionProperties::HDCPProtectionType::HDCP_2X:
            version = Exchange::IDeviceSettingsVideoPort::DS_HDCP_VERSION_2X; break;
        default:
            break;
        }
        Core::hresult rc = vp->SetHDMIPreference(vpHandle, version);
        vp->Release();
        return rc;
    }

    Core::hresult WidthInCentimeters(uint8_t& width /* @out */) const override
    {
        width = 0;
        if (IsDisplayAccessible()) {
            std::vector<uint8_t> edidVec;
            if (GetEdidBytes(edidVec) == Core::ERROR_NONE) {
                if (edidVec.size() > EDID_MAX_VERTICAL_SIZE) {
                    width = edidVec[EDID_MAX_HORIZONTAL_SIZE];
                    LOGINFO("Width in cm = %d", width);
                } else {
                    LOGERR("Failed to get Display Size!");
                }
            }
        } else {
            LOGERR("Display not accessible");
        }
        return Core::ERROR_NONE;
    }

    Core::hresult HeightInCentimeters(uint8_t& height /* @out */) const override
    {
        height = 0;
        if (IsDisplayAccessible()) {
            std::vector<uint8_t> edidVec;
            if (GetEdidBytes(edidVec) == Core::ERROR_NONE) {
                if (edidVec.size() > EDID_MAX_VERTICAL_SIZE) {
                    height = edidVec[EDID_MAX_VERTICAL_SIZE];
                    LOGINFO("Height in cm = %d", height);
                } else {
                    LOGERR("Failed to get Display Size!");
                }
            }
        } else {
            LOGERR("Display not accessible");
        }
        return Core::ERROR_NONE;
    }

    Core::hresult EDID(uint16_t& length /* @inout */, uint8_t data[] /* @out @length:length */) const override
    {
        std::vector<uint8_t> edidVec({'u', 'n', 'k', 'n', 'o', 'w', 'n'});
        Core::hresult ret = Core::ERROR_NONE;

        if (IsDisplayAccessible()) {
            std::vector<uint8_t> edidVec2;
            if (GetEdidBytes(edidVec2) == Core::ERROR_NONE) {
                edidVec = std::move(edidVec2);
            } else {
                LOGERR("GetEdidBytes failed");
                ret = Core::ERROR_GENERAL;
            }
        } else {
            LOGERR("failure: display not accessible!");
            ret = Core::ERROR_GENERAL;
        }

        uint16_t size = static_cast<uint16_t>(
            std::min(edidVec.size(), static_cast<size_t>(std::numeric_limits<uint16_t>::max())));
        if (edidVec.size() > static_cast<size_t>(std::numeric_limits<uint16_t>::max())) {
            LOGERR("Size too large to use ToString base64 wpe api");
        }

        if (size > 0) {
            string edidBase64;
            Core::ToString(edidVec.data(), size, true, edidBase64);
            LOGINFO("EDID base64: %s", edidBase64.c_str());
        }

        int i = 0;
        for (; i < length && i < size; ++i) {
            data[i] = edidVec[i];
        }
        length = static_cast<uint16_t>(i);
        return ret;
    }

    /**
     * Returns the name of the first connected HDMI or Internal video output port.
     * Mirrors the libds PortName() scan used by HDCPProtection(get/set).
     */
    Core::hresult PortName(string& name /* @out */) const override
    {
        name.clear();
        std::vector<VideoPortEntry> entries;
        if (!DSHelper::getVideoPortEntries(entries)) {
            LOGERR("PortName: video port config not available");
            return Core::ERROR_UNAVAILABLE;
        }

        auto* vp = AcquireSubInterface<Exchange::IDeviceSettingsVideoPort>();
        if (vp == nullptr) {
            LOGERR("PortName: IDeviceSettingsVideoPort not available");
            return Core::ERROR_UNAVAILABLE;
        }

        for (const auto& entry : entries) {
            if (entry.type != VideoPortType::DS_VIDEO_PORT_TYPE_HDMI &&
                entry.type != VideoPortType::DS_VIDEO_PORT_TYPE_INTERNAL) {
                continue;
            }
            int32_t handle = INVALID_DS_HANDLE;
            if (vp->GetVideoPort(entry.type, entry.index, handle) != Core::ERROR_NONE) {
                continue;
            }
            bool connected = false;
            if (vp->IsVideoPortDisplayConnected(handle, connected) == Core::ERROR_NONE && connected) {
                name = entry.name;
                TRACE(Trace::Information, (_T("Connected video output port = %s"), name.c_str()));
                break;
            }
        }
        vp->Release();

        if (name.empty()) {
            LOGERR("PortName: no connected HDMI or Internal video output port found");
        }
        return Core::ERROR_NONE;
    }

    // -------------------------------------------------------------------------
    // IDisplayProperties
    // -------------------------------------------------------------------------

    Core::hresult ColorSpace(ColourSpaceType& cs /* @out */) const override
    {
        cs = FORMAT_UNKNOWN;
        if (!IsDisplayAccessible()) {
            LOGERR("ColorSpace: HDMI not connected!");
            return Core::ERROR_NONE;
        }

        auto* vp = AcquireSubInterface<Exchange::IDeviceSettingsVideoPort>();
        if (vp == nullptr) {
            LOGERR("ColorSpace: IDeviceSettingsVideoPort not available");
            return Core::ERROR_UNAVAILABLE;
        }

        Exchange::IDeviceSettingsVideoPort::DisplayColorSpace _cs =
            Exchange::IDeviceSettingsVideoPort::DS_DISPLAY_COLORSPACE_UNKNOWN;
        Core::hresult rc = vp->GetColorSpace(DSHelper::getCachedVideoPortHandle(DSHelper::getDefaultVideoPortName()), _cs);
        vp->Release();

        if (rc == Core::ERROR_NONE) {
            TRACE(Trace::Information, (_T("colour space = %d"), static_cast<int>(_cs)));
            switch (_cs) {
            case Exchange::IDeviceSettingsVideoPort::DS_DISPLAY_COLORSPACE_RGB:
                cs = FORMAT_RGB_444; break;
            case Exchange::IDeviceSettingsVideoPort::DS_DISPLAY_COLORSPACE_YCBCR444:
                cs = FORMAT_YCBCR_444; break;
            case Exchange::IDeviceSettingsVideoPort::DS_DISPLAY_COLORSPACE_YCBCR422:
                cs = FORMAT_YCBCR_422; break;
            case Exchange::IDeviceSettingsVideoPort::DS_DISPLAY_COLORSPACE_YCBCR420:
                cs = FORMAT_YCBCR_420; break;
            case Exchange::IDeviceSettingsVideoPort::DS_DISPLAY_COLORSPACE_AUTO:
                cs = FORMAT_OTHER; break;
            default:
                cs = FORMAT_UNKNOWN; break;
            }
        }
        return rc;
    }

    Core::hresult FrameRate(FrameRateType& rate /* @out */) const override
    {
        rate = FRAMERATE_UNKNOWN;
        if (!IsDisplayAccessible()) {
            LOGERR("FrameRate: HDMI not connected!");
            return Core::ERROR_NONE;
        }
        const int32_t vpHandle = DSHelper::getCachedVideoPortHandle(DSHelper::getDefaultVideoPortName());
        if (vpHandle == INVALID_DS_HANDLE) {
            LOGERR("FrameRate: video port handle not available");
            return Core::ERROR_UNAVAILABLE;
        }
        auto* vp = AcquireSubInterface<Exchange::IDeviceSettingsVideoPort>();
        if (vp == nullptr) {
            LOGERR("FrameRate: IDeviceSettingsVideoPort not available");
            return Core::ERROR_UNAVAILABLE;
        }
        Exchange::IDeviceSettingsVideoPort::VideoPortResolution resolution;
        Core::hresult rc = vp->GetVideoPortResolution(vpHandle, resolution);
        vp->Release();

        if (rc == Core::ERROR_NONE) {
            switch (resolution.frameRate) {
            case Exchange::IDeviceSettingsVideoPort::DS_VIDEO_FRAMERATE_24:    rate = FRAMERATE_24;      break;
            case Exchange::IDeviceSettingsVideoPort::DS_VIDEO_FRAMERATE_25:    rate = FRAMERATE_25;      break;
            case Exchange::IDeviceSettingsVideoPort::DS_VIDEO_FRAMERATE_30:    rate = FRAMERATE_30;      break;
            case Exchange::IDeviceSettingsVideoPort::DS_VIDEO_FRAMERATE_60:    rate = FRAMERATE_60;      break;
            case Exchange::IDeviceSettingsVideoPort::DS_VIDEO_FRAMERATE_23_98: rate = FRAMERATE_23_976;  break;
            case Exchange::IDeviceSettingsVideoPort::DS_VIDEO_FRAMERATE_29_97: rate = FRAMERATE_29_97;   break;
            case Exchange::IDeviceSettingsVideoPort::DS_VIDEO_FRAMERATE_50:    rate = FRAMERATE_50;      break;
            case Exchange::IDeviceSettingsVideoPort::DS_VIDEO_FRAMERATE_59_94: rate = FRAMERATE_59_94;   break;
            default:                                                            rate = FRAMERATE_UNKNOWN; break;
            }
        }
        return rc;
    }

    Core::hresult ColourDepth(ColourDepthType& colour /* @out */) const override
    {
        colour = COLORDEPTH_UNKNOWN;
        if (!IsDisplayAccessible()) {
            LOGERR("ColourDepth: HDMI not connected!");
            return Core::ERROR_NONE;
        }
        auto* vp = AcquireSubInterface<Exchange::IDeviceSettingsVideoPort>();
        if (vp == nullptr) {
            LOGERR("ColourDepth: IDeviceSettingsVideoPort not available");
            return Core::ERROR_UNAVAILABLE;
        }

        uint32_t colorDepth = 0;
        Core::hresult rc = vp->GetColorDepth(DSHelper::getCachedVideoPortHandle(DSHelper::getDefaultVideoPortName()), colorDepth);
        vp->Release();

        if (rc == Core::ERROR_NONE) {
            TRACE(Trace::Information, (_T("colour depth bitmask = 0x%x"), colorDepth));
            // GetColorDepth returns a DisplayColorDepth bitmask: 8BIT=0x01, 10BIT=0x02, 12BIT=0x04
            if (colorDepth & static_cast<uint32_t>(Exchange::IDeviceSettingsVideoPort::DS_DISPLAY_COLORDEPTH_12BIT)) {
                colour = COLORDEPTH_12_BIT;
            } else if (colorDepth & static_cast<uint32_t>(Exchange::IDeviceSettingsVideoPort::DS_DISPLAY_COLORDEPTH_10BIT)) {
                colour = COLORDEPTH_10_BIT;
            } else if (colorDepth & static_cast<uint32_t>(Exchange::IDeviceSettingsVideoPort::DS_DISPLAY_COLORDEPTH_8BIT)) {
                colour = COLORDEPTH_8_BIT;
            } else {
                colour = COLORDEPTH_UNKNOWN;
            }
        }
        return rc;
    }

    Core::hresult QuantizationRange(QuantizationRangeType& qr /* @out */) const override
    {
        qr = QUANTIZATIONRANGE_UNKNOWN;
        if (!IsDisplayAccessible()) {
            LOGERR("QuantizationRange: HDMI not connected!");
            return Core::ERROR_NONE;
        }
        auto* vp = AcquireSubInterface<Exchange::IDeviceSettingsVideoPort>();
        if (vp == nullptr) {
            LOGERR("QuantizationRange: IDeviceSettingsVideoPort not available");
            return Core::ERROR_UNAVAILABLE;
        }

        Exchange::IDeviceSettingsVideoPort::DisplayQuantizationRange _qr =
            Exchange::IDeviceSettingsVideoPort::DS_DISPLAY_QUANTIZATIONRANGE_UNKNOWN;
        Core::hresult rc = vp->GetQuantizationRange(DSHelper::getCachedVideoPortHandle(DSHelper::getDefaultVideoPortName()), _qr);
        vp->Release();

        if (rc == Core::ERROR_NONE) {
            TRACE(Trace::Information, (_T("quantization range = %d"), static_cast<int>(_qr)));
            switch (_qr) {
            case Exchange::IDeviceSettingsVideoPort::DS_DISPLAY_QUANTIZATIONRANGE_LIMITED:
                qr = QUANTIZATIONRANGE_LIMITED; break;
            case Exchange::IDeviceSettingsVideoPort::DS_DISPLAY_QUANTIZATIONRANGE_FULL:
                qr = QUANTIZATIONRANGE_FULL; break;
            default:
                qr = QUANTIZATIONRANGE_UNKNOWN; break;
            }
        }
        return rc;
    }

    Core::hresult Colorimetry(IColorimetryIterator*& colorimetry /* @out */) const override
    {
        std::list<Exchange::IDisplayProperties::ColorimetryType> colorimetryCaps;

        std::vector<uint8_t> edidVec;
        if (GetEdidBytes(edidVec) == Core::ERROR_NONE) {
            uint32_t colorimetry_info = ParseColorimetryFromEdid(edidVec);
            LOGINFO("colorimetry = 0x%x", colorimetry_info);
            if (!colorimetry_info) colorimetryCaps.push_back(COLORIMETRY_UNKNOWN);
            if (colorimetry_info & Exchange::IDeviceSettingsDisplay::DS_DISPLAY_COLORIMETRY_INFO_XVYCC601)    colorimetryCaps.push_back(COLORIMETRY_XVYCC601);
            if (colorimetry_info & Exchange::IDeviceSettingsDisplay::DS_DISPLAY_COLORIMETRY_INFO_XVYCC709)    colorimetryCaps.push_back(COLORIMETRY_XVYCC709);
            if (colorimetry_info & Exchange::IDeviceSettingsDisplay::DS_DISPLAY_COLORIMETRY_INFO_SYCC601)     colorimetryCaps.push_back(COLORIMETRY_SYCC601);
            if (colorimetry_info & Exchange::IDeviceSettingsDisplay::DS_DISPLAY_COLORIMETRY_INFO_ADOBEYCC601) colorimetryCaps.push_back(COLORIMETRY_OPYCC601);
            if (colorimetry_info & Exchange::IDeviceSettingsDisplay::DS_DISPLAY_COLORIMETRY_INFO_ADOBERGB)    colorimetryCaps.push_back(COLORIMETRY_OPRGB);
            if ((colorimetry_info & Exchange::IDeviceSettingsDisplay::DS_DISPLAY_COLORIMETRY_INFO_BT2020CL) ||
                (colorimetry_info & Exchange::IDeviceSettingsDisplay::DS_DISPLAY_COLORIMETRY_INFO_BT2020NCL)) colorimetryCaps.push_back(COLORIMETRY_BT2020YCCBCBRC);
            if (colorimetry_info & Exchange::IDeviceSettingsDisplay::DS_DISPLAY_COLORIMETRY_INFO_BT2020RGB)   colorimetryCaps.push_back(COLORIMETRY_BT2020RGB_YCBCR);
            if (colorimetry_info & Exchange::IDeviceSettingsDisplay::DS_DISPLAY_COLORIMETRY_INFO_DCI_P3)      colorimetryCaps.push_back(COLORIMETRY_OTHER);
        } else {
            LOGERR("Display not connected, returning empty colorimetry list");
        }

        colorimetry = Core::Service<ColorimetryIteratorImplementation>::Create<Exchange::IDisplayProperties::IColorimetryIterator>(colorimetryCaps);
        return Core::ERROR_NONE;
    }

    Core::hresult EOTF(EotfType& eotf /* @out */) const override
    {
        eotf = EOTF_UNKNOWN;
        if (!IsDisplayAccessible()) {
            LOGERR("EOTF: display not accessible");
            return Core::ERROR_UNAVAILABLE;
        }
        auto* vp = AcquireSubInterface<Exchange::IDeviceSettingsVideoPort>();
        if (vp == nullptr) {
            LOGERR("EOTF: IDeviceSettingsVideoPort not available");
            return Core::ERROR_UNAVAILABLE;
        }

        Exchange::IDeviceSettingsVideoPort::HDRStandard hdrStandard =
            Exchange::IDeviceSettingsVideoPort::DS_HDRSTANDARD_NONE;
        Core::hresult rc = vp->GetVideoEOTF(DSHelper::getCachedVideoPortHandle(DSHelper::getDefaultVideoPortName()), hdrStandard);
        vp->Release();

        if (rc == Core::ERROR_NONE) {
            TRACE(Trace::Information, (_T("videoEOTF = %d"), static_cast<int>(hdrStandard)));
            switch (hdrStandard) {
            /* bt1886 = SDR; smpte2084 = HDR10; bt2100 = HLG */
            case Exchange::IDeviceSettingsVideoPort::DS_HDRSTANDARD_HDR10:
                eotf = EOTF_SMPTE_ST_2084; break;
            case Exchange::IDeviceSettingsVideoPort::DS_HDRSTANDARD_HLG:
                eotf = EOTF_BT2100; break;
            default:
                eotf = EOTF_UNKNOWN; break;
            }
        }
        return rc;
    }

    Core::hresult GetCurrentColorimetry(ColorimetryTypeInfo& info /* @out */) const override
    {
        info.colorimetry = COLORIMETRY_UNKNOWN;
        if (!IsDisplayAccessible()) {
            LOGERR("GetCurrentColorimetry: display not accessible");
            return Core::ERROR_NONE;
        }
        auto* vp = AcquireSubInterface<Exchange::IDeviceSettingsVideoPort>();
        if (vp == nullptr) {
            LOGERR("GetCurrentColorimetry: IDeviceSettingsVideoPort not available");
            return Core::ERROR_UNAVAILABLE;
        }

        Exchange::IDeviceSettingsVideoPort::DisplayMatrixCoefficients matCoeff =
            Exchange::IDeviceSettingsVideoPort::DS_DISPLAY_MATRIXCOEFFICIENT_UNKNOWN;
        Core::hresult rc = vp->GetMatrixCoefficients(DSHelper::getCachedVideoPortHandle(DSHelper::getDefaultVideoPortName()), matCoeff);
        vp->Release();

        if (rc == Core::ERROR_NONE) {
            LOGINFO("GetCurrentColorimetry: matrixCoefficients=%d", static_cast<int>(matCoeff));
            switch (matCoeff) {
            case Exchange::IDeviceSettingsVideoPort::DS_DISPLAY_MATRIXCOEFFICIENT_BT_709:
                info.colorimetry = COLORIMETRY_BT709; break;
            case Exchange::IDeviceSettingsVideoPort::DS_DISPLAY_MATRIXCOEFFICIENT_SMPTE_170M:
                info.colorimetry = COLORIMETRY_SMPTE170M; break;
            case Exchange::IDeviceSettingsVideoPort::DS_DISPLAY_MATRIXCOEFFICIENT_XVYCC_709:
                info.colorimetry = COLORIMETRY_XVYCC709; break;
            case Exchange::IDeviceSettingsVideoPort::DS_DISPLAY_MATRIXCOEFFICIENT_EXVYCC_601:
                info.colorimetry = COLORIMETRY_XVYCC601; break;
            case Exchange::IDeviceSettingsVideoPort::DS_DISPLAY_MATRIXCOEFFICIENT_BT_2020_NCL:
                info.colorimetry = COLORIMETRY_BT2020RGB_YCBCR; break;
            case Exchange::IDeviceSettingsVideoPort::DS_DISPLAY_MATRIXCOEFFICIENT_BT_2020_CL:
                info.colorimetry = COLORIMETRY_BT2020YCCBCBRC; break;
            case Exchange::IDeviceSettingsVideoPort::DS_DISPLAY_MATRIXCOEFFICIENT_UNKNOWN:
                info.colorimetry = COLORIMETRY_UNKNOWN; break;
            default:
                info.colorimetry = COLORIMETRY_OTHER; break;
            }
        }
        if (info.colorimetry == COLORIMETRY_UNKNOWN) {
            LOGERR("No active display connected, returning COLORIMETRY_UNKNOWN");
        }
        return Core::ERROR_NONE;
    }

    // -------------------------------------------------------------------------
    // IHDRProperties
    // -------------------------------------------------------------------------

    /**
     * @property
     * @brief HDR formats supported by TV
     * @return HDRType: array of HDR formats
     */
    Core::hresult TVCapabilities(IHDRIterator*& type /* out */) const override
    {
        int32_t capabilities = 0;

        if (IsDisplayAccessible()) {
            auto* vp = AcquireSubInterface<Exchange::IDeviceSettingsVideoPort>();
            if (vp != nullptr) {
                vp->GetTVHDRCapabilities(DSHelper::getCachedVideoPortHandle(DSHelper::getDefaultVideoPortName()), capabilities);
                vp->Release();
            }
        }

        const std::list<Exchange::IHDRProperties::HDRType> hdrCapabilities = BuildHDRCapabilities(capabilities);

        type = Core::Service<HdrteratorImplementation>::Create<Exchange::IHDRProperties::IHDRIterator>(hdrCapabilities);
        return (type != nullptr ? Core::ERROR_NONE : Core::ERROR_GENERAL);
    }

    /**
     * @property
     * @brief HDR formats supported by STB
     * @return HDRType: array of HDR formats
     */
    Core::hresult STBCapabilities(IHDRIterator*& type /* out */) const override
    {
        int32_t capabilities = 0;

        if (DSHelper::getCachedVideoDeviceHandle(0) != INVALID_DS_HANDLE) {
            auto* vd = AcquireSubInterface<Exchange::IDeviceSettingsVideoDevice>();
            if (vd != nullptr) {
                vd->GetHDRCapabilities(DSHelper::getCachedVideoDeviceHandle(0), capabilities);
                vd->Release();
            }
        }

        const std::list<Exchange::IHDRProperties::HDRType> hdrCapabilities = BuildHDRCapabilities(capabilities);

        type = Core::Service<HdrteratorImplementation>::Create<Exchange::IHDRProperties::IHDRIterator>(hdrCapabilities);
        return (type != nullptr ? Core::ERROR_NONE : Core::ERROR_GENERAL);
    }

    /**
     * @property
     * @brief HDR format in use
     * @param type: HDR format
     */
    Core::hresult HDRSetting(HDRType& type /* @out */) const override
    {
        type = IHDRProperties::HDRType::HDR_OFF;
        bool isHdr = false;

        if (IsDisplayAccessible()) {
            auto* vp = AcquireSubInterface<Exchange::IDeviceSettingsVideoPort>();
            if (vp != nullptr) {
                vp->IsVideoPortOutputHDR(DSHelper::getCachedVideoPortHandle(DSHelper::getDefaultVideoPortName()), isHdr);
                vp->Release();
            }
        }

        TRACE(Trace::Information, (_T("Output HDR = %s"), isHdr ? "Yes" : "No"));
        type = isHdr ? HDR_10 : HDR_OFF;
        return Core::ERROR_NONE;
    }

    BEGIN_INTERFACE_MAP(DisplayInfoImplementation)
        INTERFACE_ENTRY(Exchange::IGraphicsProperties)
        INTERFACE_ENTRY(Exchange::IConnectionProperties)
        INTERFACE_ENTRY(Exchange::IHDRProperties)
        INTERFACE_ENTRY(Exchange::IDisplayProperties)
        INTERFACE_ENTRY(Exchange::IConfiguration)
    END_INTERFACE_MAP

private:
    /**
     * Returns true if the display output is accessible for property queries.
     *
     * For internal display panels (TV platforms, e.g. Sharp), the plug-detect
     * call IsVideoPortDisplayConnected() is not meaningful — the panel is always
     * present.  Skip that check and return true whenever the handle is valid.
     * For HDMI ports, perform the actual connectivity check.
     */
    bool IsDisplayAccessible() const
    {
        const int32_t vpHandle = DSHelper::getCachedVideoPortHandle(DSHelper::getDefaultVideoPortName());
        if (vpHandle == INVALID_DS_HANDLE) {
            LOGERR("IsDisplayAccessible: video port handle not available (%d)", vpHandle);
            return false;
        }
        LOGINFO("IsDisplayAccessible: portHandle=%d portType=%d",
                vpHandle, static_cast<int>(_defaultPortType));
        if (_defaultPortType == VideoPortType::DS_VIDEO_PORT_TYPE_INTERNAL) {
            return true;
        }
        bool connected = false;
        auto* vp = AcquireSubInterface<Exchange::IDeviceSettingsVideoPort>();
        if (vp == nullptr) {
            LOGERR("IsDisplayAccessible: IDeviceSettingsVideoPort not available");
            return false;
        }
        Core::hresult rc = vp->IsVideoPortDisplayConnected(vpHandle, connected);
        vp->Release();
        LOGINFO("IsDisplayAccessible: IsVideoPortDisplayConnected rc=%u connected=%s",
                rc, connected ? "true" : "false");
        return connected;
    }

    /**
     * Parses the CTA-861 extension block to extract colorimetry capabilities
     * WITHOUT calling EDID_Parse (which crashes on some Sharp TV EDID data).
     *
     * Constant mapping (edid_parser::colorimetry_info_t, 1:1 with CTA-861 CDB byte 1):
     *   COLORIMETRY_INFO_XVYCC601   = 0x01  (CDB byte1, bit 0)
     *   COLORIMETRY_INFO_XVYCC709   = 0x02  (CDB byte1, bit 1)
     *   COLORIMETRY_INFO_SYCC601    = 0x04  (CDB byte1, bit 2)
     *   COLORIMETRY_INFO_ADOBEYCC601= 0x08  (CDB byte1, bit 3)
     *   COLORIMETRY_INFO_ADOBERGB   = 0x10  (CDB byte1, bit 4)
     *   COLORIMETRY_INFO_BT2020CL   = 0x20  (CDB byte1, bit 5)
     *   COLORIMETRY_INFO_BT2020NCL  = 0x40  (CDB byte1, bit 6)
     *   COLORIMETRY_INFO_BT2020RGB  = 0x80  (CDB byte1, bit 7)
     *   COLORIMETRY_INFO_DCI_P3     = 0x100 (CDB byte2, bit 7 per CTA-861-H)
     */
    static uint32_t ParseColorimetryFromEdid(const std::vector<uint8_t>& edidVec)
    {
        // Need at least 132 bytes: 128-byte base + 4-byte extension header
        if (edidVec.size() < 132) return 0;

        // CTA-861 extension block must be at byte 128 (tag = 0x02)
        if (edidVec[128] != 0x02) return 0;

        // Byte 130: byte offset from start of extension block to first DTD
        uint8_t dtdOffset = edidVec[130];
        if (dtdOffset < 4) return 0;  // No data block collection

        // Data block collection spans bytes [128+4 .. 128+dtdOffset)
        const size_t dbcStart = 132;
        const size_t dbcEnd   = static_cast<size_t>(128) + dtdOffset;
        if (dbcEnd > edidVec.size()) return 0;

        uint32_t colorimetry_info = 0;
        size_t pos = dbcStart;
        while (pos < dbcEnd) {
            uint8_t hdr    = edidVec[pos];
            uint8_t tag    = (hdr >> 5) & 0x07;
            uint8_t length = hdr & 0x1F;

            // Safety: don't read past end of buffer or DBC
            if (pos + 1u + length > edidVec.size()) break;

            if (tag == 7u && length >= 2u) {
                // Extended Data Block — first byte is extended tag
                uint8_t extTag = edidVec[pos + 1];
                if (extTag == 5u && length >= 3u) {
                    // Colorimetry Data Block (CTA-861 Section 7.5.5)
                    // Byte 2 (=pos+2): colorimetry standards, bits 7:0
                    uint8_t cdb1 = edidVec[pos + 2];
                    colorimetry_info |= static_cast<uint32_t>(cdb1);  // bits 0x01..0x80
                    if (length >= 4u) {
                        // Byte 3 (=pos+3): metadata indicators
                        // CTA-861-H: bit 7 = DCI-P3 (0x100 in our bitmask)
                        uint8_t cdb2 = edidVec[pos + 3];
                        if (cdb2 & 0x80u) colorimetry_info |= 0x100u;
                    }
                }
            }

            pos += 1u + length;
        }

        return colorimetry_info;
    }

    /**
     * Fetches raw EDID bytes via COM-RPC IDeviceSettingsDisplay::GetDisplayEdidBytes.
     * Trims trailing zeros to get the actual EDID payload size.
     *
     * @param edidVec  output vector, resized to the actual payload length
     * @return Core::ERROR_NONE on success, Core::ERROR_UNAVAILABLE if handles invalid
     */
    uint32_t GetEdidBytes(std::vector<uint8_t>& edidVec) const
    {
        if (_displayHandle == INVALID_DS_HANDLE) {
            LOGERR("GetEdidBytes: display handle not available");
            return Core::ERROR_UNAVAILABLE;
        }
        auto* disp = AcquireSubInterface<Exchange::IDeviceSettingsDisplay>();
        if (disp == nullptr) {
            LOGERR("GetEdidBytes: IDeviceSettingsDisplay not available");
            return Core::ERROR_UNAVAILABLE;
        }

        static const uint16_t kEdidBufLen = 256;
        edidVec.assign(kEdidBufLen, 0);
        Core::hresult rc = disp->GetDisplayEdidBytes(_displayHandle, edidVec.data(), kEdidBufLen);
        disp->Release();

        if (rc == Core::ERROR_NONE) {
            // Trim trailing zeros to find the actual EDID size
            size_t actualLen = kEdidBufLen;
            while (actualLen > 0 && edidVec[actualLen - 1] == 0) {
                --actualLen;
            }
            if (actualLen == 0) {
                actualLen = kEdidBufLen;  // all-zero EDID — keep full buffer
            }
            edidVec.resize(actualLen);
        }
        return rc;
    }

    /** Dispatch a resolution change event to all registered INotification sinks. */
    void DispatchResolutionChange(IConnectionProperties::INotification::Source source)
    {
        Core::IWorkerPool::Instance().Submit(DispatchJob::Create(this, source));
    }

    void Dispatch(IConnectionProperties::INotification::Source source)
    {
        _adminLock.Lock();
        for (auto* obs : _observers) obs->Updated(source);
        _adminLock.Unlock();
    }

    mutable Core::CriticalSection                    _adminLock;
    std::list<IConnectionProperties::INotification*> _observers;
    VideoPortType                                    _defaultPortType;
    int32_t                                          _displayHandle;      ///< display handle for default video port (EDID access)
    Core::Sink<DSVideoPortNotification>              _DSVideoPortNotification;

public:
    static DisplayInfoImplementation* _instance;
};

DisplayInfoImplementation* DisplayInfoImplementation::_instance = nullptr;

SERVICE_REGISTRATION(DisplayInfoImplementation, 1, 0);

} // namespace Plugin
} // namespace WPEFramework
