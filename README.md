# Celestron Origin Telescope Simulator

A comprehensive C++ Qt-based simulator for the Celestron Origin telescope system that implements both WebSocket and HTTP protocols for complete telescope control and image serving functionality.

## Features

- **Complete Protocol Implementation**: Supports both WebSocket control commands and HTTP image serving
- **Real-time Status Updates**: Broadcasts telescope status, focuser, camera, and environmental data
- **Command Processing**: Handles telescope control commands including alignment, GOTO, tracking, and imaging
- **Image Simulation**: Serves simulated telescope images via HTTP
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
   ./OriginSimulator
   ```

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