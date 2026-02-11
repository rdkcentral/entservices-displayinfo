# DisplayInfo Plugin - Product Overview

## Product Description

The DisplayInfo plugin is a Thunder/WPEFramework service that provides comprehensive, real-time information about display hardware capabilities, connection status, and graphics properties for RDK-based entertainment and media devices. It serves as the authoritative source for display-related information across the RDK software stack.

## Key Features

### Display Information Retrieval

- **Resolution Information**: Native and current display resolution, aspect ratio, refresh rates
- **Connection Status**: HDMI/DisplayPort connection state, active/inactive status
- **EDID Data**: Display manufacturer, model, year of manufacture, serial number
- **Display Capabilities**: Supported resolutions, color spaces, and video formats

### Graphics Properties

- **GPU Information**: Graphics processor type, vendor, capabilities
- **Rendering Properties**: OpenGL/Vulkan support, texture formats, shader capabilities
- **Memory Information**: Video memory size and availability
- **Acceleration Status**: Hardware acceleration capabilities for video decode/encode

### HDR Support Detection

- **HDR Format Detection**: HDR10, HDR10+, Dolby Vision, HLG support status
- **Dynamic Range Information**: Luminance levels, color gamut
- **EOTF Support**: Electro-Optical Transfer Function capabilities
- **Metadata Support**: Static and dynamic HDR metadata handling

### Connection Properties

- **HDCP Status**: HDCP version support and authentication state
- **Audio Capabilities**: Supported audio formats through HDMI/DisplayPort
- **Color Depth**: Supported bit depths (8-bit, 10-bit, 12-bit)
- **Chroma Subsampling**: YCbCr 4:4:4, 4:2:2, 4:2:0 support

### Real-Time Notifications

- **Connection Events**: Display connect/disconnect notifications
- **Resolution Changes**: Dynamic resolution change detection
- **HDCP State Changes**: Authentication status updates
- **Capability Updates**: Dynamic display capability change notifications

## Use Cases

### Content Streaming Applications

**Scenario**: Video streaming application needs to determine optimal content quality

The streaming app queries DisplayInfo to determine:
- Maximum supported resolution (4K, 8K)
- HDR format support (to serve HDR vs SDR content)
- HDCP version (for DRM content protection requirements)
- Refresh rate capabilities (for high frame rate content)

**Benefit**: Delivers best possible user experience by automatically selecting optimal video quality without user configuration.

### Smart TV User Interface

**Scenario**: TV UI needs to adapt layout and rendering based on display capabilities

The UI framework uses DisplayInfo to:
- Detect native resolution for pixel-perfect rendering
- Enable HDR tone mapping when supported
- Optimize GPU resource usage based on graphics capabilities
- Display accurate connection status indicators

**Benefit**: Provides responsive, hardware-optimized user interface with accurate status information.

### System Settings and Diagnostics

**Scenario**: Settings application displays detailed display information to users

The settings app leverages DisplayInfo to:
- Show connected display model and capabilities
- Display current resolution and refresh rate
- Indicate HDR/HDCP support status
- Provide troubleshooting information for support calls

**Benefit**: Empowers users with clear visibility into their display configuration and enables faster technical support resolution.

### Automated Testing and Quality Assurance

**Scenario**: Test automation framework validates display capabilities

Test scripts use DisplayInfo to:
- Verify correct EDID parsing across different displays
- Validate HDR capability detection
- Test connection event handling
- Ensure consistent API responses across platforms

**Benefit**: Enables comprehensive automated testing of display subsystem without manual verification.

### Multi-Display Management

**Scenario**: Commercial display systems manage multiple connected screens

Display management software uses DisplayInfo to:
- Enumerate all connected displays
- Query individual display capabilities
- Monitor connection status of each display
- Coordinate content distribution based on capabilities

**Benefit**: Enables sophisticated multi-display configurations with automatic adaptation to each screen's capabilities.

## API Capabilities

### JSON-RPC Methods

**Method: `DisplayInfo.1.displayinfo` (Property GET)**

Returns comprehensive display information structure:

```json
{
  "totalgpuram": "512M",
  "freegpuram": "128M",
  "audiopassthrough": true,
  "connected": true,
  "width": 3840,
  "height": 2160,
  "hdcp": "2.2",
  "hdrstandards": ["HDR10", "HLG"],
  "colorspace": "BT2020",
  "framerate": "60"
}
```

