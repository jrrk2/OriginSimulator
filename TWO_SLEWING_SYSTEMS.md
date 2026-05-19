# Two Separate Slewing Systems - Explained

## The Problem You Identified

You were right! There are TWO different slewing mechanisms using the same `isSlewing` flag:

1. **GotoRaDec** - Slewing to celestial coordinates (RA/Dec)
2. **Slew command** - Manual slewing with Alt/Az rates (arrow keys, park/unpark)

They were fighting over the same flag, so when you used the Slew command, it would set `isSlewing = true`, but then the existing GotoRaDec code would also check that flag and interfere.

## The Solution

Now we have **two separate flags**:

```cpp
// In TelescopeState.h
bool isSlewing = false;        // For GotoRaDec (RA/Dec target)
bool isManualSlewing = false;  // For Slew command (Alt/Az rates)
```

## How They Work

### GotoRaDec Command (Existing)
```
Client sends: {"Command": "GotoRaDec", "Ra": 3.14, "Dec": 0.78}
              ↓
handleGotoRaDec() sets:
  - isSlewing = true
  - targetRa = 3.14
  - targetDec = 0.78
              ↓
updateSlew() checks isSlewing:
  - Gradually moves telescope toward target
  - Updates ra and dec
  - Sets isSlewing = false when arrived
```

### Slew Command (New)
```
Client sends: {"Command": "Slew", "AltRate": -9, "AzmRate": 0}
              ↓
handleSlew() sets:
  - isManualSlewing = true
  - slewAltRate = -9
  - slewAzmRate = 0
              ↓
updateSlew() checks isManualSlewing:
  - Calls updateSlewMotion()
  - Updates altitude and azimuth based on rates
  - Continues until rate set to 0
```

## updateSlew() Method Structure

```cpp
void CelestronOriginSimulator::updateSlew() {
    // Handle manual Alt/Az slewing (Slew command)
    if (m_telescopeState->isManualSlewing) {
        m_telescopeState->updateSlewMotion();
    }
    
    // Handle RA/Dec goto slewing (GotoRaDec command)
    if (m_telescopeState->isSlewing) {
        // Your existing goto slew code
        // (moves toward targetRa, targetDec)
    }
}
```

## Why This Works

### Independent Operation
- **GotoRaDec** can run while **Slew** is stopped
- **Slew** can run while **GotoRaDec** is stopped
- Both can technically run simultaneously (though unusual)

### No Conflicts
- Each system has its own flag
- Each system updates different coordinates
- No interference between systems

## Real Telescope Behavior

This matches how the real Celestron Origin works:

### GotoRaDec
- User selects target in star map
- Telescope slews to RA/Dec coordinates
- Automatic, ends at target

### Slew Command
- User presses arrow keys
- Telescope moves continuously in Alt/Az
- Manual, continues until released

### They're Independent!
- You can press arrow keys during a goto (cancels goto)
- You can initiate a goto while manually slewing (cancels manual slew)
- But internally, they use different control systems

## Complete State Flow

### Park Operation Example

```
1. User clicks "Park Telescope"
   ↓
2. Client sends: {"Command": "Slew", "AltRate": -9, "AzmRate": 0}
   ↓
3. handleSlew() sets:
   - isManualSlewing = true
   - slewAltRate = -9
   ↓
4. updateSlew() called every 100ms:
   - Checks isManualSlewing → true
   - Calls updateSlewMotion()
   - altitude decreases by 0.45°
   ↓
5. Client monitors altitude, when at -10°:
   ↓
6. Client sends: {"Command": "Slew", "AltRate": 0, "AzmRate": 0}
   ↓
7. handleSlew() sets:
   - isManualSlewing = false
   - slewAltRate = 0
   ↓
8. updateSlew() called:
   - Checks isManualSlewing → false
   - No motion occurs
   ↓
9. Parked!
```

### Goto Operation Example

```
1. User clicks target on star map
   ↓
2. Client sends: {"Command": "GotoRaDec", "Ra": 3.14, "Dec": 0.78}
   ↓
3. handleGotoRaDec() sets:
   - isSlewing = true
   - targetRa = 3.14
   - targetDec = 0.78
   ↓
4. updateSlew() called every 100ms:
   - Checks isSlewing → true
   - Gradually adjusts ra and dec
   - Calculates error from target
   ↓
5. When ra ≈ targetRa and dec ≈ targetDec:
   ↓
6. Sets isSlewing = false
   ↓
7. Arrived!
```

## State Variables Summary

### For GotoRaDec
```cpp
bool isSlewing;      // Is goto active?
double targetRa;     // Where we're going (RA)
double targetDec;    // Where we're going (Dec)
double ra;           // Current position (RA)
double dec;          // Current position (Dec)
```

### For Slew Command
```cpp
bool isManualSlewing;  // Is manual slew active?
int slewAltRate;       // How fast to move altitude (-9 to +9)
int slewAzmRate;       // How fast to move azimuth (-9 to +9)
double altitude;       // Current altitude (radians)
double azimuth;        // Current azimuth (radians)
```

### They're Separate!
- Different flags
- Different coordinates
- Different update methods
- No interference

## Code in CelestronOriginSimulator.cpp

### What You Need to Add

```cpp
void CelestronOriginSimulator::updateSlew() {
    // NEW: Handle manual Alt/Az slewing
    if (m_telescopeState->isManualSlewing) {
        m_telescopeState->updateSlewMotion();
    }
    
    // EXISTING: Handle RA/Dec goto slewing
    if (m_telescopeState->isSlewing) {
        // Your existing goto code here
        // Example:
        double raError = m_telescopeState->targetRa - m_telescopeState->ra;
        double decError = m_telescopeState->targetDec - m_telescopeState->dec;
        
        if (fabs(raError) < 0.0001 && fabs(decError) < 0.0001) {
            m_telescopeState->isSlewing = false;
            m_telescopeState->isGotoOver = true;
        } else {
            m_telescopeState->ra += raError * 0.1;
            m_telescopeState->dec += decError * 0.1;
        }
    }
}
```

## Debugging Both Systems

```cpp
void CelestronOriginSimulator::updateSlew() {
    // Debug both systems
    if (m_telescopeState->isManualSlewing) {
        qDebug() << "Manual slew active: Alt rate:" 
                 << m_telescopeState->slewAltRate
                 << "Azm rate:" << m_telescopeState->slewAzmRate;
        m_telescopeState->updateSlewMotion();
    }
    
    if (m_telescopeState->isSlewing) {
        qDebug() << "Goto active: Target RA:" 
                 << m_telescopeState->targetRa
                 << "Dec:" << m_telescopeState->targetDec;
        // Your goto code
    }
}
```

## Summary

### Before (Broken)
```
isSlewing used for BOTH:
  - GotoRaDec (RA/Dec)
  - Slew command (Alt/Az)
  → They fight over the flag!
  → Manual slew doesn't work
```

### After (Fixed)
```
isSlewing for GotoRaDec (RA/Dec)
isManualSlewing for Slew command (Alt/Az)
  → Each system independent
  → Both work correctly
  → No conflicts
```

## Files Updated

1. **TelescopeState.h**
   - Added `bool isManualSlewing`
   - Updated `updateSlewMotion()` to check `isManualSlewing`

2. **CommandHandler.cpp**
   - Updated `handleSlew()` to use `isManualSlewing`

3. **CelestronOriginSimulator.cpp** (YOU need to update)
   - Add check for `isManualSlewing` in `updateSlew()`
   - Keep existing `isSlewing` check for goto

This is the correct architecture that matches the real telescope!
