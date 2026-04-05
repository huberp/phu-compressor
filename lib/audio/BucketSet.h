#pragma once

#include <algorithm>
#include <functional>
#include <iterator>
#include <vector>

namespace phu {
namespace audio {

/**
 * Bucket — a half-open index range [startIdx, endIdx) into a conceptual buffer [0, N),
 * together with a dirty flag indicating that the cached computation result is stale.
 */
struct Bucket {
    int  startIdx = 0;    ///< Inclusive start index.
    int  endIdx   = 0;    ///< Exclusive end index.
    bool dirty    = true; ///< True when the samples in this range have changed.
};

/**
 * BucketSet — partitions a buffer [0, N) into B contiguous, non-overlapping Bucket ranges.
 *
 * Partitioning modes:
 *
 *   Fixed-count (primary mode)
 *     A desired bucket count B is specified; boundaries are computed as
 *     (i * N) / B for i in [0, B], which guarantees:
 *       - All buckets cover [0, N) with no gaps or overlaps.
 *       - Bucket sizes differ by at most 1 (integer division of N / B).
 *       - Last bucket ends exactly at N.
 *     If B > N it is clamped to N so that every bucket contains at least one index.
 *     Use initializeBySize(), initializeByVector(), or initializeBySizeFn().
 *
 *   Bound-buffer mode
 *     BucketSet stores a size provider (std::function<int()>) and a target bucket
 *     count.  Calling recompute() re-reads the current size and rebuilds buckets.
 *     Useful when the underlying buffer can change size over time (e.g. ring buffer
 *     resized when BPM or sample rate changes).
 *
 * Dirty marking:
 *   Call markDirtyRange(u1, u2) when ring-buffer positions u1..u2 (inclusive) are
 *   written.  Handles ring wrap (u1 > u2).
 *   Iterate over only dirty buckets with dirtyBegin() / dirtyEnd().
 *
 * Note for juce::AudioBuffer callers:
 *   Use initializeBySize(buf.getNumSamples(), bucketCount) — no adapter needed.
 */
class BucketSet {
  public:
    BucketSet() = default;

    // -------------------------------------------------------------------------
    // Initialization
    // -------------------------------------------------------------------------

    /**
     * Partition [0, bufferSize) into bucketCount buckets.
     *
     * If bucketCount > bufferSize it is clamped to bufferSize (no empty buckets).
     * If bufferSize <= 0 the bucket list is cleared.
     * All buckets are marked dirty after this call.
     *
     * @param bufferSize   Total number of addressable indices in the buffer.
     * @param bucketCount  Desired number of buckets (e.g. display width in pixels).
     */
    void initializeBySize(int bufferSize, int bucketCount) {
        m_sizeFn            = nullptr;
        m_bufferSize        = bufferSize;
        m_bucketCountTarget = bucketCount;
        rebuild(bufferSize, bucketCount);
    }

    /**
     * Partition [0, vec.size()) into bucketCount buckets.
     *
     * Convenience wrapper for std::vector (or any container with a size() method).
     */
    template<typename T>
    void initializeByVector(const std::vector<T>& vec, int bucketCount) {
        initializeBySize(static_cast<int>(vec.size()), bucketCount);
    }

    /**
     * Store a size provider and partition immediately using the current size.
     *
     * The sizeFn is called once now to build the initial buckets, and again on
     * every subsequent recompute() call.  This allows bucket boundaries to track
     * a buffer that is resized over time without the caller passing the size each time.
     *
     * Example — ring buffer whose length changes with BPM:
     *   bucketSet.initializeBySizeFn([&ch](){ return ch.rmsRingSize; }, 512);
     *   // later, after ring is resized:
     *   bucketSet.recompute();
     *
     * Example — juce::AudioBuffer:
     *   bucketSet.initializeBySizeFn([&buf](){ return buf.getNumSamples(); }, 256);
     *
     * @param sizeFn       Callable returning the current buffer size as int.
     * @param bucketCount  Desired number of buckets.
     */
    void initializeBySizeFn(std::function<int()> sizeFn, int bucketCount) {
        m_sizeFn            = sizeFn;
        m_bucketCountTarget = bucketCount;
        const int sz        = sizeFn ? sizeFn() : 0;
        m_bufferSize        = sz;
        rebuild(sz, bucketCount);
    }

    /**
     * Recompute bucket boundaries using the current buffer size.
     *
     * If a size provider was registered with initializeBySizeFn(), it is called
     * to obtain the up-to-date size.  Otherwise the last known buffer size is used.
     *
     * Call this after the underlying buffer has been resized.
     */
    void recompute() {
        const int sz = m_sizeFn ? m_sizeFn() : m_bufferSize;
        m_bufferSize = sz;
        rebuild(sz, m_bucketCountTarget);
    }

    // -------------------------------------------------------------------------
    // Dirty marking
    // -------------------------------------------------------------------------

    /**
     * Mark every bucket whose range overlaps [fromIdx, toIdx) as dirty.
     *
     * Linear scan — O(buckets in range).  Prefer markDirtyIndex() for single writes.
     *
     * @param fromIdx  Inclusive start of the written region.
     * @param toIdx    Exclusive end of the written region.
     */
    void markDirty(int fromIdx, int toIdx) {
        for (auto& b : m_buckets) {
            if (b.startIdx < toIdx && b.endIdx > fromIdx)
                b.dirty = true;
        }
    }

    /**
     * Mark the single bucket containing writeIdx as dirty.  O(1).
     *
     * @param writeIdx  The buffer index that was just written.
     */
    void markDirtyIndex(int writeIdx) {
        if (m_buckets.empty()) return;
        m_buckets[static_cast<size_t>(findBucket(writeIdx))].dirty = true;
    }

