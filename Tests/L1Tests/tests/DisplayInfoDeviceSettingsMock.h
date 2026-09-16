#pragma once

#include <gmock/gmock.h>

#include <interfaces/IDeviceSettings.h>
#include <interfaces/IDeviceSettingsAudio.h>
#include <interfaces/IDeviceSettingsDisplay.h>
#include <interfaces/IDeviceSettingsVideoDevice.h>
#include <interfaces/IDeviceSettingsVideoPort.h>

namespace DisplayInfoTestMocks {

using namespace WPEFramework;
using namespace WPEFramework::Exchange;

template <typename Interface>
class RefCountedMock : public Interface {
public:
    uint32_t AddRef() const override { return ++_references; }
    uint32_t Release() const override
    {
        const uint32_t references = --_references;
        return references;
    }

    void* QueryInterface(const uint32_t interfaceId) override
    {
        if (interfaceId == Core::IUnknown::ID || interfaceId == Interface::ID) {
            AddRef();
            return static_cast<Interface*>(this);
        }
        return nullptr;
    }

public:
    ~RefCountedMock() override = default;

private:
    mutable uint32_t _references = 1;
};

class VideoPortMock : public RefCountedMock<IDeviceSettingsVideoPort> {
public:
    MOCK_METHOD(Core::hresult, Register, (const string, INotification*), (override));
    MOCK_METHOD(Core::hresult, Unregister, (INotification*), (override));
    MOCK_METHOD(Core::hresult, GetVideoPort, (VideoPort, int32_t, int32_t&), (override));
    MOCK_METHOD(Core::hresult, GetVideoPortResolutionConfig, (VideoPort, IVideoPortResolutionIterator*&), (const, override));
    MOCK_METHOD(Core::hresult, IsVideoPortEnabled, (int32_t, bool&), (override));
    MOCK_METHOD(Core::hresult, IsVideoPortDisplayConnected, (int32_t, bool&), (override));
    MOCK_METHOD(Core::hresult, IsVideoPortDisplaySurround, (int32_t, bool&), (override));
    MOCK_METHOD(Core::hresult, GetVideoPortDisplaySurroundMode, (int32_t, VideoPortSurroundMode&), (override));
    MOCK_METHOD(Core::hresult, EnableVideoPort, (int32_t, bool), (override));
    MOCK_METHOD(Core::hresult, GetVideoPortResolution, (int32_t, VideoPortResolution&), (override));
    MOCK_METHOD(Core::hresult, SetVideoPortResolution, (int32_t, const VideoPortResolution&, bool, bool), (override));
    MOCK_METHOD(Core::hresult, EnableHDCPOnVideoPort, (int32_t, bool, const uint8_t[], uint16_t), (override));
    MOCK_METHOD(Core::hresult, IsHDCPEnabledOnVideoPort, (int32_t, bool&), (override));
    MOCK_METHOD(Core::hresult, GetHDCPStatusOnVideoPort, (int32_t, HDCPStatus&), (override));
    MOCK_METHOD(Core::hresult, GetHDCPProtocolVersionOnVideoPort, (int32_t, HDCPProtocolVersion&), (override));
    MOCK_METHOD(Core::hresult, GetHDCPReceiverProtocolVersionOnVideoPort, (int32_t, HDCPProtocolVersion&), (override));
    MOCK_METHOD(Core::hresult, GetHDCPCurrentProtocolVersionOnVideoPort, (int32_t, HDCPProtocolVersion&), (override));
    MOCK_METHOD(Core::hresult, IsVideoPortActive, (int32_t, bool&), (override));
    MOCK_METHOD(Core::hresult, GetTVHDRCapabilities, (int32_t, int32_t&), (override));
    MOCK_METHOD(Core::hresult, GetTVSupportedResolutions, (int32_t, int32_t&), (override));
    MOCK_METHOD(Core::hresult, SetForceDisable4K, (int32_t, bool), (override));
    MOCK_METHOD(Core::hresult, GetForceDisable4K, (int32_t, bool&), (override));
    MOCK_METHOD(Core::hresult, IsVideoPortOutputHDR, (int32_t, bool&), (override));
    MOCK_METHOD(Core::hresult, ResetVideoPortOutputToSDR, (), (override));
    MOCK_METHOD(Core::hresult, GetHDMIPreference, (int32_t, HDCPProtocolVersion&), (override));
    MOCK_METHOD(Core::hresult, SetHDMIPreference, (int32_t, HDCPProtocolVersion), (override));
    MOCK_METHOD(Core::hresult, GetVideoEOTF, (int32_t, HDRStandard&), (override));
    MOCK_METHOD(Core::hresult, GetMatrixCoefficients, (int32_t, DisplayMatrixCoefficients&), (override));
    MOCK_METHOD(Core::hresult, GetColorDepth, (int32_t, uint32_t&), (override));
    MOCK_METHOD(Core::hresult, GetColorSpace, (int32_t, DisplayColorSpace&), (override));
    MOCK_METHOD(Core::hresult, GetQuantizationRange, (int32_t, DisplayQuantizationRange&), (override));
    MOCK_METHOD(Core::hresult, GetCurrentOutputSettings, (int32_t, DSOutputSettings&), (override));
    MOCK_METHOD(Core::hresult, SetBackgroundColor, (int32_t, VideoBackgroundColor), (override));
    MOCK_METHOD(Core::hresult, SetForceHDRMode, (int32_t, HDRStandard), (override));
    MOCK_METHOD(Core::hresult, GetColorDepthCapabilities, (int32_t, uint32_t&), (override));
    MOCK_METHOD(Core::hresult, GetPreferredColorDepth, (int32_t, DisplayColorDepth&, bool), (override));
    MOCK_METHOD(Core::hresult, SetPreferredColorDepth, (int32_t, DisplayColorDepth, bool), (override));
};

class DisplayMock : public RefCountedMock<IDeviceSettingsDisplay> {
public:
    MOCK_METHOD(Core::hresult, Register, (const string, INotification*), (override));
    MOCK_METHOD(Core::hresult, Unregister, (INotification*), (override));
    MOCK_METHOD(Core::hresult, Register, (const string, IDisplayHDMIHotPlugNotification*), (override));
    MOCK_METHOD(Core::hresult, Unregister, (IDisplayHDMIHotPlugNotification*), (override));
    MOCK_METHOD(Core::hresult, GetDisplayEdid, (int32_t, DisplayEDID&, IDSVideoPortResolutionIterator*&), (override));
    MOCK_METHOD(Core::hresult, GetDisplayEdidBytes, (int32_t, uint8_t[], uint16_t), (override));
    MOCK_METHOD(Core::hresult, GetDisplay, (DisplayPortType, int32_t, int32_t&), (override));
    MOCK_METHOD(Core::hresult, GetDisplayAspectRatio, (int32_t, DisplayVideoAspectRatio&), (override));
    MOCK_METHOD(Core::hresult, SetAllmEnabled, (int32_t, bool), (override));
    MOCK_METHOD(Core::hresult, SetAVIContentType, (int32_t, DisplayAVIContentType), (override));
    MOCK_METHOD(Core::hresult, SetAVIScanInformation, (int32_t, DisplayAVIScanInformation), (override));
};

class VideoDeviceMock : public RefCountedMock<IDeviceSettingsVideoDevice> {
public:
    MOCK_METHOD(Core::hresult, Register, (const string, INotification*), (override));
    MOCK_METHOD(Core::hresult, Unregister, (INotification*), (override));
    MOCK_METHOD(Core::hresult, GetVideoDeviceHandle, (int32_t, int32_t&), (override));
    MOCK_METHOD(Core::hresult, SetVideoDeviceDFC, (int32_t, VideoZoom), (override));
    MOCK_METHOD(Core::hresult, GetVideoDeviceDFC, (int32_t, VideoZoom&), (override));
    MOCK_METHOD(Core::hresult, GetHDRCapabilities, (int32_t, int32_t&), (override));
    MOCK_METHOD(Core::hresult, GetSupportedVideoCodingFormats, (int32_t, int32_t&), (override));
    MOCK_METHOD(Core::hresult, GetCodecInfo, (int32_t, VideoCodec, IDeviceSettingsVideoCodecProfileSupportIterator*&), (override));
    MOCK_METHOD(Core::hresult, DisableHDR, (int32_t, bool), (override));
    MOCK_METHOD(Core::hresult, SetFRFMode, (int32_t, int32_t), (override));
    MOCK_METHOD(Core::hresult, GetFRFMode, (int32_t, int32_t&), (override));
    MOCK_METHOD(Core::hresult, GetCurrentDisplayFrameRate, (int32_t, string&), (override));
    MOCK_METHOD(Core::hresult, SetDisplayFrameRate, (int32_t, const string&), (override));
};

class AudioMock : public RefCountedMock<IDeviceSettingsAudio> {
public:
    MOCK_METHOD(Core::hresult, Register, (const string, INotification*), (override));
    MOCK_METHOD(Core::hresult, Unregister, (INotification*), (override));
    MOCK_METHOD(Core::hresult, GetAudioPort, (AudioPortType, int32_t, int32_t&), (override));
    MOCK_METHOD(Core::hresult, IsAudioPortEnabled, (int32_t, bool&), (override));
    MOCK_METHOD(Core::hresult, EnableAudioPort, (int32_t, bool), (override));
    MOCK_METHOD(Core::hresult, GetSupportedARCTypes, (int32_t, int32_t&), (override));
    MOCK_METHOD(Core::hresult, SetSAD, (int32_t, const uint8_t[], uint8_t), (override));
    MOCK_METHOD(Core::hresult, EnableARC, (int32_t, AudioARCStatus), (override));
    MOCK_METHOD(Core::hresult, GetStereoMode, (int32_t, StereoMode&, bool), (override));
    MOCK_METHOD(Core::hresult, SetStereoMode, (int32_t, StereoMode, bool), (override));
    MOCK_METHOD(Core::hresult, GetStereoAuto, (int32_t, int32_t&), (override));
    MOCK_METHOD(Core::hresult, SetStereoAuto, (int32_t, int32_t, bool), (override));
    MOCK_METHOD(Core::hresult, SetAudioMute, (int32_t, bool), (override));
    MOCK_METHOD(Core::hresult, IsAudioMuted, (int32_t, bool&), (override));
    MOCK_METHOD(Core::hresult, SetAudioDucking, (int32_t, AudioDuckingType, AudioDuckingAction, uint8_t), (override));
    MOCK_METHOD(Core::hresult, SetAudioLevel, (int32_t, float), (override));
    MOCK_METHOD(Core::hresult, GetAudioLevel, (int32_t, float&), (override));
    MOCK_METHOD(Core::hresult, SetAudioGain, (int32_t, float), (override));
    MOCK_METHOD(Core::hresult, GetAudioGain, (int32_t, float&), (override));
    MOCK_METHOD(Core::hresult, GetAudioFormat, (int32_t, AudioFormat&), (override));
    MOCK_METHOD(Core::hresult, GetAudioEncoding, (int32_t, AudioEncoding&), (override));
    MOCK_METHOD(Core::hresult, GetAudioEnablePersist, (int32_t, bool&, string&), (override));
    MOCK_METHOD(Core::hresult, SetAudioEnablePersist, (int32_t, bool, const string&), (override));
    MOCK_METHOD(Core::hresult, IsAudioMSDecoded, (int32_t, bool&), (override));
    MOCK_METHOD(Core::hresult, IsAudioMS12Decoded, (int32_t, bool&), (override));
    MOCK_METHOD(Core::hresult, GetAudioLEConfig, (int32_t, bool&), (override));
    MOCK_METHOD(Core::hresult, EnableAudioLEConfig, (int32_t, bool), (override));
    MOCK_METHOD(Core::hresult, SetApplicationAudioConfig, (int32_t, const string&, bool), (override));
    MOCK_METHOD(Core::hresult, GetApplicationAudioConfig, (int32_t, const string&, bool&), (override));
    MOCK_METHOD(Core::hresult, GetApplicationAudioConfigList, (int32_t, IDeviceSettingsAudioApplicationConfigIterator*&), (const, override));
    MOCK_METHOD(Core::hresult, SetAudioDelay, (int32_t, uint32_t), (override));
    MOCK_METHOD(Core::hresult, GetAudioDelay, (int32_t, uint32_t&), (override));
    MOCK_METHOD(Core::hresult, SetAudioDelayOffset, (int32_t, uint32_t), (override));
    MOCK_METHOD(Core::hresult, GetAudioDelayOffset, (int32_t, uint32_t&), (override));
    MOCK_METHOD(Core::hresult, GetAudioSinkDeviceAtmosCapability, (int32_t, DolbyAtmosCapability&), (override));
    MOCK_METHOD(Core::hresult, SetAudioAtmosOutputMode, (int32_t, bool), (override));
    MOCK_METHOD(Core::hresult, SetAudioCompression, (int32_t, int32_t), (override));
    MOCK_METHOD(Core::hresult, GetAudioCompression, (int32_t, int32_t&), (override));
    MOCK_METHOD(Core::hresult, SetAudioDialogEnhancement, (int32_t, int32_t), (override));
    MOCK_METHOD(Core::hresult, GetAudioDialogEnhancement, (int32_t, int32_t&), (override));
    MOCK_METHOD(Core::hresult, SetAudioDolbyVolumeMode, (int32_t, bool), (override));
    MOCK_METHOD(Core::hresult, GetAudioDolbyVolumeMode, (int32_t, bool&), (override));
    MOCK_METHOD(Core::hresult, SetAudioIntelligentEqualizerMode, (int32_t, int32_t), (override));
    MOCK_METHOD(Core::hresult, GetAudioIntelligentEqualizerMode, (int32_t, int32_t&), (override));
    MOCK_METHOD(Core::hresult, SetAudioVolumeLeveller, (int32_t, VolumeLeveller), (override));
    MOCK_METHOD(Core::hresult, GetAudioVolumeLeveller, (int32_t, VolumeLeveller&), (override));
    MOCK_METHOD(Core::hresult, SetAudioBassEnhancer, (int32_t, int32_t), (override));
    MOCK_METHOD(Core::hresult, GetAudioBassEnhancer, (int32_t, int32_t&), (override));
    MOCK_METHOD(Core::hresult, EnableAudioSurroundDecoder, (int32_t, bool), (override));
    MOCK_METHOD(Core::hresult, IsAudioSurroundDecoderEnabled, (int32_t, bool&), (override));
    MOCK_METHOD(Core::hresult, SetAudioDRCMode, (int32_t, int32_t), (override));
    MOCK_METHOD(Core::hresult, GetAudioDRCMode, (int32_t, int32_t&), (override));
    MOCK_METHOD(Core::hresult, SetAudioSurroundVirtualizer, (int32_t, SurroundVirtualizer), (override));
    MOCK_METHOD(Core::hresult, GetAudioSurroundVirtualizer, (int32_t, SurroundVirtualizer&), (override));
    MOCK_METHOD(Core::hresult, SetAudioMISteering, (int32_t, bool), (override));
    MOCK_METHOD(Core::hresult, GetAudioMISteering, (int32_t, bool&), (override));
    MOCK_METHOD(Core::hresult, SetAudioGraphicEqualizerMode, (int32_t, int32_t), (override));
    MOCK_METHOD(Core::hresult, GetAudioGraphicEqualizerMode, (int32_t, int32_t&), (override));
    MOCK_METHOD(Core::hresult, GetAudioMS12ProfileList, (int32_t, IDeviceSettingsAudioMS12AudioProfileIterator*&), (const, override));
    MOCK_METHOD(Core::hresult, GetAudioMS12Profile, (int32_t, string&), (override));
    MOCK_METHOD(Core::hresult, SetAudioMS12Profile, (int32_t, const string&), (override));
    MOCK_METHOD(Core::hresult, SetAudioMixerLevels, (int32_t, AudioInput, int32_t), (override));
    MOCK_METHOD(Core::hresult, SetAssociatedAudioMixing, (int32_t, bool), (override));
    MOCK_METHOD(Core::hresult, GetAssociatedAudioMixing, (int32_t, bool&), (override));
    MOCK_METHOD(Core::hresult, SetAudioFaderControl, (int32_t, int32_t), (override));
    MOCK_METHOD(Core::hresult, GetAudioFaderControl, (int32_t, int32_t&), (override));
    MOCK_METHOD(Core::hresult, SetAudioPrimaryLanguage, (int32_t, const string&), (override));
    MOCK_METHOD(Core::hresult, GetAudioPrimaryLanguage, (int32_t, string&), (override));
    MOCK_METHOD(Core::hresult, SetAudioSecondaryLanguage, (int32_t, const string&), (override));
    MOCK_METHOD(Core::hresult, GetAudioSecondaryLanguage, (int32_t, string&), (override));
    MOCK_METHOD(Core::hresult, GetAudioCapabilities, (int32_t, int32_t&), (override));
    MOCK_METHOD(Core::hresult, GetAudioMS12Capabilities, (int32_t, int32_t&), (override));
    MOCK_METHOD(Core::hresult, SetAudioMS12SettingsOverride, (int32_t, const string&, const string&, const string&, MS12ProfileState), (override));
    MOCK_METHOD(Core::hresult, IsAudioOutputConnected, (int32_t, bool&), (override));
    MOCK_METHOD(Core::hresult, ResetAudioDialogEnhancement, (int32_t), (override));
    MOCK_METHOD(Core::hresult, ResetAudioBassEnhancer, (int32_t), (override));
    MOCK_METHOD(Core::hresult, ResetAudioSurroundVirtualizer, (int32_t), (override));
    MOCK_METHOD(Core::hresult, ResetAudioVolumeLeveller, (int32_t), (override));
    MOCK_METHOD(Core::hresult, GetAudioHDMIARCPortId, (int32_t, int32_t&), (override));
};

class RootMock : public RefCountedMock<IDeviceSettings> {
public:
    RootMock(VideoPortMock& videoPort, DisplayMock& display, AudioMock& audio, VideoDeviceMock& videoDevice)
        : _videoPort(videoPort), _display(display), _audio(audio), _videoDevice(videoDevice)
    {
    }

