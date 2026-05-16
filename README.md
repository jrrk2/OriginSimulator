# Celestron Origin Telescope Simulator

A comprehensive C++ Qt-based simulator for the Celestron Origin telescope system that implements both WebSocket and HTTP protocols for complete telescope control and image serving functionality.

## Features

- **Complete Protocol Implementation**: Supports both WebSocket control commands and HTTP image serving
- **Real-time Status Updates**: Broadcasts telescope status, focuser, camera, and environmental data
- **Command Processing**: Handles telescope control commands including alignment, GOTO, tracking, and imaging
- **Physically calibrated image generation**: Gaia-catalog stars rendered with TAN projection, per-magnitude ADU response matched to a real Origin sensor (14 cm aperture, 1.477″/px, ~5000-ADU sky pedestal, ~50-ADU σ read noise, FWHM ≈ 6 px); separate sub-image caches for LIVE preview vs STACKED_MASTER so the App's star detector sees clean star fields during alignment
- **Mount-error simulation**: ±0.5″ periodic error at the worm period (~260 s), σ ≈ 0.1″ per-frame walking-noise drift, alt-az field rotation around the parallactic angle, and per-exposure arc trails (cap at 100 sub-samples) — all derived from measurements of a real 393-frame imaging session
- **Sky model**: configurable Bortle class (1–9) with corresponding sky-photon contribution and shot noise, optional Rayleigh airmass brightening, simple Meeus-formula Moon position with phase-weighted Gaussian halo
- **Stellarium DSO overlay**: when launched with `--dso=<name>` the simulator parks at the requested Stellarium tile's centre and paints the corresponding PNG (with gamma-suppressed background and progressive build-up scaled by stack depth) into STACKED_MASTER / SNAPSHOT frames only, so the App's alignment-time star detector isn't confused by nebula pixels
- **Per-frame independent noise**: each frame's read-noise field is drawn from a fresh thread-local Mersenne-Twister RNG with independent samples per channel, so stacking N subs improves sky SNR as √N (just like a real camera)
- **Network Discovery**: Broadcasts UDP discovery messages for automatic telescope detection
- **WebSocket Ping/Pong**: Proper WebSocket heartbeat implementation with timeout handling

## Architecture

The simulator is organized into several key components:

### Core Classes

- **`TelescopeState`**: Maintains all telescope state data (mount, camera, focuser, environment, etc.)
- **`CelestronOriginSimulator`**: Main simulator class handling network protocols and coordination
- **`WebSocketConnection`**: Manages individual WebSocket connections with proper frame handling
- **`CommandHandler`**: Processes telescope control commands and updates state
- **`StatusSender`**: Manages status broadcasts to connected clients

### Key Features

- **Dual Protocol Support**: WebSocket for control, HTTP for image serving
- **Proper WebSocket Implementation**: Complete frame parsing, ping/pong, close handling
- **Command Processing**: Supports mount control, focuser operations, camera settings, and more
- **Status Broadcasting**: Regular status updates to all connected clients
- **Image Generation**: Creates realistic dummy telescope images with star fields

## Supported Commands

### Mount Control
- `RunInitialize` - Initialize telescope with location/time
- `StartAlignment` / `AddAlignmentPoint` / `FinishAlignment` - Alignment sequence
- `GotoRaDec` - Slew to coordinates
- `StartTracking` / `StopTracking` - Tracking control
- `AbortAxisMovement` - Emergency stop

### Camera Control
- `GetCaptureParameters` / `SetCaptureParameters` - Camera settings
- `RunImaging` / `CancelImaging` - Imaging operations
- `GetFilter` - Filter wheel status

### Focuser Control
- `MoveToPosition` - Move focuser to position
- `GetStatus` - Get focuser status
- `SetBacklash` - Set backlash compensation

### System Commands
- `GetStatus` - Get component status (Mount, Camera, Focuser, etc.)
- `GetVersion` - Get firmware version
- `GetModel` - Get device model and capabilities

## Network Protocols