    /**
     * Mark every bucket touched by a contiguous ring-buffer write from u1 to u2
     * (both inclusive).  Handles the wrap-around case where u1 > u2.
     *
     * No-wrap  (u1 <= u2): marks buckets covering [u1 .. u2]
     * Wrap     (u1 >  u2): marks buckets covering [u1 .. last] AND [0 .. u2]
     *
     * Complexity: O(number of buckets touched) — typically O(1) for a short
     * packet, O(bucketCount) at worst for a full-buffer wrap.
     *
     * @param u1  Buffer index of the first written sample (inclusive).
     * @param u2  Buffer index of the last  written sample (inclusive).
     */
    void markDirtyRange(int u1, int u2) {
        if (m_buckets.empty()) return;
        const int last = static_cast<int>(m_buckets.size()) - 1;
        const int i1   = findBucket(u1);
        const int i2   = findBucket(u2);

        if (i1 <= i2) {
            // Common case: no ring wrap, one contiguous bucket range.
            for (int i = i1; i <= i2; ++i)
                m_buckets[static_cast<size_t>(i)].dirty = true;
        } else {
            // Ring wrapped: [i1 .. last] and [0 .. i2].
            for (int i = i1; i <= last; ++i)
                m_buckets[static_cast<size_t>(i)].dirty = true;
            for (int i = 0;  i <= i2;  ++i)
                m_buckets[static_cast<size_t>(i)].dirty = true;
        }
    }

    // -------------------------------------------------------------------------
    // Dirty-bucket iteration
    // -------------------------------------------------------------------------

    /**
     * Forward iterator that yields only Bucket entries whose dirty flag is true.
     */
    class DirtyIterator {
      public:
        using iterator_category = std::forward_iterator_tag;
        using value_type        = Bucket;
        using difference_type   = std::ptrdiff_t;
        using pointer           = Bucket*;
        using reference         = Bucket&;

        DirtyIterator(std::vector<Bucket>::iterator cur,
                      std::vector<Bucket>::iterator end) noexcept
            : m_cur(cur), m_end(end) {
            skipClean();
        }

        reference  operator*()  const noexcept { return *m_cur; }
        pointer    operator->() const noexcept { return &*m_cur; }

        DirtyIterator& operator++() noexcept {
            ++m_cur;
            skipClean();
            return *this;
        }

        bool operator==(const DirtyIterator& o) const noexcept { return m_cur == o.m_cur; }
        bool operator!=(const DirtyIterator& o) const noexcept { return m_cur != o.m_cur; }

      private:
        void skipClean() noexcept {
            while (m_cur != m_end && !m_cur->dirty)
                ++m_cur;
        }

        std::vector<Bucket>::iterator m_cur;
        std::vector<Bucket>::iterator m_end;
    };

    /** Iterator to the first dirty bucket (or dirtyEnd() if none). */
    DirtyIterator dirtyBegin() { return {m_buckets.begin(), m_buckets.end()}; }

    /** Past-the-end sentinel for the dirty iterator. */
    DirtyIterator dirtyEnd()   { return {m_buckets.end(),   m_buckets.end()}; }

    // -------------------------------------------------------------------------
    // Accessors
    // -------------------------------------------------------------------------

    /** Total number of buckets (dirty and clean). */
    int bucketCount() const { return static_cast<int>(m_buckets.size()); }

    /** Direct access to a bucket by index. */
    const Bucket& bucket(int i) const { return m_buckets[static_cast<size_t>(i)]; }
    Bucket&       bucket(int i)       { return m_buckets[static_cast<size_t>(i)]; }

    /** Read-only access to the full bucket vector. */
    const std::vector<Bucket>& buckets() const { return m_buckets; }

    /** The buffer size N used in the last initialization or recompute(). */
    int bufferSize() const { return m_bufferSize; }

  private:
    /**
     * Rebuild bucket boundaries using integer division (i * N) / B for i in [0, B].
     *
     * Uses long long arithmetic to avoid overflow when N and B are large.
     * Guarantees:
     *   - bucket(0).startIdx == 0
     *   - bucket(B-1).endIdx == N
     *   - contiguous: bucket(i).endIdx == bucket(i+1).startIdx
     *   - non-empty: startIdx < endIdx  (enforced by clamping B to N)
     */
    void rebuild(int N, int B) {
        m_buckets.clear();
        if (N <= 0 || B <= 0)
            return;

        // Clamp so every bucket holds at least one index.
        const int count = std::min(B, N);
        m_buckets.reserve(static_cast<size_t>(count));

        for (int i = 0; i < count; ++i) {
            const int start = static_cast<int>(static_cast<long long>(i)     * N / count);
            const int end   = static_cast<int>(static_cast<long long>(i + 1) * N / count);
            m_buckets.push_back({start, end, true});
        }
    }

    /**
     * Return the index of the bucket containing buffer position idx.  O(1).
     *
     * Uses the inverse of the (i * N) / B boundary formula:
     *   bucket(i) covers [(i*N)/B, ((i+1)*N)/B)
     *   => bucket containing idx = (idx * B) / N, clamped to [0, B-1].
     * Long long arithmetic prevents overflow for large N and B.
     */
    int findBucket(int idx) const {
        if (m_bufferSize <= 0 || m_buckets.empty()) return 0;
        const int B = static_cast<int>(m_buckets.size());
        int bi = static_cast<int>(static_cast<long long>(idx) * B / m_bufferSize);
        if (bi < 0)  bi = 0;
        if (bi >= B) bi = B - 1;
        return bi;
    }

    std::function<int()> m_sizeFn;
    int                  m_bufferSize        = 0;
    int                  m_bucketCountTarget = 0;
    std::vector<Bucket>  m_buckets;
};

} // namespace audio
} // namespace phu
