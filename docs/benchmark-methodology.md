# Benchmark methodology

`xrbridge_benchmark` deterministically generates sinusoidal position and yaw for
at least 100,000 poses. For each pose it performs OpenXR-to-Unity-to-OpenXR
conversion, records maximum position and angular round-trip error, and submits
the Unity pose to a buffer large enough to avoid overflow. It then queries every
adjacent midpoint and measures each call with `std::chrono::steady_clock`.

Combined throughput divides all submissions plus queries by wall time around the
complete conversion/submit/query workload. Median and p95 values are order
statistics over individual query durations. The runner returns failure if any
submission or interpolation fails. JSON output includes raw counts, elapsed time,
unrounded metrics, CPU, logical threads, memory, OS, compiler, build mode, and
command.

The committed result is a single local Release run without warm-up correction,
CPU affinity, elevated scheduling priority, or background-load control. It is
useful for reproducibility and regression comparison, not as a claim about Unity
frame time, OpenXR runtime latency, or a physical headset.
