# EXACT CODE TO ADD TO CelestronOriginSimulator.cpp

## Problem Analysis

Your output shows:
```
🚀 Slew STARTED
   Starting Position: Alt=3437.75°, Azm=0.00°
```

This means:
1. ✅ The Slew command IS being received (you see the start message)
2. ❌ The altitude value was uninitialized garbage (3437.75°)
3. ❌ The updateSlew() timer is not calling updateSlewMotion() (no motion updates)

## Solution

### Step 1: Find the updateSlew() method

Look for this in your CelestronOriginSimulator.cpp:

```cpp
void CelestronOriginSimulator::updateSlew() {
    // Existing code here (if any)
}
```

### Step 2: Replace or Add This Complete Implementation

```cpp
void CelestronOriginSimulator::updateSlew() {
    // Update slew motion if active
    if (m_telescopeState->isSlewing) {
        m_telescopeState->updateSlewMotion();
    }
    
    // Existing goto/slew code can stay here if you have any
}
```

### Step 3: Verify Timer is Running

In your `setupTimers()` method, make sure you have:

```cpp
void CelestronOriginSimulator::setupTimers() {
    // ... other timers ...
    
    // Make sure this exists:
    m_slewTimer = new QTimer(this);
    connect(m_slewTimer, &QTimer::timeout, this, &CelestronOriginSimulator::updateSlew);
    m_slewTimer->start(100);  // Update every 100ms
    
    qDebug() << "✓ Slew timer started (100ms interval)";
}
```

## Quick Test Code

Add this temporary debug code to verify the timer is working:

```cpp
void CelestronOriginSimulator::updateSlew() {
    static int callCount = 0;
    callCount++;
    
    // Debug: Print every 50 calls (every 5 seconds)
    if (callCount % 50 == 0) {
        qDebug() << "updateSlew() called" << callCount << "times";
    }
    
    // Update manual slew motion if active (Slew command)
    if (m_telescopeState->isManualSlewing) {
        qDebug() << "About to call updateSlewMotion()";
        m_telescopeState->updateSlewMotion();
        qDebug() << "After updateSlewMotion()";
    }
    
    // Update goto slew if active (GotoRaDec command)
    if (m_telescopeState->isSlewing) {
        // Your existing GotoRaDec code here
    }
}
```

## Expected Output After Fix

When you click "Park Telescope", you should now see:

```
🚀 Slew STARTED
   Alt Rate: -9 (-4.5°/sec)
   Azm Rate: 0 (0°/sec)
   Starting Position: Alt=45.00°, Azm=180.00°
About to call updateSlewMotion()
🔭 ALT:  45.00° →  44.55° (Δ-0.45°, rate: -9)
After updateSlewMotion()
About to call updateSlewMotion()
🔭 ALT:  44.55° →  44.10° (Δ-0.45°, rate: -9)
After updateSlewMotion()
About to call updateSlewMotion()
🔭 ALT:  44.10° →  43.65° (Δ-0.45°, rate: -9)
After updateSlewMotion()
... (continues every 100ms) ...
```

## If Still No Motion

### Check 1: Is Timer Created?

Add this to end of setupTimers():

```cpp
qDebug() << "=== Timer Status ===";
qDebug() << "Slew timer active:" << (m_slewTimer && m_slewTimer->isActive());
qDebug() << "Slew timer interval:" << (m_slewTimer ? m_slewTimer->interval() : -1);
```

### Check 2: Is updateSlew() Being Called?

At the very start of updateSlew():

```cpp
void CelestronOriginSimulator::updateSlew() {
    qDebug() << "▶ updateSlew() called, isSlewing:" << m_telescopeState->isSlewing;
    
    // ... rest of method
}
```

### Check 3: Is State Being Set?

In CommandHandler::handleSlew(), verify:

```cpp
qDebug() << "After setting state:";
qDebug() << "  isSlewing:" << m_telescopeState->isSlewing;
qDebug() << "  slewAltRate:" << m_telescopeState->slewAltRate;
qDebug() << "  slewAzmRate:" << m_telescopeState->slewAzmRate;
```

## Complete updateSlew() Template

Here's the complete method with all debug:

```cpp
void CelestronOriginSimulator::updateSlew() {
    // Optional: Print that we're being called
    static int callCount = 0;
    if (++callCount % 50 == 0) {
        qDebug() << "⏱ updateSlew() heartbeat:" << callCount << "calls";
    }
    
    // Update slew motion if active
    if (m_telescopeState->isSlewing) {
        // This is where the magic happens
        m_telescopeState->updateSlewMotion();
    }
    
    // Keep any existing goto/slew code here if you have it
    if (m_telescopeState->isSlewing && 
        m_telescopeState->targetRa != 0.0 && 
        m_telescopeState->targetDec != 0.0) {
        // Your existing goto slew code
    }
}
```

## Minimal Fix (No Debug)

If you just want it working without debug spam:

```cpp
void CelestronOriginSimulator::updateSlew() {
    // Handle manual slewing (Slew command with Alt/Az rates)
    if (m_telescopeState->isManualSlewing) {
        m_telescopeState->updateSlewMotion();
    }
    
    // Handle goto slewing (GotoRaDec command with RA/Dec target)
    if (m_telescopeState->isSlewing) {
        // Your existing GotoRaDec slew code here
    }
}
```

## Alternative: If updateSlew() Doesn't Exist Yet

Create the complete method:

```cpp
void CelestronOriginSimulator::updateSlew() {
    // Handle manual slewing (Slew command)
    if (m_telescopeState->isSlewing) {
        m_telescopeState->updateSlewMotion();
    }
    
    // Handle goto slewing (GotoRaDec command)
    if (m_telescopeState->targetRa != 0.0 || m_telescopeState->targetDec != 0.0) {
        // Simulate goto completion
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

## Testing Checklist

1. ✅ Start simulator
2. ✅ See "Slew timer started" message
3. ✅ See heartbeat messages every 5 seconds
4. ✅ Connect client
5. ✅ Click "Park Telescope"
6. ✅ See "Slew STARTED" with Alt=45.00°
7. ✅ See motion updates every 100ms
8. ✅ See altitude decreasing
9. ✅ See "Slew STOPPED" at -10°

## Summary

The issue is that `updateSlew()` is either:
1. Not calling `updateSlewMotion()`, OR
2. Not being called by the timer at all

Add the single line:
```cpp
m_telescopeState->updateSlewMotion();
```

Inside the `if (m_telescopeState->isSlewing)` block in your `updateSlew()` method.

That's it!
