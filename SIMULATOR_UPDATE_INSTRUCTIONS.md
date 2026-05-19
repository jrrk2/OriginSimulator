# Simulator Update Instructions

## Files Already Updated
✅ CommandHandler.h - Added handleSlew() declaration
✅ CommandHandler.cpp - Added handleSlew() implementation and case in processCommand()
✅ TelescopeState.h - Added slew state variables and updateSlewMotion() method

## Remaining Change: CelestronOriginSimulator.cpp

### Location
In the `updateSlew()` method (already exists in your code)

### Find this method:
```cpp
void CelestronOriginSimulator::updateSlew() {
    // Existing slew update code
}
```

### Add this code at the beginning of updateSlew():
```cpp
void CelestronOriginSimulator::updateSlew() {
    // NEW: Update slew motion if active
    if (m_telescopeState->isSlewing) {
        m_telescopeState->updateSlewMotion();
        
        // Optional: Log position changes for debugging
        if (m_telescopeState->slewAltRate != 0) {
            double altDeg = m_telescopeState->altitude * 180.0 / M_PI;
            qDebug() << QString("Slewing - Alt: %1° (rate: %2)")
                        .arg(altDeg, 0, 'f', 2)
                        .arg(m_telescopeState->slewAltRate);
        }
    }
    
    // ... rest of existing updateSlew() code ...
}
```

### Alternative: If updateSlew() doesn't exist
If the `updateSlew()` method doesn't exist yet, add this in `setupTimers()`:

```cpp
void CelestronOriginSimulator::setupTimers() {
    // ... existing timer setup code ...
    
    // Setup slew motion timer
    m_slewTimer = new QTimer(this);
    connect(m_slewTimer, &QTimer::timeout, this, &CelestronOriginSimulator::updateSlew);
    m_slewTimer->start(100);  // Update every 100ms for smooth motion
}
```

And add the updateSlew() method:
```cpp
void CelestronOriginSimulator::updateSlew() {
    if (m_telescopeState->isSlewing) {
        m_telescopeState->updateSlewMotion();
        
        // Optional: Log position changes
        if (m_telescopeState->slewAltRate != 0) {
            double altDeg = m_telescopeState->altitude * 180.0 / M_PI;
            qDebug() << QString("Slewing - Alt: %1° (rate: %2)")
                        .arg(altDeg, 0, 'f', 2)
                        .arg(m_telescopeState->slewAltRate);
        }
    }
}
```

## Testing the Implementation

### 1. Start Simulator
```bash
./CelestronOriginSimulator
```

### 2. Connect Client
Your telescope GUI client connects to ws://localhost/SmartScope-1.0/mountControlEndpoint

### 3. Test Slew Command
Send these commands from your client:

**Move altitude up:**
```json
{"Command": "Slew", "Destination": "Mount", "AltRate": 9, "AzmRate": 0}
```

**Stop motion:**
```json
{"Command": "Slew", "Destination": "Mount", "AltRate": 0, "AzmRate": 0}
```

**Move altitude down (parking):**
```json
{"Command": "Slew", "Destination": "Mount", "AltRate": -9, "AzmRate": 0}
```

### 4. Monitor Mount Status
Watch the mount status updates. The altitude value should change smoothly based on the slew rate.

### 5. Test Park/Unpark from GUI
- Click "Park Telescope" button
- Watch altitude decrease from current position to -10°
- Motion should stop automatically at -10°
- Click "Unpark Telescope" button
- Watch altitude increase to +60°
- Telescope should initialize automatically

## Debug Output

Expected console output when parking (moving down):
```
🚀 Slew STARTED
   Alt Rate: -9 (-4.5°/sec)
   Azm Rate: 0 (0°/sec)
   Starting Position: Alt=45.00°, Azm=180.00°
🔭 ALT:  45.00° →  44.55° (Δ-0.45°, rate: -9)
🔭 ALT:  44.55° →  44.10° (Δ-0.45°, rate: -9)
🔭 ALT:  44.10° →  43.65° (Δ-0.45°, rate: -9)
🔭 ALT:  43.65° →  43.20° (Δ-0.45°, rate: -9)
...
🔭 ALT:  -9.45° →  -9.90° (Δ-0.45°, rate: -9)
🛑 Slew STOPPED
   Final Position: Alt=-9.90°, Azm=180.00°
```

Expected console output when unparking (moving up):
```
🚀 Slew STARTED
   Alt Rate: 9 (4.5°/sec)
   Azm Rate: 0 (0°/sec)
   Starting Position: Alt=-10.00°, Azm=180.00°
🔭 ALT: -10.00° →  -9.55° (Δ 0.45°, rate: 9)
🔭 ALT:  -9.55° →  -9.10° (Δ 0.45°, rate: 9)
🔭 ALT:  -9.10° →  -8.65° (Δ 0.45°, rate: 9)
...
🔭 ALT:  59.55° →  60.00° (Δ 0.45°, rate: 9)
🛑 Slew STOPPED
   Final Position: Alt=60.00°, Azm=180.00°
```

Expected console output for azimuth motion (arrow keys):
```
🚀 Slew STARTED
   Alt Rate: 0 (0°/sec)
   Azm Rate: 9 (4.5°/sec)
   Starting Position: Alt=45.00°, Azm=180.00°
🔭 AZM: 180.00° → 180.45° (Δ 0.45°, rate: 9)
🔭 AZM: 180.45° → 180.90° (Δ 0.45°, rate: 9)
🔭 AZM: 180.90° → 181.35° (Δ 0.45°, rate: 9)
...
🛑 Slew STOPPED
   Final Position: Alt=45.00°, Azm=185.00°
```

## How It Works

1. **Client sends Slew command** with AltRate and AzmRate
2. **CommandHandler::handleSlew()** updates TelescopeState slew rates
3. **Timer calls updateSlew()** every 100ms
4. **TelescopeState::updateSlewMotion()** updates altitude/azimuth
5. **Mount status broadcasts** include updated altitude value
6. **Client monitors altitude** and sends stop command at target

## Motion Rate Calibration

Current settings:
- Rate 9 = 0.45° per 100ms = 4.5°/second
- To move from 45° to -10° (55° total): ~12 seconds
- To move from -10° to +60° (70° total): ~16 seconds

If motion is too fast/slow, adjust the scale factor in updateSlewMotion():
```cpp
double altDelta = slewAltRate * SCALE_FACTOR * (M_PI / 180.0);
// Current: SCALE_FACTOR = 0.05
// Try: 0.1 (twice as fast), 0.025 (half as fast), etc.
```

## Benefits

✅ Park/unpark commands now work
✅ Arrow keys functional for manual slewing
✅ Smooth, realistic motion simulation
✅ Matches real telescope Slew protocol
✅ Easy to test and debug

## Summary

Only one method needs updating in CelestronOriginSimulator.cpp:
- Add slew motion update to `updateSlew()` method
- The timer should already be calling this method every 100ms
- All other files are complete and ready

That's it! Your simulator will now support park/unpark and manual slewing operations.
