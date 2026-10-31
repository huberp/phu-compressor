#pragma once

#include <cmath>
#include "Constants.h"

/**
 * @file BpmJitterPrevention.h
 * 
 * Unified BPM jitter prevention utilities for phu-compressor.
 * 
 * Many DAW hosts compute BPM as 60.0 / secondsPerBeat each processBlock,
 * which accumulates epsilon-level floating-point variation (< 1e-6 BPM) between
 * calls. Without protection, this noise would trigger unnecessary reinitialization
 * of data structures (detector windows, display durations) on every audio block.
 * 
 * This module provides a unified, millisecond-based approach to prevent such jitter.
 * 
 * Rationale for millisecond-based tolerance:
 * - BPM and milliseconds per beat have a non-linear relationship:
 *   d(ms_per_beat)/d(bpm) = -60000 / bpm^2
 * - A fixed BPM tolerance (e.g., 0.1 BPM) results in different millisecond
 *   changes depending on the BPM value:
 *   - At 60 BPM: 0.1 BPM ≈ 1.67ms
 *   - At 100 BPM: 0.1 BPM ≈ 0.6ms
 *   - At 120 BPM: 0.1 BPM ≈ 0.42ms
 * - A millisecond-based tolerance provides consistent behavior across all BPM ranges.
 * 
 * Typical host jitter is < 0.02 BPM, which at any reasonable BPM translates to
 * < 1.0ms of jitter. A tolerance of 1.0ms is imperceptible musically while
 * being well above typical floating-point noise.
 * 
 * This utility will be moved to phu-audio-lib in a future update for reuse
 * across all phu plugins.
 */

namespace phu::compressor
{

/**
 * Default tolerance in milliseconds for BPM-derived jitter prevention.
 * 
 * 1.0ms is imperceptible musically yet well above typical DAW floating-point
 * jitter (< 0.02 BPM at any reasonable tempo).
 */
inline constexpr float kBpmJitterToleranceMs = 1.0f;

/**
 * Sentinel value returned by bpmToMsPerBeat when bpm <= 0 (invalid/stopped transport).
 * Also used as the initial value for oldBpm to force a refresh on first call.
 */
inline constexpr double kInvalidMsPerBeat = -1.0;

/**
 * Converts BPM to milliseconds per beat.
 * 
 * @param bpm        The BPM value to convert
 * @return           Milliseconds per beat, or kInvalidMsPerBeat if bpm <= 0 (invalid/stopped)
 */
[[nodiscard]] inline double bpmToMsPerBeat(const double bpm) noexcept
{
    return (bpm > 0.0) ? (phu::utility::kMillisecondsPerMinute / bpm) : kInvalidMsPerBeat;
}

/**
 * Checks if a BPM change exceeds the jitter tolerance.
 * 
 * This function compares the milliseconds-per-beat difference between the
 * new and old BPM values against a configurable tolerance. This approach
 * provides consistent behavior across all BPM ranges, unlike a fixed BPM
 * tolerance which has non-linear effects.
 * 
 * Use this for any BPM-derived calculations (detector windows, display durations,
 * etc.) to prevent jitter from host floating-point imprecision.
 * 
 * @param newBpm     The new BPM value
 * @param oldBpm     The previous BPM value
 * @param toleranceMs The jitter tolerance in milliseconds (defaults to kBpmJitterToleranceMs)
 * @return           true if the change exceeds tolerance, false otherwise
 * 
 * @note Pass a negative value as oldBpm (e.g., -1.0) to force a refresh on
 *       the first call (sentinel value).
 */
[[nodiscard]] inline bool bpmChangeExceedsTolerance(
    const double newBpm,
    const double oldBpm,
    const float toleranceMs = kBpmJitterToleranceMs) noexcept
{
    // Force refresh if oldBpm is negative sentinel
    if (oldBpm < 0.0)
        return true;

    // No change if both are equivalent
    if (newBpm == oldBpm)
        return false;

    // Handle stopped/uninitialized cases (bpm <= 0)
    // If transport stops (bpm goes from positive to <= 0) or starts, consider it a real change
    if (newBpm <= 0.0 || oldBpm <= 0.0)
        return newBpm != oldBpm;

    // Calculate milliseconds per beat for both values
    const double newMsPerBeat = phu::utility::kMillisecondsPerMinute / newBpm;
    const double oldMsPerBeat = phu::utility::kMillisecondsPerMinute / oldBpm;

    return std::abs(newMsPerBeat - oldMsPerBeat) > static_cast<double>(toleranceMs);
}

/**
 * Checks if a millisecond value change exceeds the jitter tolerance.
 * 
 * Use this when you already have millisecond values (e.g., detector window sizes)
 * and want to check if the change is significant or just jitter.
 * 
 * @param newMs      The new value in milliseconds
 * @param oldMs      The previous value in milliseconds
 * @param toleranceMs The jitter tolerance in milliseconds (defaults to kBpmJitterToleranceMs)
 * @return           true if the change exceeds tolerance, false otherwise
 * 
 * @note This is the underlying primitive used by bpmChangeExceedsTolerance for
 *       the actual comparison. Exposed for cases where you need to compare
 *       millisecond values directly (e.g., detector window sizes).
 */
[[nodiscard]] inline bool msChangeExceedsTolerance(
    const float newMs,
    const float oldMs,
    const float toleranceMs = kBpmJitterToleranceMs) noexcept
{
    return std::abs(newMs - oldMs) > toleranceMs;
}

} // namespace phu::compressor
