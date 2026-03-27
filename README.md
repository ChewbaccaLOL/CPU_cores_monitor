# CPU Cores Monitor

Linux-targeted C++17 command-line application that reports per-core CPU load on request and can optionally append periodic samples to a file.

Design decisions, tradeoffs, and requirement interpretations are documented in [DECISIONS.md](DECISIONS.md).

## Build

```sh
bazel build //:cpu_monitor
```

## Run

Interactive mode:

```sh
bazel run //:cpu_monitor
```

Interactive mode with periodic file logging every 5 seconds:

```sh
bazel run //:cpu_monitor -- --interval-sec 5 --output cpu.log
```

## Runtime commands

- Press `Enter` or type `print` to print the current per-core CPU load to `stdout`.
- Type `quit` to exit cleanly.
- `Ctrl+C` also stops the application gracefully.

## Notes

- The application uses POSIX APIs for runtime behavior and Linux `/proc/stat` for per-core CPU counters.
- Periodic logging uses monotonic time for scheduling and prefixes each log line with a human-readable timestamp.
- Design assumptions, tradeoffs, and requirement interpretations are documented in `DECISIONS.md`.
