# Compressor support library (header-only, local files only)
add_library(CompressorLib INTERFACE)

target_include_directories(CompressorLib INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_sources(CompressorLib INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/audio/BeatSyncBuffer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/audio/BucketSet.h
    ${CMAKE_CURRENT_SOURCE_DIR}/audio/PacketFifo.h
    ${CMAKE_CURRENT_SOURCE_DIR}/audio/PpqRingBuffer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/audio/RmsPacketFifo.h
)

target_link_libraries(CompressorLib INTERFACE PhuAudioLib)
