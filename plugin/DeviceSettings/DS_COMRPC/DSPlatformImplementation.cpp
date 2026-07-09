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
 * @file DSPlatformImplementation.cpp
 *
 * @brief DisplayInfo platform implementation using COM-RPC DeviceSettings plugin.
 *
 * This file is compiled when USE_DEVICESETTING_PLUGIN is defined.
 * It replaces DeviceSettings/PlatformImplementation.cpp (libds-based) for that
 * build configuration, providing identical IGraphicsProperties, IConnectionProperties,
 * IHDRProperties, and IDisplayProperties behaviour via the entservices-devicesettings
 * COM-RPC plugin rather than directly calling the DS shared library.
 *
 * Architecture:
 *   - Inherits DeviceSettingsClientHelper (single COM-RPC link on root IDeviceSettings).
 *   - Inherits Exchange::IConfiguration so the DisplayInfo proxy can pass IShell*
 *     via Configure(service) and trigger DeviceSettingsClientHelper::Open().
 *   - Caches video-port, display and video-device handles in OnDeviceSettingsActivated().
 *   - Inner notification delegate (DSVideoPortNotification) routes resolution
 *     pre/post change events to all registered IConnectionProperties::INotification sinks.
 *   - GPU RAM queries still go through SoC_abstraction (displayinfo-soc library).
 *   - EDID parsing uses edid-parser.hpp (still available in the displayinfo sysroot).
 */

#ifndef USE_DEVICESETTING_PLUGIN
#error "DSPlatformImplementation.cpp must only be compiled with USE_DEVICESETTING_PLUGIN defined"
#endif

#include "../../Module.h"
#include "../../DisplayInfoTracing.h"
#include "../SoC_abstraction.h"

