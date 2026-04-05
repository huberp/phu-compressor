#pragma once

#include <cmath>
#include <cstring>
#include <vector>

namespace phu {
namespace audio {

/**
 * WriteResult — describes the buffer region(s) written by a single PpqRingBuffer::insert() call.
 *
 * When no wrap-around occurs, only range1 is valid.
 * When the write wraps around the end of the working buffer, both ranges are valid:
 *   range1 covers [startIdx, workingSize)
 *   range2 covers [0, remainder)
 *
 * Both Range::start and Range::end follow half-open interval convention [start, end),
 * matching BucketSet::markDirty(fromIdx, toIdx) and std::memcpy count semantics.
 */
struct WriteResult {
    struct Range {
        int start = 0;  ///< Inclusive start index.
        int end   = 0;  ///< Exclusive end index.

        /** Returns true when this range contains at least one element. */
        bool valid() const noexcept { return end > start; }
    };

    Range range1;       ///< First (or only) written index range.
    Range range2;       ///< Second written index range (wrap-around tail). Check range2.valid().
    bool  ok = false;   ///< False when the insert was rejected (e.g. count > workingSize).
};

/**
 * PpqRingBuffer — PPQ-position-aware ring buffer for audio samples.
 *
 * The buffer is pre-allocated once at construction time based on worst-case capacity
 * parameters (minBpm, maxSampleRate, maxBeats), so no dynamic allocation occurs in
 * the real-time processBlock path.
 *
 * The ring represents exactly one cycle of numBeats beats.  Each PPQ position maps
 * deterministically to a ring index via:
 *
 *   index = (int)(fmod(ppq, numBeats) / numBeats * workingSize) % workingSize
 *
 * Incoming sample blocks are written with one or two memcpy calls (two when the
 * write wraps around the end of the working buffer).  The returned WriteResult
 * can be passed directly to BucketSet::setDirty() to mark only the affected region.
 *
 * Typical usage:
 * @code
 *   // 1. Construct once with worst-case parameters.
 *   PpqRingBuffer ring(60.0, 96000.0, 4.0);
 *
 *   // 2. Update working size when BPM, sample rate, or beat count changes.
 *   ring.setWorkingSize(currentBpm, currentSampleRate, displayBeats);
 *
 *   // 3. Insert samples from processBlock.
 *   WriteResult r = ring.insert(ppqBlockStart, samples, numSamples);
 *   if (r.ok)
 *       bucketSet.setDirty(r);
 * @endcode
 */
class PpqRingBuffer {
  public:
    /**
     * Construct and pre-allocate the maximum buffer capacity.
     *
     * Capacity = ceil(maxBeats / minBpm * 60.0 * maxSampleRate)
     *
     * @param minBpm         Minimum BPM the ring will ever need to support (e.g. 60.0).
     * @param maxSampleRate  Maximum sample rate in Hz (e.g. 96000.0).
     * @param maxBeats       Maximum number of beats in one display cycle (e.g. 4.0 or 0.25).
     */
    PpqRingBuffer(double minBpm, double maxSampleRate, double maxBeats) {
        if (minBpm > 0.0 && maxSampleRate > 0.0 && maxBeats > 0.0) {
            const int cap =
                static_cast<int>(std::ceil(maxBeats / minBpm * 60.0 * maxSampleRate));
            m_buffer.resize(static_cast<size_t>(cap), 0.0f);
        }
    }

    PpqRingBuffer() = default;

    // Non-copyable to prevent accidental large-buffer copies.
    PpqRingBuffer(const PpqRingBuffer&)            = delete;
    PpqRingBuffer& operator=(const PpqRingBuffer&) = delete;
    PpqRingBuffer(PpqRingBuffer&&)                 = default;
    PpqRingBuffer& operator=(PpqRingBuffer&&)      = default;

    // -------------------------------------------------------------------------
    // Configuration
    // -------------------------------------------------------------------------

