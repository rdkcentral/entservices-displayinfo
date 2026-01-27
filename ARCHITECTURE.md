# DisplayInfo Plugin Architecture

## Overview

The DisplayInfo plugin is a Thunder/WPEFramework service that provides comprehensive display and graphics hardware information for RDK-based devices. It exposes display capabilities, connection properties, HDR support, and graphics properties through a standardized JSON-RPC API interface.

## System Architecture

### Component Structure

The DisplayInfo plugin follows a modular architecture with platform-specific abstractions:

```
┌─────────────────────────────────────────────────────────┐
│           Thunder Framework (WPEFramework)              │
│  ┌──────────────────────────────────────────────────┐   │
│  │         DisplayInfo Plugin Service               │   │
│  │  ┌────────────────────────────────────────────┐  │   │
│  │  │   JSON-RPC Interface Layer                 │  │   │
│  │  │  - GraphicsProperties API                  │  │   │
│  │  │  - ConnectionProperties API                │  │   │
│  │  │  - HDRProperties API                       │  │   │
│  │  │  - DisplayProperties API                   │  │   │
│  │  └────────────────────────────────────────────┘  │   │
│  │                      ▼                            │   │
│  │  ┌────────────────────────────────────────────┐  │   │
│  │  │   Core DisplayInfo Service                 │  │   │
│  │  │  - Plugin Lifecycle Management             │  │   │
│  │  │  - Event Notification System               │  │   │
│  │  │  - Interface Aggregation                   │  │   │
│  │  └────────────────────────────────────────────┘  │   │
│  │                      ▼                            │   │
│  │  ┌────────────────────────────────────────────┐  │   │
│  │  │   Platform Implementation Layer            │  │   │
│  │  │                                            │  │   │
│  │  │  ┌──────────┐  ┌──────────┐  ┌─────────┐ │  │   │
│  │  │  │DeviceSet │  │  Linux   │  │   RPI   │ │  │   │
│  │  │  │ tings    │  │   DRM    │  │  BCM    │ │  │   │
│  │  │  └──────────┘  └──────────┘  └─────────┘ │  │   │
│  │  └────────────────────────────────────────────┘  │   │
│  └──────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
                       ▼
     ┌──────────────────────────────────────┐
     │   Hardware Abstraction Layer (HAL)   │
     │  - Device Settings Library (DS)       │
     │  - IARM Bus Communication             │
     │  - DRM/KMS Interface                  │
     │  - Broadcom Host API                  │
     └──────────────────────────────────────┘
```

### Core Components

#### 1. DisplayInfo Service Core (`DisplayInfo.cpp/h`)

The main service class that implements:
- **IPlugin Interface**: Plugin lifecycle management (Initialize, Deinitialize)
- **IWeb Interface**: HTTP/REST endpoint handling
- **JSONRPC Interface**: JSON-RPC method dispatching
- **Interface Aggregation**: Exposes multiple capability interfaces through a single service

Key responsibilities:
- Service initialization and resource acquisition
- Interface composition and registration
- Connection property monitoring
- Lifecycle management of platform implementations

#### 2. JSON-RPC API Layer (`DisplayInfoJsonRpc.cpp`)

Provides standardized API endpoints:
- **displayinfo**: Returns comprehensive display information including resolution, HDR capabilities, and connection status
- **updated event**: Notifies clients of display connection changes

The JSON-RPC layer automatically marshals Thunder C++ interfaces into JSON-RPC methods.

#### 3. Platform Implementation Layer

Multiple platform-specific implementations provide hardware abstraction:

**DeviceSettings Implementation** (`DeviceSettings/PlatformImplementation.cpp`)
- Uses RDK Device Settings (DS) library
- Communicates via IARM Bus
- Supports Broadcom, Amlogic, and other RDK-compliant SoCs
- Provides display resolution, EDID parsing, HDR capabilities

**Linux/DRM Implementation** (`Linux/PlatformImplementation.cpp`)
- Direct kernel DRM/KMS interface
- Suitable for x86, ARM Linux platforms
- Connector detection and EDID reading
- Resolution and display mode enumeration

