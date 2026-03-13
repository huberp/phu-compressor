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

## DSP Architecture — OttCompressor

The compressor in `src/OttCompressor.h` uses a **standard 4-stage** architecture. All modifications to the DSP must respect these guardrails.

### Architecture (in per-sample order)

1. **Level Detector**: A fast peak envelope follower (`BallisticsFilter` with fixed 0.01 ms attack, 50 ms release) converts oscillating audio into a smooth envelope. This is the ONLY component that sees raw audio.
2. **Gain Computer**: Two independent static curves compute always-positive dB values from the smooth envelope:
   - Downward: `downGrDb = (envDb - threshDb) * (1 - 1/ratio)` when `envDb > downThreshDb` (0 otherwise).
   - Upward: `upBoostDb = (threshDb - envDb) * (1 - 1/ratio)` when `envDb < upThreshDb` (0 otherwise). Upward target is always 0 when in compression zone.
3. **Ballistics**: Two independent smoothers:
   - `downBallistics` (`BallisticsFilter`): smooths `downGrDb` with user attack/release (direct mapping).
   - `upBallistics` (manual one-pole): smooths `upBoostDb` with user upAttack when rising, user upRelease when falling — **but uses `downAttackCoeff` when in compression zone (`envDb > downThreshDb`)**. This ensures residual boost releases in sync with compression onset.
4. **Apply Gain**: `totalGainDb = smoothedBoost - smoothedGR`, then `output = input * dBtoLinear(totalGainDb)`.

### Hard Rules — DO NOT VIOLATE

- **NEVER feed raw audio samples** into `downBallistics` or `upBallistics`. They receive only positive dB values from the gain computer.
- **NEVER swap attack/release mappings**. Both paths use direct mapping: user attack → BF attack, user release → BF release. No inversions.
- **NEVER remove the level detector**. The envelope follower is essential — without it, gain computation oscillates at audio rate.
- **NEVER use binary 0/1 triggers**. The ballistics smooth the computed GR/boost dB values directly.
- **NEVER compute gain from raw `abs(input)`**. All gain computation uses the smooth envelope from the level detector.
- **NEVER allow upward boost to linger during compression** — the manual up-smoother must use `downAttackCoeff` when `envDb > downThreshDb`.

### Parameters (8 total)

| ID                | Description                        | Range           |
|-------------------|------------------------------------|-----------------|
| `down_thresh`     | Downward compression threshold dB  | -60 to 0        |
| `down_ratio`      | Downward compression ratio         | 1 to 20         |
| `down_attack_ms`  | Downward trigger attack time ms    | 0.01 to 1000    |
| `down_release_ms` | Downward trigger release time ms   | 0.1 to 2000     |
| `up_thresh`       | Upward compression threshold dB    | -60 to 0        |
| `up_ratio`        | Upward compression ratio           | 1 to 20         |
| `up_attack_ms`    | Upward trigger attack time ms      | 0.01 to 1000    |
| `up_release_ms`   | Upward trigger release time ms     | 0.1 to 2000     |
