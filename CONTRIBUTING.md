# Contributing to PHU Compressor

Contributions are welcome.

1. Fork and branch from `main`
2. Follow existing C++20/JUCE code style
3. No memory allocation, system calls, or locks on the audio thread
4. Verify the project builds and passes pluginval before opening a PR

**Bug reports** — please include DAW name/version, OS, and reproduction steps in a [GitHub Issue](https://github.com/huberp/phu-compressor/issues).

---

## Project Layout

```
phu-compressor/
├── CMakeLists.txt / CMakePresets.json
├── cmake/
│   ├── PhuDependencies.cmake        FetchContent helpers for JUCE and phu-audio-lib
│   ├── juce_config.cmake            JUCE feature flags and build options
│   └── compiler_flags.cmake         Optional warnings / sanitizers / coverage
├── docs/                            Screenshots and build guide
├── JUCE/                            JUCE 9.0.0, fetched by CMake (git-ignored)
├── phu-audio-lib/                   Shared audio/event library, fetched by CMake (git-ignored)
├── src/
│   ├── PluginProcessor.h/cpp        processBlock, OTT compressor, FIFOs,
│   │                                BeatSyncBuffers, APVTS
│   ├── PluginEditor.h/cpp           UI layout, 60 Hz timer, APVTS attachments
│   ├── CompressorDisplay.h/cpp      Transfer curve + rolling/beat-sync waveform
│   ├── OttCompressor.h              Upward + downward compressor chain
│   ├── CompressorStage.h            One compressor stage (up or down)
│   ├── VolumeDetector.h             RMS / PeakMax rolling-window detector
│   ├── PluginConstants.h            Beat-division tables, buffer size constants
│   └── CMakeLists.txt
├── lib/
│   └── audio/
│       ├── BeatSyncBuffer.h         Position-indexed overwrite buffer
│       ├── BucketSet.h              Dirty-tracked bucket partitioning
│       ├── RmsPacketFifo.h          PPQ-anchored detector level packets
│       ├── PpqRingBuffer.h          PPQ-indexed ring buffer helper
│       └── PacketFifo.h             Generic lock-free packet FIFO
├── .github/workflows/               CI build + pluginval + release workflows
└── scripts/
    ├── build.bat                    Windows convenience build script
    ├── release.bat                  Windows release packaging script
    └── install-linux-deps.sh        Installs JUCE Linux dependencies
```

---

## Building

JUCE and phu-audio-lib are downloaded automatically by CMake during the first configure (network access to GitHub required); there are no submodules to initialise. To work against a local checkout of the shared library, configure with `-DFETCHCONTENT_SOURCE_DIR_PHU-AUDIO-LIB=<path>`; to stop CMake from touching the network on later configures, use `-DFETCHCONTENT_FULLY_DISCONNECTED=ON`.

Presets follow `<os>-<arch>-<format>-<config>` (macOS omits the format and builds VST3 + AU); every configure preset has a matching `-build` preset. Examples: `windows-x64-vst3-release`, `linux-x64-vst3-release`, `macos-arm64-release`. See `cmake --list-presets=all` and the [README](README.md#building).

Optional CMake options (see `cmake/compiler_flags.cmake`): `-DENABLE_STRICT_WARNINGS=ON` for strict compiler warnings, plus the `sanitizers` (Address + UB sanitizers) and `coverage` presets.

---

## Implementation Details

- **Dual-stage OTT chain** — upward stage before downward; both share the same APVTS-wired ballistics
- **Beat-synced detector window** — recomputed each `processBlock` via play-head BPM; fractions 1/32 → 4 beats
- **Transfer curve interaction** — draggable handles on the curve update threshold parameters in real time
- **No blocking on audio thread** — all cross-thread communication uses lock-free FIFOs and atomic reads