### WebSocket (Port 80)
- **Endpoint**: `ws://localhost/SmartScope-1.0/mountControlEndpoint`
- **Purpose**: Real-time telescope control and status
- **Features**: Command/response, status notifications, ping/pong heartbeat

### HTTP (Port 80)
- **Live Images**: `http://localhost/SmartScope-1.0/dev2/Images/Temp/`
- **Astrophotography**: `http://localhost/SmartScope-1.0/dev2/Images/Astrophotography/`
- **Purpose**: Serve telescope images and captures

### UDP Broadcast (Port 55555)
- **Purpose**: Network discovery
- **Message**: "Origin IP Address: [IP] Identity: Origin140020 Version: 1.1.4248"

## Building

### Requirements
- Qt 5.12 or later
- C++11 compatible compiler
- macOS (for Xcode project generation)

### Build Steps
```bash
qmake OriginSimulator.pro
make
```

### For Xcode Development
```bash
qmake -spec macx-xcode OriginSimulator.pro
open OriginSimulator.xcodeproj
```

## Usage

1. **Start the Simulator**:
   ```bash
   ./OriginSimulator [--dso=<filename-fragment>] [--dso-attenuation=<float>] [--bortle=<1..9>] [--rayleigh]
   ```

   Flags:
   - `--dso=<frag>` — restrict the Stellarium DSO overlay to one tile (filename substring match) and park the mount at that object's centre on startup. Sidesteps the Origin App's search-UI crash and the multi-DSO "Can't see stars" detector failure. Example: `--dso=m51-vasey`.
   - `--dso-attenuation=<float>` — multiply DSO overlay brightness (default 1.0). Use `<1` to dim, `>1` to brighten without rebuilding.
   - `--bortle=<1..9>` — sky-photon contribution to the pedestal (default 5 = suburban). Bortle 5 adds only a few ADU/px at 10 s exposure; meaningful change is at ≥7.
   - `--rayleigh` — enable airmass-driven sky brightening (off by default — it can drown the App's star detector at moderate altitudes).

2. **Connect via WebSocket**:
   ```javascript
   const ws = new WebSocket('ws://localhost/SmartScope-1.0/mountControlEndpoint');
   ```

3. **Send Commands**:
   ```json
   {
     "Command": "GetStatus",
     "Destination": "Mount",
     "SequenceID": 1,
     "Source": "TestClient",
     "Type": "Command"
   }
   ```

4. **View Images**:
   Open `http://localhost/SmartScope-1.0/dev2/Images/Temp/0.jpg` in browser

## File Structure

```
├── TelescopeState.h           # Telescope state data structure
├── CelestronOriginSimulator.h # Main simulator class
├── CelestronOriginSimulator.cpp
├── WebSocketConnection.h      # WebSocket protocol handler
├── WebSocketConnection.cpp
├── CommandHandler.h           # Command processing
├── CommandHandler.cpp
├── StatusSender.h            # Status broadcast manager
├── StatusSender.cpp
├── main.cpp                  # Application entry point
├── OriginSimulator.pro       # Qt project file
└── README.md                # This file
```

## Implementation Notes

### WebSocket Protocol
- Full RFC 6455 compliance for frame parsing
- Proper masking/unmasking for client frames
- Ping/pong heartbeat with 10-second timeout
- Graceful connection closure with status codes

### Command Processing
- Asynchronous command handling
- Proper error responses with codes/messages
- State validation for operations (e.g., alignment required for GOTO)
- Realistic timing simulation for slews and imaging

### Status Broadcasting
- Regular status updates every second
- Component-specific update intervals
- Efficient JSON message construction
- Selective client notification support

## Testing

The simulator has been tested with:
- WebSocket clients (browser, Node.js)
- HTTP image requests
- Multiple simultaneous connections
- Command sequences and error conditions
- Network discovery via UDP

## Future Enhancements

- Support for additional telescope commands
- More realistic imaging simulation
- Configuration file support
- Logging and debugging features
- Additional image formats and metadata