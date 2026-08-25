# Architecture and invariants

## Data path

The input boundary accepts one OpenXR-style pose and monotonic timestamp for the
head, left controller, or right controller. Conversion and quaternion
normalization happen before the sample enters its per-device queue. Queries use
an exact sample when timestamps match, interpolate between adjacent samples, or
return the newest sample while it remains within the configured age limit.

Each queue has a fixed capacity. Accepting a sample beyond the limit removes the
oldest sample and increments `dropped_overflow`. A duplicate timestamp or a
timestamp older than the newest accepted sample is rejected without changing
the queue.

## Ownership and concurrency

`PoseBuffer` owns all accepted samples. Submission and query copy value types;
the caller retains no reference into native storage. One mutex protects all
queues and counters, providing a simple linearizable order across devices.

The C boundary holds the active buffer in a shared pointer. A call copies that
pointer while the lifecycle mutex is held and then releases the lifecycle lock.
Shutdown clears the global pointer, but an already-started call retains its local
owner until completion. Result strings are constants; detailed last-error text
uses thread-local storage.

## Failure behavior

Invalid device IDs, negative timestamps, non-finite positions, non-finite or
near-zero quaternions, null outputs, duplicates, and order violations return
explicit result codes. C++ exceptions are caught inside every C entry point.
Queries do not modify their caller-owned output unless they succeed.

## Extension boundary

The core is independent of OpenXR loader state. Enabling the optional adapter
adds a small conversion from the official `XrPosef` definition; session creation,
action binding, and runtime polling remain responsibilities of the host SDK layer.
This keeps synthetic replay usable in CI and makes hardware/runtime integration
replaceable without changing Unity's ABI.
