# Music Bottles v4 (Raspberry Pi)

Interactive installation: users lift bottle caps to trigger layered music playback. Bottle identities are determined by weight differences measured on a single load cell (HX711). An Arduino drives NeoPixel LEDs to visualize bottle/cap states.

## Conceptual design

- **Single scale, three bottles**: All bottles sit on one load cell. The system interprets total weight changes to infer which bottle and/or cap has been removed.
- **Discrete state model**: Each bottle has three states: (0) bottle+cap present, (1) bottle present/cap removed, (2) bottle removed. The system enumerates all 27 state combinations and matches the live weight to the closest combination within a threshold.
- **Audio layering**: Three audio channels are continuously looped. When a bottle’s cap is removed, that bottle’s audio channel fades in. When it is replaced or removed, the channel fades out.
- **Lighting feedback**: The Pi sets GPIO output pins for each bottle/cap state. An Arduino reads these pins and animates NeoPixel segments for each bottle.

## Technical design

### Core runtime

- **Main app**: `musicBottles.c`
  - Reads the HX711 via `hx711.c`/`hx711.h`.
  - Uses GPIO (via `minimal_gpio.c`) for button input and for signaling the Arduino.
  - Uses SDL2 + SDL2_mixer (via `audio.c`) for audio playback.
  - Implements state matching for the three bottles based on weight deltas.

- **Audio**: `audio.c` / `audio.h`
  - Initializes SDL2 audio and SDL2_mixer.
  - Loads 3 tracks per “set” (Jazz, Classic, Synth, Boston) and assigns them to channels A/B/C.
  - Fades channel volume to create smooth transitions.

- **Scale interface**: `hx711.c` / `hx711.h`
  - Bit-bangs HX711 data/clock lines.
  - Provides `getCleanSample()` for noise-reduced sampling and a simple `speedTest()`.

- **GPIO memory mapping**: `minimal_gpio.c`
  - Bare-metal style /dev/mem access for fast GPIO operations.

- **Utilities**:
  - `scaleTool.c`: live sampling and tare tool to measure raw and filtered values.
  - `lowpass.c`: test harness for low-pass filtering behavior.
  - `runBottlesSquare.sh`: example run command with calibrated weights.

### Arduino lighting controller

- `arduino/musicBottles/musicBottles.ino`
  - Reads six GPIO pins (3 bottles + 3 caps) from the Pi.
  - Drives a NeoPixel strip split into three sections, each section representing a bottle.

- `arduino/musicBottles/neopixelbottlesthirds.h`
  - Pattern logic and per-bottle color definitions (closed, open, missing).

### Bottle state mapping

For each bottle $i$, the runtime uses three weights:

- $W_{bot_i}$: bottle body weight
- $W_{cap_i}$: cap weight
- $W_{tare}$: empty scale baseline

The measured sample is compared to all 27 combinations:

- state 0: bottle+cap on
- state 1: bottle on, cap off
- state 2: bottle+cap off

For any combination, the expected delta is:

$$\Delta W = -\sum_{i=1}^{3} (\text{removed parts})$$

A combination is valid if its distance to the measured delta is within `STABLE_THRESH` in [musicBottles.c](musicBottles.c).

### GPIO pin map (Pi)

Inputs (buttons):
- Re-tare: GPIO 26
- Music set: GPIO 19, 13, 6, 5

Outputs (to Arduino):
- Bottle 1: GPIO 18
- Cap 1: GPIO 17
- Bottle 2: GPIO 27
- Cap 2: GPIO 22
- Bottle 3: GPIO 23
- Cap 3: GPIO 24

HX711:
- Data: GPIO 21
- Clock: GPIO 20

### Audio channels

- Channel A: Bottle 1
- Channel B: Bottle 2
- Channel C: Bottle 3

If all bottles are “off” (state 2), the streams are rewound so the next interaction starts from the beginning.

## Building and running

### Dependencies

- GCC
- SDL2
- SDL2_mixer

On Raspberry Pi OS (example):

- `sudo apt-get install libsdl2-dev libsdl2-mixer-dev`

### Build

- `make musicbottles`

This compiles [musicBottles.c](musicBottles.c) with [hx711.c](hx711.c), [audio.c](audio.c), and [gb_common.c](gb_common.c).

### Run

The runtime expects **six integer weights** (bottle and cap for each bottle), plus an optional tare:

```
./musicBottles bot1 cap1 bot2 cap2 bot3 cap3 [tare]
```

If `tare` is omitted, the program performs a tare on startup. Example run command is in [runBottlesSquare.sh](runBottlesSquare.sh).

### Calibration workflow

1. Run [scaleTool.c](scaleTool.c) to measure and stabilize tare.
2. Measure each bottle and cap weight (scaled by 100 in this codebase).
3. Start `musicBottles` with the measured values.
4. Tune `STABLE_THRESH` in [musicBottles.c](musicBottles.c) if state matching is noisy.

### Audio assets

`audio.c` expects `.wav` files such as `jazz1.wav`, `classic1.wav`, `synth1.wav`, etc., under `music-files/`. Ensure the WAV files exist and match the configured names.

## Platform portability (Pi 3 → Pi 4)

This code uses **direct /dev/mem GPIO mapping**, which is sensitive to SoC base addresses. Two locations are relevant:

1. **Gertboard-style mapping** in [gb_common.c](gb_common.c) uses `BCM2708_PERI_BASE` hard-coded to `0x3F000000` (Pi 2/3).
2. **minimal_gpio.c** uses `piPeriphBase = 0x3F000000` for ARMv7/ARMv8 systems (Pi 2/3).

On **Raspberry Pi 4**, the peripheral base address is **0xFE000000**. To run on Pi 4, you must update these bases:

- In [gb_common.c](gb_common.c), change `BCM2708_PERI_BASE` to `0xFE000000`.
- In [minimal_gpio.c](minimal_gpio.c), set `piPeriphBase` to `0xFE000000` when detecting Pi 4.

Recommended portability approach:

- Replace manual /dev/mem access with a maintained GPIO library (e.g., pigpio or libgpiod) to avoid base-address differences.
- Gate Pi model detection and base selection in one place and share it between `gb_common` and `minimal_gpio`.

## Project layout

- [musicBottles.c](musicBottles.c): main runtime logic
- [audio.c](audio.c): SDL2 audio loading and playback
- [hx711.c](hx711.c): load cell interface
- [minimal_gpio.c](minimal_gpio.c): fast GPIO access
- [gb_common.c](gb_common.c): Gertboard helpers (mem-mapped GPIO)
- [scaleTool.c](scaleTool.c): measurement tool
- [lowpass.c](lowpass.c): filter test harness
- [arduino/musicBottles/musicBottles.ino](arduino/musicBottles/musicBottles.ino): LED control firmware

## Notes

- The program must be run with root privileges (`sudo`) due to /dev/mem access.
- `STABLE_THRESH` and the sampling parameters in `handleScale()` are tuned empirically per build and load cell.
- Audio volumes use the SDL2_mixer range 0–128. The code keeps volume above 100 to avoid fade-out logic suppressing playback.
