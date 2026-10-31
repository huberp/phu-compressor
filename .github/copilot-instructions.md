# Instructions for GitHub Copilot

## Building This Project

This is a JUCE-based audio plugin project that requires specific dependencies on Linux.

### Before Building on Linux

1. **Install dependencies first**: Run `sudo bash scripts/install-linux-deps.sh`
2. **Dependencies are fetched by CMake**: JUCE and phu-audio-lib are downloaded into `JUCE/` and `phu-audio-lib/` during configure (see `cmake/PhuDependencies.cmake`); no submodule init is needed
3. **Use the Linux preset**: Build with `cmake --preset linux-x64-vst3-release && cmake --build --preset linux-x64-vst3-release-build`

### Build Timeouts

If builds timeout:
- Use fewer parallel jobs: `cmake --build --preset linux-x64-vst3-release-build -j2`
- Ensure all dependencies are installed before attempting to build
- The JUCE and phu-audio-lib fetch must complete (network access to GitHub is required on first configure)

### Documentation

- Main README: [README.md](./README.md)
- Linux build guide: [docs/LINUX_BUILD.md](./docs/LINUX_BUILD.md)
- JUCE Linux dependencies: `JUCE/docs/Linux Dependencies.md` (available after the first configure, once CMake has fetched JUCE)