    MOCK_METHOD(Core::hresult, Configure, (PluginHost::IShell*), (override));
    MOCK_METHOD(Core::hresult, GetDeviceSettingConfigs, (DeviceSettingConfigs&), (override));

    void* QueryInterface(const uint32_t interfaceId) override
    {
        if (interfaceId == IDeviceSettingsVideoPort::ID) {
            _videoPort.AddRef();
            return static_cast<IDeviceSettingsVideoPort*>(&_videoPort);
        }
        if (interfaceId == IDeviceSettingsDisplay::ID) {
            _display.AddRef();
            return static_cast<IDeviceSettingsDisplay*>(&_display);
        }
        if (interfaceId == IDeviceSettingsAudio::ID) {
            _audio.AddRef();
            return static_cast<IDeviceSettingsAudio*>(&_audio);
        }
        if (interfaceId == IDeviceSettingsVideoDevice::ID) {
            _videoDevice.AddRef();
            return static_cast<IDeviceSettingsVideoDevice*>(&_videoDevice);
        }
        return RefCountedMock<IDeviceSettings>::QueryInterface(interfaceId);
    }

private:
    VideoPortMock& _videoPort;
    DisplayMock& _display;
    AudioMock& _audio;
    VideoDeviceMock& _videoDevice;
};

} // namespace DisplayInfoTestMocks