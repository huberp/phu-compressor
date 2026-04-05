# Compressor support library (header-only)
add_library(CompressorLib INTERFACE)

target_include_directories(CompressorLib INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_sources(CompressorLib INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/audio/AudioSampleFifo.h
    ${CMAKE_CURRENT_SOURCE_DIR}/audio/BeatSyncBuffer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/audio/BucketSet.h
    ${CMAKE_CURRENT_SOURCE_DIR}/audio/PacketFifo.h
    ${CMAKE_CURRENT_SOURCE_DIR}/audio/PpqRingBuffer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/audio/RmsPacketFifo.h
    ${CMAKE_CURRENT_SOURCE_DIR}/events/Event.h
    ${CMAKE_CURRENT_SOURCE_DIR}/events/EventSource.h
    ${CMAKE_CURRENT_SOURCE_DIR}/events/SyncGlobals.h
    ${CMAKE_CURRENT_SOURCE_DIR}/events/SyncGlobalsListener.h
)
