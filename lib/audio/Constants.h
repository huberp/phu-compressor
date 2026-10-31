namespace phu::utility
{
    /**
     * Number of milliseconds in one minute.
     * Used for BPM <-> time conversions: ms_per_beat = 60000 / bpm.
     */
    inline constexpr double kMillisecondsPerMinute = 60000.0;
    /**
     * Number of milliseconds in one second.
     * Used for time conversions: ms = s * 1000.
     */
    inline constexpr double kMillisecondsPerSecond = 1000.0;
}