#include <interfaces/IDisplayInfo.h>
#include <interfaces/IConfiguration.h>
#include "DeviceSettingsClientHelper.h"              // DeviceSettingsClientHelper + config stores + VP/Audio/VideoDevice sub-interfaces
#include <interfaces/IDeviceSettingsDisplay.h>     // Exchange::IDeviceSettingsDisplay (GetDisplay, GetDisplayEdidBytes)
#include "edid-parser.hpp"                         // edid_parser::COLORIMETRY_INFO_* constants

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
    , public DeviceSettingsClientHelper
{
private:
    using HdrteratorImplementation          = RPC::IteratorType<Exchange::IHDRProperties::IHDRIterator>;
    using ColorimetryIteratorImplementation = RPC::IteratorType<Exchange::IDisplayProperties::IColorimetryIterator>;

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

public:
    DisplayInfoImplementation()
        : _adminLock()
        , _observers()
        , _defaultPortType(VideoPortType::DS_VIDEO_PORT_TYPE_HDMI)
        , _DSVideoPortNotification(*this)
    {
        DisplayInfoImplementation::_instance = this;
    }

    DisplayInfoImplementation(const DisplayInfoImplementation&)            = delete;
    DisplayInfoImplementation& operator=(const DisplayInfoImplementation&) = delete;

    ~DisplayInfoImplementation() override
    {
        // Unregister resolution change notifications before severing the COM-RPC link.
        auto* vp = AcquireSubInterface<Exchange::IDeviceSettingsVideoPort>();
        if (vp != nullptr) {
            vp->Unregister(&_DSVideoPortNotification);
            vp->Release();
        }
        DeviceSettingsClientHelper::Close();
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
        DeviceSettingsClientHelper::Open(service);
        return Core::ERROR_NONE;
    }

    // -------------------------------------------------------------------------
    // DeviceSettingsClientHelper lifecycle overrides
    // -------------------------------------------------------------------------

    /**
     * Called when the DeviceSettings plugin activates (or re-activates after a
     * crash/restart).  Loads the video-port config, caches port/display/device
     * handles, and subscribes to resolution change events.
     */
    void OnDeviceSettingsActivated() override
    {
        LOGINFO("DisplayInfo: DeviceSettings activated — loading config and caching handles");

        // ---- 1. Video port config (1-arg convenience wrapper — no raw pointer needed) ----
        if (!LoadVideoPortConfig(_vpConfigStore)) {
            LOGERR("OnDeviceSettingsActivated: failed to load video port config");
        }

        // ---- 2. Default port handle + resolution notifications ----
        auto* vp = AcquireSubInterface<Exchange::IDeviceSettingsVideoPort>();
        if (vp != nullptr) {
            VideoPortEntry defaultEntry;
            if (_vpConfigStore.ResolveByName(_vpConfigStore.GetDefaultVideoPortName(), defaultEntry)) {
                Core::hresult rc = vp->GetVideoPort(defaultEntry.type, defaultEntry.index, _videoPortHandles[_vpConfigStore.GetDefaultVideoPortName()]);
                if (rc != Core::ERROR_NONE) {
                    LOGERR("OnDeviceSettingsActivated: GetVideoPort failed: %u", rc);
                    _videoPortHandles[_vpConfigStore.GetDefaultVideoPortName()] = INVALID_DS_HANDLE;
                } else {
                    _defaultPortType = defaultEntry.type;
                    LOGINFO("Cached default video port handle: %d (port=%s, type=%d)",
                            getCachedVideoPortHandle(_vpConfigStore.GetDefaultVideoPortName()), _vpConfigStore.GetDefaultVideoPortName().c_str(),
                            static_cast<int>(_defaultPortType));
                }
            } else {
                LOGERR("OnDeviceSettingsActivated: failed to resolve default video port entry");
            }

            vp->Register(&_DSVideoPortNotification);
            vp->Release();
        } else {
            LOGERR("OnDeviceSettingsActivated: IDeviceSettingsVideoPort not available");
        }

        // ---- 3. Display handle for the default port ----
        if (getCachedVideoPortHandle(_vpConfigStore.GetDefaultVideoPortName()) != INVALID_DS_HANDLE) {
            auto* disp = AcquireSubInterface<Exchange::IDeviceSettingsDisplay>();
            if (disp != nullptr) {
                VideoPortEntry defaultEntry;
                if (_vpConfigStore.ResolveByName(_vpConfigStore.GetDefaultVideoPortName(), defaultEntry)) {
                    Exchange::IDeviceSettingsDisplay::DisplayPortType dpType =
                        static_cast<Exchange::IDeviceSettingsDisplay::DisplayPortType>(defaultEntry.type);
                    Core::hresult rc = disp->GetDisplay(dpType, defaultEntry.index, _displayHandles[_vpConfigStore.GetDefaultVideoPortName()]);
                    if (rc != Core::ERROR_NONE) {
                        LOGERR("OnDeviceSettingsActivated: GetDisplay failed: %u", rc);
                        _displayHandles[_vpConfigStore.GetDefaultVideoPortName()] = INVALID_DS_HANDLE;
                    } else {
                        LOGINFO("Cached display handle: %d", getCachedDisplayHandle(_vpConfigStore.GetDefaultVideoPortName()));
                    }
                }
                disp->Release();
            } else {
                LOGERR("OnDeviceSettingsActivated: IDeviceSettingsDisplay not available");
            }
        }

        // ---- 4. Video device handle (index 0) ----
        auto* vd = AcquireSubInterface<Exchange::IDeviceSettingsVideoDevice>();
        if (vd != nullptr) {
            Core::hresult rc = vd->GetVideoDeviceHandle(0, _videoDeviceHandle);
            if (rc != Core::ERROR_NONE) {
                LOGERR("OnDeviceSettingsActivated: GetVideoDeviceHandle failed: %u", rc);
                _videoDeviceHandle = INVALID_DS_HANDLE;
            } else {
                LOGINFO("Cached video device handle: %d", _videoDeviceHandle);
            }
            vd->Release();
        } else {
            LOGERR("OnDeviceSettingsActivated: IDeviceSettingsVideoDevice not available");
        }
    }

    /**
     * Called when the DeviceSettings plugin deactivates.
     * The COM-RPC connection is already severed — must NOT call any interface methods here.
     */
    void OnDeviceSettingsDeactivated() override
    {
        LOGINFO("DisplayInfo: DeviceSettings deactivated — clearing cached handles");
        _vpConfigStore.Clear();
        _videoPortHandles.clear();
        _displayHandles.clear();
        _videoDeviceHandle = INVALID_DS_HANDLE;
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
        if (_vpConfigStore.IsEmpty()) {
            LOGERR("IsAudioPassthrough: config not available");
            return Core::ERROR_UNAVAILABLE;
        }

        int32_t connectedAudioType  = -1;
        int32_t connectedAudioIndex = -1;
        if (!_vpConfigStore.GetConnectedAudioPort(
                _vpConfigStore.GetDefaultVideoPortName(),
                connectedAudioType, connectedAudioIndex)) {
            LOGERR("IsAudioPassthrough: connected audio port not found in config");
            return Core::ERROR_NOT_EXIST;
        }

        auto* audio = AcquireSubInterfaceMutable<Exchange::IDeviceSettingsAudio>();
        if (audio == nullptr) {
            LOGERR("IsAudioPassthrough: IDeviceSettingsAudio not available");
            return Core::ERROR_UNAVAILABLE;
        }

        int32_t audioHandle = INVALID_DS_HANDLE;
        Exchange::IDeviceSettingsAudio::AudioPortType audioPortType =
            static_cast<Exchange::IDeviceSettingsAudio::AudioPortType>(connectedAudioType);
        Core::hresult rc = audio->GetAudioPort(audioPortType, connectedAudioIndex, audioHandle);
        if (rc == Core::ERROR_NONE) {
            Exchange::IDeviceSettingsAudio::StereoMode mode =
                Exchange::IDeviceSettingsAudio::AUDIO_STEREO_UNKNOWN;
            rc = audio->GetStereoMode(audioHandle, mode);
            if (rc == Core::ERROR_NONE) {
                value = (mode == Exchange::IDeviceSettingsAudio::AUDIO_STEREO_PASSTHROUGH);
            }
        }
        audio->Release();
        return rc;
    }

    Core::hresult Connected(bool& connected) const override
    {
        connected = false;
        if (getCachedVideoPortHandle(_vpConfigStore.GetDefaultVideoPortName()) == INVALID_DS_HANDLE) {
            LOGERR("Connected: video port handle not available");
            return Core::ERROR_UNAVAILABLE;
        }
        auto* vp = AcquireSubInterfaceMutable<Exchange::IDeviceSettingsVideoPort>();
        if (vp == nullptr) {
            LOGERR("Connected: IDeviceSettingsVideoPort not available");
            return Core::ERROR_UNAVAILABLE;
        }
        Core::hresult rc = vp->IsVideoPortDisplayConnected(getCachedVideoPortHandle(_vpConfigStore.GetDefaultVideoPortName()), connected);
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
        if (getCachedVideoPortHandle(_vpConfigStore.GetDefaultVideoPortName()) == INVALID_DS_HANDLE) {
            LOGERR("HDCPProtection(get): video port handle not available");
            return Core::ERROR_UNAVAILABLE;
        }
        auto* vp = AcquireSubInterfaceMutable<Exchange::IDeviceSettingsVideoPort>();
        if (vp == nullptr) {
            LOGERR("HDCPProtection(get): IDeviceSettingsVideoPort not available");
            return Core::ERROR_UNAVAILABLE;
        }
        Exchange::IDeviceSettingsVideoPort::HDCPProtocolVersion version =
            Exchange::IDeviceSettingsVideoPort::DS_HDCP_VERSION_MAX;
        Core::hresult rc = vp->GetHDMIPreference(getCachedVideoPortHandle(_vpConfigStore.GetDefaultVideoPortName()), version);
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
        if (getCachedVideoPortHandle(_vpConfigStore.GetDefaultVideoPortName()) == INVALID_DS_HANDLE) {
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
        Core::hresult rc = vp->SetHDMIPreference(getCachedVideoPortHandle(_vpConfigStore.GetDefaultVideoPortName()), version);
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
        if (_vpConfigStore.IsEmpty()) {
            LOGERR("PortName: video port config not available");
            return Core::ERROR_UNAVAILABLE;
        }

        std::vector<VideoPortEntry> entries;
        _vpConfigStore.BuildVideoPortEntries(entries);

        auto* vp = AcquireSubInterfaceMutable<Exchange::IDeviceSettingsVideoPort>();
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
            LOGERR("ColorSpace: display not accessible");
            return Core::ERROR_UNAVAILABLE;
        }

        auto* vp = AcquireSubInterfaceMutable<Exchange::IDeviceSettingsVideoPort>();
        if (vp == nullptr) {
            LOGERR("ColorSpace: IDeviceSettingsVideoPort not available");
            return Core::ERROR_UNAVAILABLE;
        }

        Exchange::IDeviceSettingsVideoPort::DisplayColorSpace _cs =
            Exchange::IDeviceSettingsVideoPort::DS_DISPLAY_COLORSPACE_UNKNOWN;
        Core::hresult rc = vp->GetColorSpace(getCachedVideoPortHandle(_vpConfigStore.GetDefaultVideoPortName()), _cs);
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
        if (getCachedVideoPortHandle(_vpConfigStore.GetDefaultVideoPortName()) == INVALID_DS_HANDLE) {
            LOGERR("FrameRate: video port handle not available");
            return Core::ERROR_UNAVAILABLE;
        }
        auto* vp = AcquireSubInterfaceMutable<Exchange::IDeviceSettingsVideoPort>();
        if (vp == nullptr) {
            LOGERR("FrameRate: IDeviceSettingsVideoPort not available");
            return Core::ERROR_UNAVAILABLE;
        }
        Exchange::IDeviceSettingsVideoPort::VideoPortResolution resolution;
        Core::hresult rc = vp->GetVideoPortResolution(getCachedVideoPortHandle(_vpConfigStore.GetDefaultVideoPortName()), resolution);
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
            LOGERR("ColourDepth: display not accessible");
            return Core::ERROR_UNAVAILABLE;
        }
        auto* vp = AcquireSubInterfaceMutable<Exchange::IDeviceSettingsVideoPort>();
        if (vp == nullptr) {
            LOGERR("ColourDepth: IDeviceSettingsVideoPort not available");
            return Core::ERROR_UNAVAILABLE;
        }

        uint32_t colorDepth = 0;
        Core::hresult rc = vp->GetColorDepth(getCachedVideoPortHandle(_vpConfigStore.GetDefaultVideoPortName()), colorDepth);
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
            LOGERR("QuantizationRange: display not accessible");
            return Core::ERROR_UNAVAILABLE;
        }
        auto* vp = AcquireSubInterfaceMutable<Exchange::IDeviceSettingsVideoPort>();
        if (vp == nullptr) {
            LOGERR("QuantizationRange: IDeviceSettingsVideoPort not available");
            return Core::ERROR_UNAVAILABLE;
        }

        Exchange::IDeviceSettingsVideoPort::DisplayQuantizationRange _qr =
            Exchange::IDeviceSettingsVideoPort::DS_DISPLAY_QUANTIZATIONRANGE_UNKNOWN;
        Core::hresult rc = vp->GetQuantizationRange(getCachedVideoPortHandle(_vpConfigStore.GetDefaultVideoPortName()), _qr);
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
            if (colorimetry_info & edid_parser::COLORIMETRY_INFO_XVYCC601)    colorimetryCaps.push_back(COLORIMETRY_XVYCC601);
            if (colorimetry_info & edid_parser::COLORIMETRY_INFO_XVYCC709)    colorimetryCaps.push_back(COLORIMETRY_XVYCC709);
            if (colorimetry_info & edid_parser::COLORIMETRY_INFO_SYCC601)     colorimetryCaps.push_back(COLORIMETRY_SYCC601);
            if (colorimetry_info & edid_parser::COLORIMETRY_INFO_ADOBEYCC601) colorimetryCaps.push_back(COLORIMETRY_OPYCC601);
            if (colorimetry_info & edid_parser::COLORIMETRY_INFO_ADOBERGB)    colorimetryCaps.push_back(COLORIMETRY_OPRGB);
            if ((colorimetry_info & edid_parser::COLORIMETRY_INFO_BT2020CL) ||
                (colorimetry_info & edid_parser::COLORIMETRY_INFO_BT2020NCL)) colorimetryCaps.push_back(COLORIMETRY_BT2020YCCBCBRC);
            if (colorimetry_info & edid_parser::COLORIMETRY_INFO_BT2020RGB)   colorimetryCaps.push_back(COLORIMETRY_BT2020RGB_YCBCR);
            if (colorimetry_info & edid_parser::COLORIMETRY_INFO_DCI_P3)      colorimetryCaps.push_back(COLORIMETRY_OTHER);
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
        auto* vp = AcquireSubInterfaceMutable<Exchange::IDeviceSettingsVideoPort>();
        if (vp == nullptr) {
            LOGERR("EOTF: IDeviceSettingsVideoPort not available");
            return Core::ERROR_UNAVAILABLE;
        }

        Exchange::IDeviceSettingsVideoPort::HDRStandard hdrStandard =
            Exchange::IDeviceSettingsVideoPort::DS_HDRSTANDARD_NONE;
        Core::hresult rc = vp->GetVideoEOTF(getCachedVideoPortHandle(_vpConfigStore.GetDefaultVideoPortName()), hdrStandard);
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
        std::list<Exchange::IHDRProperties::HDRType> hdrCapabilities;
        int32_t capabilities = 0;

        if (IsDisplayAccessible()) {
            auto* vp = AcquireSubInterfaceMutable<Exchange::IDeviceSettingsVideoPort>();
            if (vp != nullptr) {
                vp->GetTVHDRCapabilities(getCachedVideoPortHandle(_vpConfigStore.GetDefaultVideoPortName()), capabilities);
                vp->Release();
            }
        }

        if (!capabilities) hdrCapabilities.push_back(HDR_OFF);
        if (capabilities & static_cast<int32_t>(Exchange::IDeviceSettingsVideoPort::DS_HDRSTANDARD_HDR10))
            hdrCapabilities.push_back(HDR_10);
        if (capabilities & static_cast<int32_t>(Exchange::IDeviceSettingsVideoPort::DS_HDRSTANDARD_HDR10PLUS))
            hdrCapabilities.push_back(HDR_10PLUS);
        if (capabilities & static_cast<int32_t>(Exchange::IDeviceSettingsVideoPort::DS_HDRSTANDARD_HLG))
            hdrCapabilities.push_back(HDR_HLG);
        if (capabilities & static_cast<int32_t>(Exchange::IDeviceSettingsVideoPort::DS_HDRSTANDARD_DOLBYVISION))
            hdrCapabilities.push_back(HDR_DOLBYVISION);
        if (capabilities & static_cast<int32_t>(Exchange::IDeviceSettingsVideoPort::DS_HDRSTANDARD_TECHNICOLORPRIME))
            hdrCapabilities.push_back(HDR_TECHNICOLOR);
        if (capabilities & static_cast<int32_t>(Exchange::IDeviceSettingsVideoPort::DS_HDRSTANDARD_INVALID))
            hdrCapabilities.push_back(HDR_OFF);
        if (capabilities & static_cast<int32_t>(Exchange::IDeviceSettingsVideoPort::DS_HDRSTANDARD_SDR))
            hdrCapabilities.push_back(HDR_SDR);

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
        std::list<Exchange::IHDRProperties::HDRType> hdrCapabilities;
        int32_t capabilities = 0;

        if (_videoDeviceHandle != INVALID_DS_HANDLE) {
            auto* vd = AcquireSubInterfaceMutable<Exchange::IDeviceSettingsVideoDevice>();
            if (vd != nullptr) {
                vd->GetHDRCapabilities(_videoDeviceHandle, capabilities);
                vd->Release();
            }
        }

        if (!capabilities) hdrCapabilities.push_back(HDR_OFF);
        if (capabilities & static_cast<int32_t>(Exchange::IDeviceSettingsVideoPort::DS_HDRSTANDARD_HDR10))
            hdrCapabilities.push_back(HDR_10);
        if (capabilities & static_cast<int32_t>(Exchange::IDeviceSettingsVideoPort::DS_HDRSTANDARD_HDR10PLUS))
            hdrCapabilities.push_back(HDR_10PLUS);
        if (capabilities & static_cast<int32_t>(Exchange::IDeviceSettingsVideoPort::DS_HDRSTANDARD_HLG))
            hdrCapabilities.push_back(HDR_HLG);
        if (capabilities & static_cast<int32_t>(Exchange::IDeviceSettingsVideoPort::DS_HDRSTANDARD_DOLBYVISION))
            hdrCapabilities.push_back(HDR_DOLBYVISION);
        if (capabilities & static_cast<int32_t>(Exchange::IDeviceSettingsVideoPort::DS_HDRSTANDARD_TECHNICOLORPRIME))
            hdrCapabilities.push_back(HDR_TECHNICOLOR);
        if (capabilities & static_cast<int32_t>(Exchange::IDeviceSettingsVideoPort::DS_HDRSTANDARD_INVALID))
            hdrCapabilities.push_back(HDR_OFF);

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
            auto* vp = AcquireSubInterfaceMutable<Exchange::IDeviceSettingsVideoPort>();
            if (vp != nullptr) {
                vp->IsVideoPortOutputHDR(getCachedVideoPortHandle(_vpConfigStore.GetDefaultVideoPortName()), isHdr);
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
     * Const wrapper so const methods can call non-const AcquireSubInterface<T>().
     * The const_cast is safe because AcquireSubInterface() only reads from the
     * COM-RPC proxy state; it does not modify any observable member variables.
     */
    template<typename T>
    T* AcquireSubInterfaceMutable() const
    {
        return const_cast<DisplayInfoImplementation*>(this)->AcquireSubInterface<T>();
    }

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
        if (getCachedVideoPortHandle(_vpConfigStore.GetDefaultVideoPortName()) == INVALID_DS_HANDLE) {
            LOGERR("IsDisplayAccessible: video port handle not available (%d)", getCachedVideoPortHandle(_vpConfigStore.GetDefaultVideoPortName()));
            return false;
        }
        LOGINFO("IsDisplayAccessible: portHandle=%d portType=%d",
                getCachedVideoPortHandle(_vpConfigStore.GetDefaultVideoPortName()), static_cast<int>(_defaultPortType));
        if (_defaultPortType == VideoPortType::DS_VIDEO_PORT_TYPE_INTERNAL) {
            return true;
        }
        bool connected = false;
        auto* vp = AcquireSubInterfaceMutable<Exchange::IDeviceSettingsVideoPort>();
        if (vp == nullptr) {
            LOGERR("IsDisplayAccessible: IDeviceSettingsVideoPort not available");
            return false;
        }
        Core::hresult rc = vp->IsVideoPortDisplayConnected(getCachedVideoPortHandle(_vpConfigStore.GetDefaultVideoPortName()), connected);
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
        if (getCachedDisplayHandle(_vpConfigStore.GetDefaultVideoPortName()) == INVALID_DS_HANDLE) {
            LOGERR("GetEdidBytes: display handle not available");
            return Core::ERROR_UNAVAILABLE;
        }
        auto* disp = AcquireSubInterfaceMutable<Exchange::IDeviceSettingsDisplay>();
        if (disp == nullptr) {
            LOGERR("GetEdidBytes: IDeviceSettingsDisplay not available");
            return Core::ERROR_UNAVAILABLE;
        }

        static const uint16_t kEdidBufLen = 256;
        edidVec.assign(kEdidBufLen, 0);
        Core::hresult rc = disp->GetDisplayEdidBytes(getCachedDisplayHandle(_vpConfigStore.GetDefaultVideoPortName()), edidVec.data(), kEdidBufLen);
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
        _adminLock.Lock();
        for (auto* obs : _observers) {
            obs->Updated(source);
        }
        _adminLock.Unlock();
    }

    mutable Core::CriticalSection                    _adminLock;
    std::list<IConnectionProperties::INotification*> _observers;
    VideoPortType                                    _defaultPortType;
    Core::Sink<DSVideoPortNotification>              _DSVideoPortNotification;

public:
    static DisplayInfoImplementation* _instance;
};

DisplayInfoImplementation* DisplayInfoImplementation::_instance = nullptr;

SERVICE_REGISTRATION(DisplayInfoImplementation, 1, 0);

} // namespace Plugin
} // namespace WPEFramework