**RPI Implementation** (`RPI/PlatformImplementation.cpp`)
- Broadcom VideoCore API
- Raspberry Pi specific optimizations
- BCM_HOST library integration

#### 4. Interface Definitions

The plugin exposes four primary interfaces defined in the Thunder API:

- **IGraphicsProperties**: GPU information, rendering capabilities
- **IConnectionProperties**: Display connection state, HDCP status, resolution
- **IHDRProperties**: HDR format support (HDR10, HLG, Dolby Vision)
- **IDisplayProperties**: Extended display capabilities (color depth, color space)

## Data Flow

### Initialization Sequence

```
1. Thunder Framework → DisplayInfo::Initialize()
2. DisplayInfo → Root<IConnectionProperties>()
   - Out-of-process implementation instantiation
3. Platform Implementation → Hardware/HAL initialization
4. DisplayInfo → Interface Query (IGraphicsProperties, IHDRProperties, etc.)
5. DisplayInfo → Interface Registration with JSON-RPC layer
6. DisplayInfo → Notification registration for connection events
```

### Query Request Flow

```
Client → JSON-RPC Request → DisplayInfo Service → 
  Platform Implementation → Hardware/DS API → 
    Hardware Response → Platform Implementation → 
      DisplayInfo Service → JSON-RPC Response → Client
```

### Event Notification Flow

```
Hardware Event (cable plug/unplug) → 
  HAL/IARM Event → Platform Implementation → 
    IConnectionProperties::INotification::Updated() → 
      DisplayInfo Service → JSON-RPC Event Broadcast → 
        Subscribed Clients
```

## Plugin Framework Integration

### Thunder Plugin Lifecycle

1. **Registration**: Plugin metadata registered via `SERVICE_REGISTRATION` macro
2. **Activation**: Framework calls `Initialize()` with `IShell` service context
3. **Operation**: Handles JSON-RPC requests and publishes events
4. **Deactivation**: Framework calls `Deinitialize()` to clean up resources

### Out-of-Process Execution

The DisplayInfo plugin supports out-of-process execution for:
- **Isolation**: Crashes in platform code don't affect Thunder
- **Security**: Privilege separation
- **Resource Management**: Independent process lifecycle

Communication uses Thunder RPC with automatic proxy/stub generation.

## Dependencies

### Build-Time Dependencies

- **Thunder Framework** (WPEFramework): Core plugin infrastructure
- **Thunder Plugins**: Plugin base classes and utilities  
- **Thunder Definitions**: Interface definitions (IGraphicsProperties, etc.)
- **Thunder APIs**: JSON data structures

### Platform-Specific Dependencies

- **Device Settings (DS)**: RDK device settings library for display management
- **IARM Bus**: Inter-application communication for RDK
- **DRM/LibDRM**: Direct Rendering Manager for Linux graphics
- **BCM_HOST**: Broadcom VideoCore host interface (Raspberry Pi)
- **NEXUS/NXCLIENT**: Broadcom set-top-box platform API

### Runtime Dependencies

- Thunder Controller service (for plugin management)
- Platform HAL libraries (devicesettings, IARM daemon)
- Display hardware drivers (DRM modules, proprietary drivers)

## Configuration

The plugin uses Thunder's standard configuration mechanism:

- **DisplayInfo.json**: Plugin configuration file
- **Startup Mode**: Automatic, on-demand, or disabled
- **Startup Order**: Determines initialization sequence relative to other plugins
- **Out-of-Process**: Controls whether to run in separate process

## Security Considerations

- No authentication/authorization implemented (relies on Thunder security model)
- Read-only information exposure (no display control capabilities)
- Potential information leakage of hardware capabilities
- HDCP status exposure requires secure channel in production

## Performance Characteristics

- **Initialization Time**: 50-200ms depending on platform
- **Query Latency**: 10-50ms typical
- **Memory Footprint**: 2-5 MB (in-process), 5-10 MB (out-of-process)
- **Event Propagation**: <100ms from hardware event to client notification
- **CPU Usage**: Minimal (<1%) during steady state
