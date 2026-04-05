#pragma once

#include <algorithm>
#include <cstring>
#include <juce_core/juce_core.h>

namespace phu {
namespace audio {

/**
 * A time-stamped batch of samples anchored by a PPQ position.
 *
 * @tparam SampleT  Element type (e.g. float, double, int16_t).
 * @tparam MaxCount Maximum number of elements in one packet.
 */
template <typename SampleT, int MaxCount>
struct TimedBufferPacket {
    double  startPpq       = 0.0;
    int     count          = 0;
    SampleT data[MaxCount] = {};

    /**
     * Populate the packet from a raw buffer.
     * @param ppq    PPQ position of the first sample.
     * @param src    Pointer to source sample data.
     * @param n      Number of samples to copy (clamped to MaxCount).
     */
    void set(double ppq, const SampleT* src, int n) {
        startPpq = ppq;
        count    = std::min(n, MaxCount);
        std::memcpy(data, src, static_cast<size_t>(count) * sizeof(SampleT));
    }
};

/**
 * Lock-free single-writer / single-reader FIFO for transporting packets
 * between threads (e.g. audio thread → UI thread).
 *
 * Uses juce::AbstractFifo over a fixed-size pool of PacketT slots.
 * push() is called on the writer thread; pull() is called on the reader thread.
 *
 * @tparam PacketT   The packet type to transport.
 * @tparam PoolSlots Number of packet slots in the internal pool.
 */
template <typename PacketT, int PoolSlots>
class PacketFifo {
  public:
    PacketFifo() : fifo_(PoolSlots) {}

    void reset() { fifo_.reset(); }

    /**
     * Push a packet from the writer thread.
     * @return false if the FIFO is full (packet is silently dropped).
     */
    bool push(const PacketT& packet) {
        const auto scope = fifo_.write(1);
        if (scope.blockSize1 + scope.blockSize2 == 0)
            return false;  // full

        const int slot = scope.blockSize1 > 0 ? scope.startIndex1 : scope.startIndex2;
        pool_[static_cast<size_t>(slot)] = packet;
        return true;
    }

    /**
     * Pull a packet on the reader thread.
     * @return false if the FIFO is empty.
     */
    bool pull(PacketT& out) {
        const auto scope = fifo_.read(1);
        if (scope.blockSize1 + scope.blockSize2 == 0)
            return false;  // empty

        const int slot = scope.blockSize1 > 0 ? scope.startIndex1 : scope.startIndex2;
        out = pool_[static_cast<size_t>(slot)];
        return true;
    }

    int getNumReady() const { return fifo_.getNumReady(); }

  private:
    juce::AbstractFifo fifo_;
    PacketT            pool_[PoolSlots];
};

} // namespace audio
} // namespace phu
