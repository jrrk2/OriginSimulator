# Simulator Debug Output Examples

## Park Operation (Move Down to -10°)

### Client Action
User clicks "Park Telescope" button in GUI

### Simulator Console Output
```
🚀 Slew STARTED
   Alt Rate: -9 (-4.5°/sec)
   Azm Rate: 0 (0°/sec)
   Starting Position: Alt=45.00°, Azm=180.00°

🔭 ALT:  45.00° →  44.55° (Δ-0.45°, rate: -9)
🔭 ALT:  44.55° →  44.10° (Δ-0.45°, rate: -9)
🔭 ALT:  44.10° →  43.65° (Δ-0.45°, rate: -9)
🔭 ALT:  43.65° →  43.20° (Δ-0.45°, rate: -9)
🔭 ALT:  43.20° →  42.75° (Δ-0.45°, rate: -9)
🔭 ALT:  42.75° →  42.30° (Δ-0.45°, rate: -9)
🔭 ALT:  42.30° →  41.85° (Δ-0.45°, rate: -9)
🔭 ALT:  41.85° →  41.40° (Δ-0.45°, rate: -9)
🔭 ALT:  41.40° →  40.95° (Δ-0.45°, rate: -9)
🔭 ALT:  40.95° →  40.50° (Δ-0.45°, rate: -9)
... (continues every 100ms) ...
🔭 ALT:  -8.10° →  -8.55° (Δ-0.45°, rate: -9)
🔭 ALT:  -8.55° →  -9.00° (Δ-0.45°, rate: -9)
🔭 ALT:  -9.00° →  -9.45° (Δ-0.45°, rate: -9)
🔭 ALT:  -9.45° →  -9.90° (Δ-0.45°, rate: -9)

🛑 Slew STOPPED
   Final Position: Alt=-9.90°, Azm=180.00°
```

### Timing
- Total distance: 55° (45° to -10°)
- Rate: 4.5°/second
- Time: ~12 seconds
- Updates: ~120 debug messages

---

## Unpark Operation (Move Up to +60°)

### Client Action
User clicks "Unpark Telescope" button in GUI

### Simulator Console Output
```
🚀 Slew STARTED
   Alt Rate: 9 (4.5°/sec)
   Azm Rate: 0 (0°/sec)
   Starting Position: Alt=-10.00°, Azm=180.00°

🔭 ALT: -10.00° →  -9.55° (Δ 0.45°, rate: 9)
🔭 ALT:  -9.55° →  -9.10° (Δ 0.45°, rate: 9)
🔭 ALT:  -9.10° →  -8.65° (Δ 0.45°, rate: 9)
🔭 ALT:  -8.65° →  -8.20° (Δ 0.45°, rate: 9)
🔭 ALT:  -8.20° →  -7.75° (Δ 0.45°, rate: 9)
... (continues through 0°) ...
🔭 ALT:  -0.90° →  -0.45° (Δ 0.45°, rate: 9)
🔭 ALT:  -0.45° →   0.00° (Δ 0.45°, rate: 9)
🔭 ALT:   0.00° →   0.45° (Δ 0.45°, rate: 9)
🔭 ALT:   0.45° →   0.90° (Δ 0.45°, rate: 9)
... (continues upward) ...
🔭 ALT:  58.20° →  58.65° (Δ 0.45°, rate: 9)
🔭 ALT:  58.65° →  59.10° (Δ 0.45°, rate: 9)
🔭 ALT:  59.10° →  59.55° (Δ 0.45°, rate: 9)
🔭 ALT:  59.55° →  60.00° (Δ 0.45°, rate: 9)

🛑 Slew STOPPED
   Final Position: Alt=60.00°, Azm=180.00°
```

### Timing
- Total distance: 70° (-10° to +60°)
- Rate: 4.5°/second
- Time: ~16 seconds
- Updates: ~160 debug messages

---

## Arrow Key - Up (Manual Slew)

### Client Action
User presses UP arrow key, holds for 2 seconds, releases

