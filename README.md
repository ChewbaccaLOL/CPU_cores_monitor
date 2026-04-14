# CPU Cores Monitor

Linux-targeted C++17 command-line application that reports per-core CPU load on request and can optionally append periodic samples to a file.

Design decisions, tradeoffs, and requirement interpretations are documented in [DECISIONS.md](DECISIONS.md).

## Build

```sh
bazel build //:cpu_monitor
```

## Test

Run the full test suite:

```sh
bazel test //...
```

Run individual test targets:

```sh
bazel test //:args_test
bazel test //:cpu_reader_test
bazel test //:app_test
```

Run Robot Framework acceptance tests:

```sh
source .venv/bin/activate
bazel build //:cpu_monitor
robot --outputdir robot-results tests/robot
```

## Coverage

Generate an LCOV coverage report for the full test suite:

```sh
bazel coverage //... --combined_report=lcov
```

The combined LCOV report is written to:

```sh
bazel-out/_coverage/_coverage_report.dat
```

If `genhtml` is installed, you can turn that LCOV file into a browsable HTML report:

```sh
genhtml -o coverage-html bazel-out/_coverage/_coverage_report.dat
```

Then open:

```sh
coverage-html/index.html
```

Current coverage snapshot:

```text
Overall coverage rate:
  lines......: 80.0% (392 of 490 lines)
  functions..: 70.9% (39 of 55 functions)

Key source files:
  src/app.cc: 273/367 lines, 27/41 functions
  src/cpu_reader.cc: 69/71 lines, 6/6 functions
  src/args.cc: 47/49 lines, 3/3 functions
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
