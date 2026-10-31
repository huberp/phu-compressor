#pragma once

#include "audio/PacketFifo.h"

namespace phu {
namespace audio {

/// Maximum number of per-sample detector RMS values that fit in one RmsPacket.
/// Sized for kRmsAccumBlocks (4) blocks × 8192 samples/block (worst-case block size).
static constexpr int kRmsMaxPacketSamples = 4 * 8192;  // 32768

/// Number of packet slots in the ring pool.
static constexpr int kRmsPacketPoolSlots = 8;

/// A batch of per-sample detector RMS values with the PPQ position of the first sample.
using RmsPacket = TimedBufferPacket<float, kRmsMaxPacketSamples>;

/// Lock-free SPSC FIFO transporting RmsPacket objects from the audio thread to the UI thread.
using RmsPacketFifo = PacketFifo<RmsPacket, kRmsPacketPoolSlots>;

} // namespace audio
} // namespace phu