### Simulator Console Output
```
🚀 Slew STARTED
   Alt Rate: 9 (4.5°/sec)
   Azm Rate: 0 (0°/sec)
   Starting Position: Alt=30.00°, Azm=180.00°

🔭 ALT:  30.00° →  30.45° (Δ 0.45°, rate: 9)
🔭 ALT:  30.45° →  30.90° (Δ 0.45°, rate: 9)
🔭 ALT:  30.90° →  31.35° (Δ 0.45°, rate: 9)
🔭 ALT:  31.35° →  31.80° (Δ 0.45°, rate: 9)
🔭 ALT:  31.80° →  32.25° (Δ 0.45°, rate: 9)
🔭 ALT:  32.25° →  32.70° (Δ 0.45°, rate: 9)
🔭 ALT:  32.70° →  33.15° (Δ 0.45°, rate: 9)
🔭 ALT:  33.15° →  33.60° (Δ 0.45°, rate: 9)
🔭 ALT:  33.60° →  34.05° (Δ 0.45°, rate: 9)
🔭 ALT:  34.05° →  34.50° (Δ 0.45°, rate: 9)
🔭 ALT:  34.50° →  34.95° (Δ 0.45°, rate: 9)
🔭 ALT:  34.95° →  35.40° (Δ 0.45°, rate: 9)
🔭 ALT:  35.40° →  35.85° (Δ 0.45°, rate: 9)
🔭 ALT:  35.85° →  36.30° (Δ 0.45°, rate: 9)
🔭 ALT:  36.30° →  36.75° (Δ 0.45°, rate: 9)
🔭 ALT:  36.75° →  37.20° (Δ 0.45°, rate: 9)
🔭 ALT:  37.20° →  37.65° (Δ 0.45°, rate: 9)
🔭 ALT:  37.65° →  38.10° (Δ 0.45°, rate: 9)
🔭 ALT:  38.10° →  38.55° (Δ 0.45°, rate: 9)
🔭 ALT:  38.55° →  39.00° (Δ 0.45°, rate: 9)

🛑 Slew STOPPED
   Final Position: Alt=39.00°, Azm=180.00°
```

### Analysis
- Held for 2 seconds
- Moved 9° (2 seconds × 4.5°/sec)
- 20 position updates

---

## Arrow Key - Right (Manual Azimuth Slew)

### Client Action
User presses RIGHT arrow key, holds for 1 second, releases

### Simulator Console Output
```
🚀 Slew STARTED
   Alt Rate: 0 (0°/sec)
   Azm Rate: 9 (4.5°/sec)
   Starting Position: Alt=45.00°, Azm=180.00°

🔭 AZM: 180.00° → 180.45° (Δ 0.45°, rate: 9)
🔭 AZM: 180.45° → 180.90° (Δ 0.45°, rate: 9)
🔭 AZM: 180.90° → 181.35° (Δ 0.45°, rate: 9)
🔭 AZM: 181.35° → 181.80° (Δ 0.45°, rate: 9)
🔭 AZM: 181.80° → 182.25° (Δ 0.45°, rate: 9)
🔭 AZM: 182.25° → 182.70° (Δ 0.45°, rate: 9)
🔭 AZM: 182.70° → 183.15° (Δ 0.45°, rate: 9)
🔭 AZM: 183.15° → 183.60° (Δ 0.45°, rate: 9)
🔭 AZM: 183.60° → 184.05° (Δ 0.45°, rate: 9)
🔭 AZM: 184.05° → 184.50° (Δ 0.45°, rate: 9)

🛑 Slew STOPPED
   Final Position: Alt=45.00°, Azm=184.50°
```

### Analysis
- Held for 1 second
- Moved 4.5° in azimuth
- Altitude unchanged
- 10 position updates

---

## Combined Motion (Diagonal)

### Client Action
User presses UP and RIGHT arrow keys simultaneously

### Simulator Console Output
```
🚀 Slew STARTED
   Alt Rate: 9 (4.5°/sec)
   Azm Rate: 9 (4.5°/sec)
   Starting Position: Alt=30.00°, Azm=180.00°

🔭 ALT:  30.00° →  30.45° (Δ 0.45°, rate: 9)
🔭 AZM: 180.00° → 180.45° (Δ 0.45°, rate: 9)
🔭 ALT:  30.45° →  30.90° (Δ 0.45°, rate: 9)
🔭 AZM: 180.45° → 180.90° (Δ 0.45°, rate: 9)
🔭 ALT:  30.90° →  31.35° (Δ 0.45°, rate: 9)
🔭 AZM: 180.90° → 181.35° (Δ 0.45°, rate: 9)
... (continues) ...

🛑 Slew STOPPED
   Final Position: Alt=35.00°, Azm=185.00°
```

### Analysis
- Both axes move simultaneously
- Diagonal motion across sky
- Each axis moves at 4.5°/sec