    /**
     * Set the working ring size for the current BPM, sample rate, and beat count.
     *
     * The working size is always clamped to the pre-allocated capacity, so this
     * method never allocates memory.  Call this whenever BPM, sample rate, or
     * the display beat count changes.
     *
     * @param bpm         Current BPM (must be > 0).
     * @param sampleRate  Current sample rate in Hz (must be > 0).
     * @param numBeats    Number of beats represented by one full ring cycle (must be > 0).
     * @return True if the working configuration changed, false if unchanged.
     */
    bool setWorkingSize(double bpm, double sampleRate, double numBeats) {
        if (bpm <= 0.0 || sampleRate <= 0.0 || numBeats <= 0.0)
            return false;

        const int newSize =
            static_cast<int>(std::ceil(numBeats / bpm * 60.0 * sampleRate));
        const int clamped = std::min(newSize, capacity());

        if (clamped == m_workingSize && numBeats == m_numBeats)
            return false;

        m_workingSize = clamped;
        m_numBeats    = numBeats;
        return true;
    }

    // -------------------------------------------------------------------------
    // Insert
    // -------------------------------------------------------------------------

    /**
     * Insert samples into the ring at the position determined by @p ppq.
     *
     * The write position is computed as:
     *   startIdx = (int)(fmod(ppq, numBeats) / numBeats * workingSize) % workingSize
     *
     * One or two memcpy calls are used depending on whether the write wraps around
     * the end of the working buffer.  The returned WriteResult carries the written
     * index range(s) for direct use with BucketSet::setDirty().
     *
     * Precondition: setWorkingSize() must have been called at least once.
     *
     * @param ppq      PPQ position of the first sample in the block.
     * @param samples  Pointer to the source samples.  Must not be null.
     * @param count    Number of samples to write.
     * @return         WriteResult with ok == true on success, ok == false if the
     *                 block is larger than the working ring size (count > workingSize).
     */
    WriteResult insert(double ppq, const float* samples, int count) {
        WriteResult result;

        if (m_workingSize <= 0 || m_numBeats <= 0.0 || samples == nullptr || count <= 0)
            return result;  // ok remains false

        if (count > m_workingSize) {
            // The block is larger than the ring — cannot insert without overwriting
            // data that has not yet wrapped. Return an error result.
            return result;  // ok remains false
        }

        // Map PPQ position to a ring index.
        double ppqMod = std::fmod(ppq, m_numBeats);
        if (ppqMod < 0.0)
            ppqMod += m_numBeats;
        int startIdx =
            static_cast<int>(ppqMod / m_numBeats * static_cast<double>(m_workingSize));
        if (startIdx >= m_workingSize)
            startIdx = m_workingSize - 1;  // guard against floating-point overshoot

        const int endIdx = startIdx + count;  // exclusive

        if (endIdx <= m_workingSize) {
            // Common case: no wrap-around — single memcpy.
            std::memcpy(m_buffer.data() + startIdx,
                        samples,
                        static_cast<size_t>(count) * sizeof(float));
            result.range1 = {startIdx, endIdx};
        } else {
            // Wrap-around: write tail of ring, then beginning.
            const int firstPart  = m_workingSize - startIdx;
            const int secondPart = count - firstPart;

            std::memcpy(m_buffer.data() + startIdx,
                        samples,
                        static_cast<size_t>(firstPart) * sizeof(float));
            std::memcpy(m_buffer.data(),
                        samples + firstPart,
                        static_cast<size_t>(secondPart) * sizeof(float));

            result.range1 = {startIdx, m_workingSize};
            result.range2 = {0, secondPart};
        }

        result.ok = true;
        return result;
    }

    // -------------------------------------------------------------------------
    // Accessors
    // -------------------------------------------------------------------------

    /** Read-only access to the raw buffer. Size is capacity(), not workingSize(). */
    const float* data() const noexcept { return m_buffer.data(); }

    /** Pre-allocated capacity in samples (fixed at construction). */
    int capacity() const noexcept { return static_cast<int>(m_buffer.size()); }

    /** Current working ring size in samples (≤ capacity()). */
    int workingSize() const noexcept { return m_workingSize; }

    /** Current number of beats represented by one full ring cycle. */
    double numBeats() const noexcept { return m_numBeats; }

  private:
    std::vector<float> m_buffer;
    int                m_workingSize = 0;
    double             m_numBeats    = 0.0;
};

} // namespace audio
} // namespace phu
