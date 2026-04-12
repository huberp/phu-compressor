# Contributing to PHU Compressor

Contributions are welcome.

1. Fork and branch from `main`
2. Follow existing C++17/JUCE code style
3. No memory allocation, system calls, or locks on the audio thread
4. Verify the project builds and passes pluginval before opening a PR

**Bug reports** — please include DAW name/version, OS, and reproduction steps in a [GitHub Issue](https://github.com/huberp/phu-compressor/issues).

---

## Project Layout

```
phu-compressor/
├── CMakeLists.txt / CMakePresets.json
├── docs/                            Screenshots and build guide
├── JUCE/                            JUCE 8.0.12 (git submodule)
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
│   ├── audio/
│   │   ├── AudioSampleFifo.h        Lock-free SPSC FIFO (audio→UI)
│   │   ├── BeatSyncBuffer.h         Position-indexed overwrite buffer
│   │   ├── BucketSet.h              Dirty-tracked bucket partitioning
│   │   ├── RmsPacketFifo.h          PPQ-anchored detector level packets
│   │   ├── PpqRingBuffer.h          PPQ-indexed ring buffer helper
│   │   └── PacketFifo.h             Generic lock-free packet FIFO
│   └── events/
│       ├── SyncGlobals.h            BPM / PPQ / transport state
│       ├── SyncGlobalsListener.h    Event listener interface
│       └── Event.h / EventSource.h  Typed event infrastructure
├── .github/workflows/               CI build + pluginval + release workflows
└── scripts/
    ├── build.bat                    Windows convenience build script
    ├── release.bat                  Windows release packaging script
    └── install-linux-deps.sh        Installs JUCE Linux dependencies
```

---

## Implementation Details

- **Dual-stage OTT chain** — upward stage before downward; both share the same APVTS-wired ballistics
- **Beat-synced detector window** — recomputed each `processBlock` via play-head BPM; fractions 1/32 → 4 beats
- **Transfer curve interaction** — draggable handles on the curve update threshold parameters in real time
- **No blocking on audio thread** — all cross-thread communication uses lock-free FIFOs and atomic reads
