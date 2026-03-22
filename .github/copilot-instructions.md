# Instructions for GitHub Copilot

## Building This Project

This is a JUCE-based audio plugin project that requires specific dependencies on Linux.

### Before Building on Linux

1. **Install dependencies first**: Run `sudo bash scripts/install-linux-deps.sh`
2. **Initialize submodules**: Ensure JUCE submodule is initialized with `git submodule update --init --recursive`
3. **Use the Linux preset**: Build with `cmake --preset linux-release && cmake --build --preset linux-build`

### Build Timeouts

If builds timeout:
- Use fewer parallel jobs: `cmake --build --preset linux-build -j2`
- Ensure all dependencies are installed before attempting to build
- The JUCE submodule must be fully initialized

### Documentation

- Main README: [README.md](./README.md)
- Linux build guide: [docs/LINUX_BUILD.md](./docs/LINUX_BUILD.md)
- JUCE Linux dependencies: [JUCE/docs/Linux Dependencies.md](./JUCE/docs/Linux%20Dependencies.md)

---

## DSP Architecture — OttCompressor (3-Module Design)

The compressor is built from three composable modules in `src/`. All modifications to the DSP must respect these guardrails.

### Modules

1. **VolumeDetector** (`src/VolumeDetector.h`): A pluggable level detector with two modes:
   - **RMS**: Running sum-of-squares with configurable window length (ms). O(1) per sample via circular buffer. Window can be BPM-synced to musical divisions (1/8, 1/4, 1/2, 1, 2, 4 beats).
   - **PeakMax**: Tracks maximum absolute peak in a configurable short window (ms). Uses circular buffer with rescan-on-eviction.
   - Output: smooth dB level per channel via `processSample()` and `getCurrentLevelDb()`.

2. **CompressorStage** (`src/CompressorStage.h`): A single independent compressor stage, configurable for Downward or Upward operation:
   - **Binary trigger**: 1.0 when detector level crosses threshold (above for Downward, below for Upward), 0.0 otherwise.
   - **Gate ballistics**: One-pole smoother on the binary trigger with independent attack/release.
   - **Gain computation**: `gainDb = ±gate * |envDb - threshDb| * (1 - 1/ratio)`.
   - Returns `Result { gainDb, gate }` per sample.

3. **OttCompressor** (`src/OttCompressor.h`): Orchestrator that composes VolumeDetector + 2×CompressorStage:
   - Per-sample flow: `detector.processSample()` → `downStage.process()` + `upStage.process()` → sum gains → apply to input.
   - Exposes `getDetectorLevelDb(channel)` for UI FIFO.

### Architecture (per-sample order)

1. **Detect**: `VolumeDetector` processes raw audio → smooth dB envelope.
2. **Trigger + Gate + Gain** (×2): Each `CompressorStage` receives the smooth envelope dB:
   - Binary trigger: 0 or 1 based on threshold comparison.
   - Gate smoothing: one-pole ballistics on the trigger value.
   - Gain: `gate * |envDb - threshDb| * (1 - 1/ratio)` — negative for Downward, positive for Upward.
3. **Apply**: `totalGainDb = upGainDb + downGainDb`, then `output = input * dBtoLinear(totalGainDb)`.

### Hard Rules — DO NOT VIOLATE

- **NEVER feed raw audio samples** into CompressorStage. Stages receive only the smooth dB envelope from VolumeDetector.
- **NEVER swap attack/release mappings**. Both stages use direct mapping: user attack → smoother attack, user release → smoother release.
- **NEVER remove the VolumeDetector**. Without it, gain computation oscillates at audio rate.
- **NEVER compute gain from raw `abs(input)`**. All gain computation uses the smooth envelope.
- **Binary trigger is intentional**. The gate ballistics smooth the 0/1 trigger — do NOT feed continuous dB values into the gate smoother.
- **Stages are independent**. Do NOT cross-couple downward and upward stages (no shared coefficients between them).

### Parameters (13 total)

| ID                | Description                        | Range / Type              |
|-------------------|------------------------------------|---------------------------|
| `down_thresh`     | Downward compression threshold dB  | -60 to 0                  |
| `down_ratio`      | Downward compression ratio         | 1 to 20                   |
| `down_attack_ms`  | Downward gate attack time ms       | 0.01 to 1000              |
| `down_release_ms` | Downward gate release time ms      | 0.1 to 2000               |
| `up_thresh`       | Upward compression threshold dB    | -60 to 0                  |
| `up_ratio`        | Upward compression ratio           | 1 to 20                   |
| `up_attack_ms`    | Upward gate attack time ms         | 0.01 to 1000              |
| `up_release_ms`   | Upward gate release time ms        | 0.1 to 2000               |
| `detector_type`   | Volume detector mode               | Choice: RMS, Peak         |
| `rms_window_ms`   | RMS detector window length ms      | 1.0 to 300.0              |
| `rms_sync_mode`   | RMS window synced to BPM           | Bool                       |
| `rms_beat_div`    | RMS musical division               | Choice: 1/8,1/4,1/2,1,2,4 |
| `peak_window_ms`  | PeakMax detector window length ms  | 1.0 to 50.0               |