**Event: `DisplayInfo.1.updated`**

Notifies subscribers of connection or capability changes:

```json
{
  "event": "updated"
}
```

### Integration Patterns

**Polling Pattern**: Applications can periodically query display information
```javascript
// Query display info every 5 seconds
setInterval(() => {
  client.call("DisplayInfo.1.displayinfo")
}, 5000);
```

**Event-Driven Pattern**: Applications subscribe to real-time updates
```javascript
// Subscribe to display changes
client.on("DisplayInfo.1.updated", () => {
  // Re-query display info on changes
  client.call("DisplayInfo.1.displayinfo")
});
```

## Target Scenarios

### Set-Top Boxes (STB)

Primary deployment platform for cable, satellite, and IPTV services requiring:
- Comprehensive HDMI capability detection
- HDCP compliance validation
- HDR content delivery optimization
- Multi-format display support

### Smart TVs

Integrated entertainment platforms requiring:
- Native display property access
- Dynamic resolution adaptation
- Built-in display capability awareness
- Simplified configuration workflow

### Streaming Devices

HDMI dongle and streaming stick deployments needing:
- Automatic display detection
- Format negotiation with TV
- Minimal resource footprint
- Fast initialization time

### Hybrid Broadcast Broadband TV (HbbTV)

European broadcast-internet hybrid systems requiring:
- Standards-compliant display detection
- Broadcast/broadband format coordination
- Regional display standard support
- Comprehensive EDID interpretation

## Performance and Reliability

### Response Time

- **Initial Query**: < 50ms typical for cached values
- **Hardware Query**: < 200ms when querying hardware directly
- **Event Notification**: < 100ms from hardware event to client notification
- **Startup Time**: < 200ms plugin initialization on typical platforms

### Resource Utilization

- **Memory Footprint**: 2-5 MB in-process, 5-10 MB out-of-process
- **CPU Usage**: < 1% during normal operation, < 5% during initialization
- **Network Impact**: Zero (local-only communication)
- **Storage Requirements**: < 500KB plugin binary

### Reliability Characteristics

- **Failure Handling**: Graceful degradation on hardware errors
- **Error Recovery**: Automatic reconnection on transient failures
- **State Consistency**: Maintains accurate cached state with hardware synchronization
- **Process Isolation**: Out-of-process execution prevents Thunder framework crashes

### Scalability

- **Concurrent Clients**: Supports unlimited JSON-RPC clients
- **Event Subscribers**: Efficient broadcast to multiple subscribers
- **Query Rate**: Handles hundreds of queries per second
- **Multi-Display**: Supports multiple display outputs on capable hardware

## Platform Support

### RDK Platforms

- **RDK-V (Video)**: Full support with Device Settings integration
- **RDK-B (Broadband)**: Supported with appropriate HAL
- **Reference Platforms**: Validated on official RDK reference designs

### Hardware Architectures

- **Broadcom**: Native Device Settings support
- **Amlogic**: Device Settings integration
- **ARM Linux**: DRM/KMS implementation
- **x86 Linux**: DRM/KMS implementation
- **Raspberry Pi**: BCM_HOST native implementation

### Operating Systems

- **RDK Linux**: Primary deployment target
- **Yocto Linux**: Fully supported
- **Ubuntu**: Compatible via DRM implementation
- **Other Linux**: Support via standard DRM/KMS interfaces

## Integration Benefits

### For Application Developers

- **Single API**: Unified interface across all RDK platforms
- **Automatic Updates**: Real-time notifications eliminate polling
- **Rich Information**: Comprehensive display capabilities in single call
- **Easy Integration**: Standard JSON-RPC interface

### For Device Manufacturers

- **Platform Abstraction**: Hardware differences hidden behind consistent API
- **Reduced Development**: No need to implement custom display detection
- **Standards Compliance**: RDK-standard display information service
- **Quality Assurance**: Well-tested reference implementation

### For Service Providers

- **Customer Support**: Detailed display information for troubleshooting
- **Content Optimization**: Automatic quality adaptation based on capabilities
- **Feature Detection**: Enable/disable features based on display support
- **Diagnostics**: Comprehensive display status for remote support

## Future Roadmap

- Enhanced multi-display support for commercial deployments
- Extended color space and gamut information
- Variable refresh rate (VRR) detection and configuration
- Display calibration data exposure
- Advanced EDID extension parsing
- DisplayPort-specific capability detection
