using System.Diagnostics;

namespace XRBridge
{
    public static class MonotonicClock
    {
        public static long NowNanoseconds =>
            (long)(Stopwatch.GetTimestamp() * (1_000_000_000.0 / Stopwatch.Frequency));
    }
}
