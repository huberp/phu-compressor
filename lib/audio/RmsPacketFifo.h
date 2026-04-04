#pragma once

#include <cstring>
#include <juce_core/juce_core.h>

namespace phu {
namespace audio {

/// Maximum number of per-sample detector RMS values that fit in one RmsPacket.
/// Sized for kRmsAccumBlocks (4) blocks × 8192 samples/block (worst-case block size).
static constexpr int kRmsMaxPacketSamples = 4 * 8192;  // 32768

/// Number of packet slots in the ring pool.
static constexpr int kRmsPacketPoolSlots = 8;

/**
 * A batch of per-sample detector RMS values produced by the audio thread,
 * together with the PPQ position of the first sample in the batch.
 *
 * The audio thread fills this via RmsPacketFifo::push(); the UI thread
 * consumes it via RmsPacketFifo::pull().
 */
struct RmsPacket {
    double startPpq = 0.0;
    int    count    = 0;
    float  data[kRmsMaxPacketSamples] = {};
};

/**
 * Lock-free single-writer / single-reader FIFO transporting RmsPacket objects
 * from the audio thread to the UI thread.
 *
 * Uses juce::AbstractFifo over a fixed-size pool of RmsPacket slots.
 * push() is called on the audio thread; pull() is called on the UI thread.
 */
class RmsPacketFifo {
  public:
    RmsPacketFifo() : fifo_(kRmsPacketPoolSlots) {}

    void reset() {
        fifo_.reset();
    }

    /**
     * Push a packet from the audio thread.
     * @return false if the FIFO is full (packet is silently dropped).
     */
    bool push(double startPpq, const float* data, int count) {
        count = juce::jmin(count, kRmsMaxPacketSamples);
        if (count <= 0) return false;

        const auto scope = fifo_.write(1);
        if (scope.blockSize1 + scope.blockSize2 == 0)
            return false;  // full

        const int slot = scope.blockSize1 > 0 ? scope.startIndex1 : scope.startIndex2;
        pool_[static_cast<size_t>(slot)].startPpq = startPpq;
        pool_[static_cast<size_t>(slot)].count    = count;
        std::memcpy(pool_[static_cast<size_t>(slot)].data, data,
                    static_cast<size_t>(count) * sizeof(float));
        return true;
    }

    /**
     * Pull a packet on the UI thread.
     * @return false if the FIFO is empty.
     */
    bool pull(RmsPacket& out) {
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
    RmsPacket          pool_[kRmsPacketPoolSlots];
};

} // namespace audio
} // namespace phu
