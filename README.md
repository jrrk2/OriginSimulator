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

## Raw FITS snapshots (Origin firmware 1.5.x)

When a client sets `BitDepth` 16 and runs `RunSampleCapture` with `Debayer` false (or absent, as on current firmware), the simulator serves a raw 16-bit Bayer FITS
instead of a TIFF: 3056 x 2048, `BITPIX` 16 / `BZERO` 32768, black level about 4985 ADU, GRBG layout, `ImageType` `SAMPLE_CAPTURE`, file `/tmp/Images_N.fits`
(`FitsImageGenerator`). The star field depends on the pointing (same stars for the same target, shifted by the real offset, plus a small dither), star size depends on the
focuser position (best focus at 18900, so a focus sweep shows a V-curve), and brightness follows exposure x ISO. Set `ORIGIN_SIM_CLOUD=0..1` to dim the stars.
`MoveTo` (with `Value`) on the Focuser is accepted as well as `MoveToPosition`. The simulator listens on port 80 (`SERVER_PORT`); on Linux use a high port and tell the
client, e.g. `OriginHeadless serve --connect 127.0.0.1 --telescope-port 8080`.

## The mount as a machine (for host-side calibration and cord-wrap tests)

`MountModel` simulates the mount mechanically, so that a client can be tested on what a real Origin on uneven ground does, not on an ideal mount:

- **Encoders and a tilted base.** `Enc0`/`Enc1` (radians, azimuth wrapped as the firmware reports it) are always in the mount status. The true pointing comes
  from them through the altitude encoder's zero, tube flexure (F cos alt), a cone error (optical axis off square to the altitude axis), periodic error in each
  axis's worm (with a second harmonic), the azimuth encoder's zero and the base's tilt (a rotation).
- **A secret set-up.** Each start picks a level error up to 10 degrees toward any side, a random azimuth zero, and plausible altitude zero, flexure, cone error,
  periodic error and tilt-sensor offset. `ORIGIN_SIM_SEED` repeats one; `ORIGIN_SIM_SECRET=0` gives the mount measured on 2026-11-08 (5 degrees toward
  azimuth 18). `GET /sim/truth` reveals it, for scoring a calibration after it has finished; `GET /sim/state` shows only what someone at the telescope could see.
- **The firmware's frame.** After an initialisation the firmware reports `Alt`/`Azm` = encoders plus constant offsets (a level-base model) and `Ra`/`Dec` in J2000.
  A fake initialisation (`RunInitialize` with `FakeInitialize`) declares the current pointing to be azimuth 180 at the tilt sensor's altitude; a real one finds
  the true pointing, fails with -53 if the live stream is disabled, and refuses a second with -73. `GotoRaDec` and `TrackMovingTarget` answer -5 until the
  firmware is ready, turn the azimuth the short way round (as the firmware does), and track after arriving; `HaltTasks` stops them (`StopTracking` is unknown). `Slew` works at any time,
  at the measured speeds (rate 9 about 4 deg/s); zero rates leave a goto or track running.
- **Cables.** The running count of azimuth from cable-neutral (`ORIGIN_SIM_CABLE_NEUTRAL`, default the start). Past 180 degrees the log warns `CORD WRAP`, and
  `/sim/state` counts each time (`cord.times_taut`) and keeps the furthest each way. `GET /sim/slip?degrees=x` simulates a knock.
- **The sky in the frames.** Real stars from Gaia DR3 (see `data/README.md`), each refracted at its own altitude (Saemundsson, `ORIGIN_SIM_PRESSURE`,
  `ORIGIN_SIM_TEMPERATURE`), in the gnomonic projection onto the plane square to the tube, with the camera's orientation as the mount carries it. Each frame
  integrates the mount through the exposure, so a tracked field rotates about its centre (fastest near the zenith) and a still mount lets the stars trail; for
  30 s after a slew the stars are oval. The FITS header and `NewImageReady` carry the firmware's idea of the pointing, never the truth.

Other settings: `ORIGIN_SIM_PORT` (default 80), `ORIGIN_SIM_BROADCAST=0` (no discovery broadcast, when a real telescope is on the network),
`ORIGIN_SIM_START_READY=0` (start uninitialised), `ORIGIN_SIM_START="enc0,enc1"`, `ORIGIN_SIM_TILT="deg,toward"`, `ORIGIN_SIM_AZ_ZERO`, `ORIGIN_SIM_ALT_ZERO`,
`ORIGIN_SIM_FLEXURE`, `ORIGIN_SIM_CONE` (arcmin), `ORIGIN_SIM_PE="az,alt"` (arcsec), `ORIGIN_SIM_WORM_TEETH` (a placeholder: the Origin's is not known),
`ORIGIN_SIM_REFRACTION=0`, `ORIGIN_SIM_SETTLE` (seconds), `ORIGIN_SIM_INIT_TICK_MS`, `ORIGIN_SIM_INIT_FAIL` (percent), `ORIGIN_SIM_GAIA` (a catalogue file, a
Cartes du Ciel folder, or `none`).

PI.N.S.'s tests drive all of this end to end: `ORIGIN_SIM=127.0.0.1:18080 dotnet test --filter "OriginSimulatorCalibrationTest|OriginSimulatorDitherTest"`.
