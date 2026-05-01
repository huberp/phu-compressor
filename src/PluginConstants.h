#pragma once

/**
 * PluginConstants — single source of truth for all magic numbers that must stay
 * consistent across the processor, editor, and DSP components.
 *
 * Two entirely separate beat-division tables are defined here:
 *
 *   kDetector*  — detector RMS/PeakMax window in BPM-sync mode.
 *                 Ranges from sub-beat (1/32) to multi-beat (4 bars), covering
 *                 everything from fast transient response to very smooth levelling.
 *
 *   kDisplay*   — waveform/GR display range in the rolling and beat-sync views.
 *                 Whole-beat multiples only: 1, 2, 4, 8 beats.
 *
 * These two tables are intentionally kept SEPARATE. The detector window setting
 * and the display range setting are independent user controls with different
 * semantic meaning and different value ranges. Do not conflate them.
 *
 * kDetectorMaxWindowMs is derived from first principles so VolumeDetector's ring
 * buffer is always exactly large enough for any valid setting without magic numbers.
 */

// ── System limits ─────────────────────────────────────────────────────────────

/// Highest supported sample rate (for buffer pre-allocation).
static constexpr double kMaxSampleRate = 192000.0;

/// Maximum lookahead time in milliseconds (determines delay-line buffer size).
static constexpr double kMaxLookaheadMs = 20.0;

/// Lowest supported tempo — determines maximum detector window length.
static constexpr double kMinBPM = 40.0;

// ── Channel / buffer constants ─────────────────────────────────────────────────

/// Number of stereo channels processed throughout the plugin.
static constexpr int kNumStereoChannels = 2;

/// Extra headroom added to temp buffers in prepareToPlay() to avoid
/// reallocation on the audio thread when block size increases slightly.
static constexpr int kPrepareBufferHeadroom = 8192;

/// Number of position-indexed bins in the beat-sync display buffers.
static constexpr int kBeatSyncBufferBins = 4096;

// ── Detector parameter ranges ─────────────────────────────────────────────────

/// Minimum detector window length in milliseconds.
static constexpr float kDetectorMinWindowMs = 1.0f;

/// APVTS stores booleans as floats; values ≥ this threshold are treated as true.
static constexpr float kRmsSyncToggleThreshold = 0.5f;

// ── Fallback defaults ─────────────────────────────────────────────────────────

/// Fallback sample rate used before the DAW calls prepareToPlay().
static constexpr double kFallbackSampleRate = 48000.0;

/// Fallback tempo used when no BPM information is available from the DAW.
static constexpr double kFallbackBPM = 120.0;

// ── Display dB conventions ────────────────────────────────────────────────────

/// Minimum linear amplitude treated as non-zero for dB conversion (avoids log(0)).
static constexpr float kLinearNoiseFloor = 1.0e-10f;

/// Bottom of the dB scale for waveform and transfer-curve displays.
static constexpr float kDisplayMinDb = -60.0f;

/// Top of the dB scale for waveform and transfer-curve displays.
static constexpr float kDisplayMaxDb = 0.0f;

/// Maximum gain-reduction / upward-boost depth shown in the UI (dB).
static constexpr float kDisplayGrMaxDb = 24.0f;

// ── Detector beat divisions ───────────────────────────────────────────────────
// Governs the RMS/PeakMax detector window length when BPM-sync mode is active.
// MUST stay consistent with: rmsBeatDivCombo item order, kParamRmsBeatDiv choice list.

static constexpr int kDetectorNumDivisions = 8;

static constexpr float kDetectorBeatFractions[kDetectorNumDivisions] = {
    0.03125f,  // 1/32
    0.0625f,   // 1/16
    0.125f,    // 1/8
    0.25f,     // 1/4
    0.5f,      // 1/2
    1.0f,      // 1
    2.0f,      // 2
    4.0f,      // 4
};

static constexpr const char* kDetectorBeatLabels[kDetectorNumDivisions] = {
    "1/32", "1/16", "1/8", "1/4", "1/2", "1", "2", "4"
};

// ── Display range options ─────────────────────────────────────────────────────
// Governs how many beats the rolling waveform/GR display covers.
// MUST stay consistent with: CompressorDisplay time buttons.

static constexpr int kDisplayNumRanges = 4;

static constexpr float kDisplayBeatFractions[kDisplayNumRanges] = {
    1.0f, 2.0f, 4.0f, 8.0f
};

static constexpr const char* kDisplayBeatLabels[kDisplayNumRanges] = {
    "1", "2", "4", "8"
};

// ── Derived: maximum detector window ─────────────────────────────────────────
// Worst case: the longest detector beat fraction (4 beats) at the lowest
// supported BPM (40) gives the maximum window in milliseconds:
//   kDetectorMaxWindowMs = (4.0 / 40.0) * 60000.0 = 6000.0 ms
//
// VolumeDetector uses this to size its ring buffer at prepare() time based on
// the actual sample rate — no hardcoded sample counts anywhere.

static constexpr double kDetectorMaxBeatFraction =
    kDetectorBeatFractions[kDetectorNumDivisions - 1];             // = 4.0

static constexpr double kDetectorMaxWindowMs =
    (kDetectorMaxBeatFraction / kMinBPM) * 60000.0;                // = 6000.0 ms

// ── Derived: maximum display range ───────────────────────────────────────────

/// Largest display beat fraction — used to pre-allocate the detector RMS ring buffer.
static constexpr double kDisplayMaxBeatFraction =
    static_cast<double>(kDisplayBeatFractions[kDisplayNumRanges - 1]);  // = 8.0

// ── Display RMS ring buffer capacity ─────────────────────────────────────────
// Worst case: 8 beats / 40 BPM × 60 s × 192000 Hz = 2,304,000 samples.
// The detector RMS ring buffer is sized to this maximum so it can hold exactly
// one full display window of per-sample dB values at any supported configuration.

static constexpr int kDetRmsRingMaxSize =
    static_cast<int>(static_cast<double>(kDisplayBeatFractions[kDisplayNumRanges - 1])
                     / kMinBPM * 60.0 * kMaxSampleRate) + 1024;   // = 2,305,024
