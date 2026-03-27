# Decisions And Justifications

## Important

### 1. Linux-specific CPU metric source
Choice: use POSIX APIs for file I/O, timing, waiting, signals, and process behavior, and use Linux `/proc/stat` specifically for per-core CPU counters.

Reason: POSIX does not define a portable API for system-wide per-core CPU load. The task explicitly asks for load of every core, so a Linux-specific metric source is necessary in practice.

Impact / limitation: the application targets Linux. Porting it to a non-Linux system would require replacing the CPU metric source.

### 2. Meaning of "by request"
Choice: interpret "by request" as interactive `stdin` commands.

Reason: this fits a command-line application naturally, is easy to demonstrate, and avoids unsafe signal-handler logic for printing. The task wording says to print CPU load "for every core by request", which was interpreted as "print all per-core values when requested", not "request one specific core by index".

Impact / limitation: pressing Enter or typing `print` triggers stdout output; typing `quit` exits the application.

## Other

### 3. How CPU load is computed
Choice: compute utilization from deltas between two `/proc/stat` snapshots.

Reason: `/proc/stat` exposes cumulative CPU time counters since boot, not an instantaneous percentage.

Impact / limitation: the first reported sample represents activity since initialization and can be close to zero if requested immediately after startup.

### 4. Which `/proc/stat` data is used
Choice: parse only `cpuN` lines and use the first 8 numeric fields: `user`, `nice`, `system`, `idle`, `iowait`, `irq`, `softirq`, `steal`.

Reason: the task asks for per-core load, and those fields are sufficient for the standard Linux utilization formula.

Impact / limitation: the aggregate `cpu` line and unrelated lines like `intr`, `ctxt`, `btime`, and `softirq` are ignored intentionally. Extra trailing CPU fields are also ignored by design.

### 5. Idle time definition
Choice: treat `idle + iowait` as idle time.

Reason: this matches the common Linux-style interpretation of CPU utilization derived from `/proc/stat`.

Impact / limitation: busy time is treated as everything else in the sampled interval.

### 6. Single-threaded event loop
Choice: use one `pselect()`-based loop instead of multiple threads.

Reason: one loop can handle stdin requests and periodic logging without synchronization complexity.

Impact / limitation: the design stays simpler to explain and reason about. It also avoids races around shared sampling state.

### 7. Separate sampling baselines
Choice: maintain separate previous snapshots for stdout requests and periodic file logging.

Reason: without this split, manual `print` requests would change the interval used for the next scheduled log sample.

Impact / limitation: stdout and file logging are independent sampling streams even though they run in the same event loop.

### 8. Logging schedule and timestamps
Choice: schedule periodic logging with `CLOCK_MONOTONIC`, but prefix file lines with wall-clock local time.

Reason: monotonic time is correct for interval scheduling because it is not affected by wall-clock adjustments, while human-readable timestamps are better for log inspection.

Impact / limitation: scheduling stays stable even if system time changes, but the printed timestamps are calendar time, not monotonic time.

### 9. Log deadline catch-up behavior
Choice: if the loop wakes up late, advance the deadline in fixed `N`-second steps until it is back in the future.

Reason: this preserves the intended periodic schedule better than re-basing each new deadline on the current wake-up time.

Impact / limitation: catch-up preserves schedule alignment, but it does not reconstruct missed historical CPU states; logged values always reflect real snapshots taken when the process runs.

### 10. Input buffering strategy
Choice: handle `stdin` with a fixed-size reusable byte buffer.

Reason: `read()` is a byte-stream API, and this approach avoids line-by-line dynamic allocations in `Run`.

Impact / limitation: commands longer than 255 bytes are rejected. The implementation still handles partial lines and multiple commands arriving in one read.

### 11. `/proc/stat` buffer sizing
Choice: pre-size a reusable raw buffer based on the detected core count.

Reason: this keeps runtime memory behavior predictable and avoids growing buffers during monitoring.

Impact / limitation: this is a practical estimate, not a fully dynamic reader. It is a tradeoff between robustness and the task's preference to avoid runtime allocations.

### 12. Output format
Choice: print one line per sample as `core0=12.34% core1=8.91% ...`, and prefix file samples with a timestamp.

Reason: the format is simple to read in both interactive and logged modes.

Impact / limitation: the output is optimized for humans rather than structured machine parsing.

### 13. Graceful shutdown
Choice: support `quit`, `SIGINT`, and `SIGTERM` as clean shutdown paths.

Reason: this is normal command-line behavior and works well with the existing event loop.

Impact / limitation: the operating system would close file descriptors on process termination anyway, but graceful shutdown lets the loop exit intentionally rather than being cut off immediately.
