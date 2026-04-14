import re
from datetime import datetime
from pathlib import Path


_CPU_SAMPLE_RE = re.compile(
    r"^(?P<timestamp>20\d\d-\d\d-\d\d \d\d:\d\d:\d\d) "
    r"core0=\d+[.]\d\d%.*$"
)


def _sample_timestamps(path):
    log_path = Path(path)
    if not log_path.exists():
        return []

    timestamps = []
    for line in log_path.read_text(encoding="utf-8").splitlines():
        match = _CPU_SAMPLE_RE.match(line)
        if match:
            timestamps.append(
                datetime.strptime(match.group("timestamp"), "%Y-%m-%d %H:%M:%S")
            )
    return timestamps


def count_cpu_samples_in_file(path):
    return len(_sample_timestamps(path))


def cpu_sample_timestamps_should_advance_about_every_second(
    path, sample_count=5, minimum_delta_seconds=1, maximum_delta_seconds=2
):
    timestamps = _sample_timestamps(path)
    sample_count = int(sample_count)
    minimum_delta_seconds = int(minimum_delta_seconds)
    maximum_delta_seconds = int(maximum_delta_seconds)

    if len(timestamps) < sample_count:
        raise AssertionError(
            f"Expected at least {sample_count} CPU samples, got {len(timestamps)}."
        )

    checked = timestamps[:sample_count]
    deltas = [
        int((current - previous).total_seconds())
        for previous, current in zip(checked, checked[1:])
    ]
    for index, delta in enumerate(deltas, start=1):
        if delta < minimum_delta_seconds or delta > maximum_delta_seconds:
            raise AssertionError(
                "Expected CPU sample timestamps to advance about every second; "
                f"delta between sample {index} and {index + 1} was {delta}s. "
                f"All checked deltas: {deltas}"
            )
