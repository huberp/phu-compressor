# PHU COMPRESSOR

[![Build](https://github.com/huberp/phu-compressor/actions/workflows/build.yml/badge.svg)](https://github.com/huberp/phu-compressor/actions/workflows/build.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![VST3](https://img.shields.io/badge/format-VST3-blue)](https://steinbergmedia.github.io/vst3_doc/)
[![JUCE](https://img.shields.io/badge/built%20with-JUCE-orange)](https://juce.com/)

A free, open-source **OTT-style dual-band compressor** VST3 plugin built with JUCE.  
Combines classic **downward compression** (tame loud signals) with **upward compression**
(lift quiet signals) in a single, lightweight processor — perfect for glue compression,
mastering levelling, or parallel pumping effects.

---

## Table of Contents

- [Features](#features)
- [Parameters](#parameters)
- [Installation](#installation)
  - [Pre-built Binaries](#pre-built-binaries)
  - [Supported Platforms](#supported-platforms)
- [Building from Source](#building-from-source)
  - [Windows](#windows)
  - [Linux](#linux)
  - [CMake Presets](#cmake-presets)
- [Project Structure](#project-structure)
- [Contributing](#contributing)
- [License](#license)
- [Acknowledgements](#acknowledgements)

---

## Features

- **Dual-mode compression** — independent downward *and* upward compressor in one plugin
- **OTT-style gain computation** — ratio-based power-law mapping with smooth ballistic envelopes
- **Per-channel stereo processing** — left and right channels processed independently
- **Zero-latency** — no look-ahead; tail length is zero
- **Preset state save/restore** — full DAW automation and session recall via APVTS
- **Lightweight** — no web browser, no CURL, no unnecessary JUCE modules
- **FOSS** — MIT licensed; source included, forks welcome

---

## Parameters

| Parameter | ID | Range | Default | Description |
|---|---|---|---|---|
| Down Threshold | `down_thresh` | −60 … 0 dB | −12 dB | Level above which downward compression engages |
| Down Ratio | `down_ratio` | 1 … 20 | 4 | Downward compression ratio (N:1) |
| Up Threshold | `up_thresh` | −60 … 0 dB | −30 dB | Level below which upward compression engages |
| Up Ratio | `up_ratio` | 1 … 20 | 4 | Upward compression ratio (N:1) |
| Attack | `attack_ms` | 0.1 … 200 ms | 10 ms | Attack time for both compressors |
| Release | `release_ms` | 1 … 2 000 ms | 100 ms | Release time for both compressors |

All parameters are fully automatable and saved with the DAW project.

---

## Installation

### Pre-built Binaries

Download the latest pre-built VST3 from the
[**Releases**](https://github.com/huberp/phu-compressor/releases) page.

#### Windows

1. Download `PHU-COMPRESSOR-windows.zip` from the latest release.
2. Extract and copy `PHU COMPRESSOR.vst3` to your VST3 folder:
   ```
   C:\Program Files\Common Files\VST3\
   ```
3. Rescan plugins in your DAW.

#### Linux

1. Download `PHU-COMPRESSOR-linux.zip` from the latest release.
2. Extract and copy `PHU COMPRESSOR.vst3` to:
   ```bash
   ~/.vst3/
   # or system-wide:
   /usr/lib/vst3/
   ```
3. Rescan plugins in your DAW.

### Supported Platforms

| Platform | Architecture | Format |
|---|---|---|
| Windows 10 / 11 | x64 | VST3 |
| Ubuntu 22.04+ / Debian | x64 | VST3 |
| Other Linux | x64 | VST3 (build from source) |

> **macOS** is not yet officially supported (contributions welcome — see [Contributing](#contributing)).

---

## Building from Source

### Prerequisites

- **Git** (with submodule support)
- **CMake ≥ 3.23**
- **C++17-capable compiler** (MSVC 2022+ on Windows, GCC 11+ / Clang 13+ on Linux)

Clone the repository *with submodules* (JUCE is a submodule):

```bash
git clone --recurse-submodules https://github.com/huberp/phu-compressor.git
cd phu-compressor
```

If you already cloned without `--recurse-submodules`:

```bash
git submodule update --init --recursive
```

---

### Windows

#### Quick build (batch script)

```bat
scripts\build.bat
```

The script configures and builds a Release VST3 using the `vs2026-x64` CMake preset.
The output is placed in:
```
build\vs2026-x64\src\phu-compressor_artefacts\Release\VST3\PHU COMPRESSOR.vst3
```

#### Create a release package

```bat
scripts\release.bat
```

This builds Release and zips the VST3 into `dist\PHU-COMPRESSOR-windows.zip`.

#### Manual steps

```bat
cmake --preset vs2026-x64
cmake --build --preset release
```

---

### Linux

Install system dependencies first (Ubuntu / Debian):

```bash
sudo bash scripts/install-linux-deps.sh
```

Then build:

```bash
cmake --preset linux-release
cmake --build --preset linux-build
```

The built plugin is at:
```
build/linux-release/src/phu-compressor_artefacts/Release/VST3/PHU COMPRESSOR.vst3/
```

Install to your user VST3 folder:

```bash
cp -r "build/linux-release/src/phu-compressor_artefacts/Release/VST3/PHU COMPRESSOR.vst3" \
    ~/.vst3/
```

For detailed Linux instructions see [docs/LINUX_BUILD.md](docs/LINUX_BUILD.md).

---

### CMake Presets

| Preset | Platform | Description |
|---|---|---|
| `vs2026-x64` | Windows | Visual Studio 2026, x64 |
| `linux-release` | Linux | Unix Makefiles, Release |
| `release` (build) | Windows | Release build targeting `phu-compressor_VST3` |
| `debug` (build) | Windows | Debug build |
| `linux-build` (build) | Linux | Release build |

---

## Project Structure

```
phu-compressor/
├── src/
│   ├── OttCompressor.h        # Core DSP — OTT-style compressor (header-only)
│   ├── PluginProcessor.cpp/.h # JUCE AudioProcessor: parameter setup & audio routing
│   ├── PluginEditor.cpp/.h    # JUCE AudioProcessorEditor: slider UI
│   └── CMakeLists.txt         # Plugin target definition
├── JUCE/                      # JUCE framework (git submodule)
├── scripts/
│   ├── build.bat              # Windows quick-build script
│   ├── release.bat            # Windows release-package script
│   └── install-linux-deps.sh  # Linux dependency installer
├── docs/
│   └── LINUX_BUILD.md         # Detailed Linux build guide
├── .github/
│   └── workflows/
│       ├── build.yml          # CI: build + pluginval on every push/PR
│       └── release.yml        # CD: publish a GitHub Release on version tags
├── CMakeLists.txt
├── CMakePresets.json
└── LICENSE                    # MIT
```

---

## Contributing

Contributions, bug reports and feature requests are welcome!

1. **Fork** the repository.
2. **Create a branch**: `git checkout -b feature/my-feature`
3. **Commit** your changes with a clear message.
4. **Open a Pull Request** against `main`.

Please keep code style consistent with the existing `.clang-format` configuration
(`BasedOnStyle: Google`, 100-column line limit).

Ideas for future improvements:

- macOS / AU support
- Gain reduction meter in the UI
- Mid/Side processing mode
- Multiband (phu-splitter integration)

---

## License

[MIT License](LICENSE) — © 2026 huberp

You are free to use, modify, and distribute this plugin in both open-source and commercial
projects. Attribution is appreciated but not required.

---

## Acknowledgements

- [**JUCE**](https://juce.com/) — cross-platform C++ audio framework (ISC / GPL)
- [**pluginval**](https://github.com/Tracktion/pluginval) — plugin validation used in CI
- Inspired by the classic **OTT** multiband compressor preset