---

## Edge Cases

### Hitting Altitude Limit (90°)

```
🚀 Slew STARTED
   Alt Rate: 9 (4.5°/sec)
   Azm Rate: 0 (0°/sec)
   Starting Position: Alt=88.00°, Azm=180.00°

🔭 ALT:  88.00° →  88.45° (Δ 0.45°, rate: 9)
🔭 ALT:  88.45° →  88.90° (Δ 0.45°, rate: 9)
🔭 ALT:  88.90° →  89.35° (Δ 0.45°, rate: 9)
🔭 ALT:  89.35° →  89.80° (Δ 0.45°, rate: 9)
🔭 ALT:  89.80° →  90.00° (Δ 0.20°, rate: 9)  ← Clamped!
🔭 ALT:  90.00° →  90.00° (Δ 0.00°, rate: 9)  ← At limit
🔭 ALT:  90.00° →  90.00° (Δ 0.00°, rate: 9)  ← Still at limit
...
```

### Azimuth Wraparound (360° → 0°)

```
🚀 Slew STARTED
   Alt Rate: 0 (0°/sec)
   Azm Rate: 9 (4.5°/sec)
   Starting Position: Alt=45.00°, Azm=358.00°

🔭 AZM: 358.00° → 358.45° (Δ 0.45°, rate: 9)
🔭 AZM: 358.45° → 358.90° (Δ 0.45°, rate: 9)
🔭 AZM: 358.90° → 359.35° (Δ 0.45°, rate: 9)
🔭 AZM: 359.35° → 359.80° (Δ 0.45°, rate: 9)
🔭 AZM: 359.80° →   0.25° (Δ 0.45°, rate: 9)  ← Wrapped!
🔭 AZM:   0.25° →   0.70° (Δ 0.45°, rate: 9)
...
```

---

## Performance Analysis

### Update Frequency
- Timer interval: 100ms
- Updates per second: 10
- Motion per update: 0.45° (at rate 9)
- Motion per second: 4.5° (at rate 9)

### Motion Rates
| Rate | °/update | °/second | Description |
|------|----------|----------|-------------|
| 1    | 0.05°    | 0.5°/s   | Very slow   |
| 3    | 0.15°    | 1.5°/s   | Slow        |
| 5    | 0.25°    | 2.5°/s   | Medium      |
| 7    | 0.35°    | 3.5°/s   | Fast        |
| 9    | 0.45°    | 4.5°/s   | Maximum     |

### Typical Park Operation
```
Distance: 55° (45° to -10°)
Rate: 4.5°/second
Time: 55° ÷ 4.5°/s = 12.2 seconds
Updates: 122 messages
```

---

## Reading the Debug Output

### Format Explanation
```
🔭 ALT:  45.00° →  44.55° (Δ-0.45°, rate: -9)
     │      │         │        │       └─ Current rate setting
     │      │         │        └─ Change this update
     │      │         └─ New position
     │      └─ Old position
     └─ Axis (ALT or AZM)
```

### Rate Signs
- **Positive rate** (+9): Moving up (altitude) or clockwise (azimuth)
- **Negative rate** (-9): Moving down (altitude) or counterclockwise (azimuth)
- **Zero rate** (0): Not moving on this axis

### Delta Signs
- **Positive delta** (+0.45°): Position increased
- **Negative delta** (-0.45°): Position decreased
- **Zero delta** (0.00°): Position unchanged (at limit)

---

## Troubleshooting

### No Debug Output
**Problem:** Simulator runs but no slew messages appear

**Check:**
1. Is `updateSlew()` being called? Add debug at start of method
2. Is timer running? Check `m_slewTimer->isActive()`
3. Is `isSlewing` flag set? Check in `handleSlew()`

### Motion Too Fast/Slow
**Problem:** Telescope moves at wrong speed

**Adjust:** Scale factor in `updateSlewMotion()`:
```cpp
// Current:
double altDelta = slewAltRate * 0.05 * (M_PI / 180.0);

// Slower (half speed):
double altDelta = slewAltRate * 0.025 * (M_PI / 180.0);

// Faster (double speed):
double altDelta = slewAltRate * 0.10 * (M_PI / 180.0);
```

### Position Not Updating in GUI
**Problem:** Debug shows motion but GUI altitude doesn't change

**Check:**
1. Is mount status being broadcast? (every 5 seconds)
2. Does status include altitude field?
3. Is altitude being sent in radians or degrees?
