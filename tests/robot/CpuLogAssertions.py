import re
from datetime import datetime
from pathlib import Path


_CPU_FIELDS_PATTERN = r"core0=\d+[.]\d\d%(?: core\d+=\d+[.]\d\d%)*"
_CPU_SAMPLE_RE = re.compile(
    r"^(?P<timestamp>20\d\d-\d\d-\d\d \d\d:\d\d:\d\d) "
    rf"{_CPU_FIELDS_PATTERN}$"
)
_STDOUT_SAMPLE_RE = re.compile(rf"^{_CPU_FIELDS_PATTERN}$")


def _sample_timestamps_from_text(text):
    timestamps = []
    for line in text.splitlines():
        match = _CPU_SAMPLE_RE.match(line)
        if match:
            timestamps.append(
                datetime.strptime(match.group("timestamp"), "%Y-%m-%d %H:%M:%S")
            )
    return timestamps


def _sample_timestamps(path):
    log_path = Path(path)
    if not log_path.exists():
        return []
    return _sample_timestamps_from_text(log_path.read_text(encoding="utf-8"))


def _count_stdout_samples(text):
    return sum(1 for line in text.splitlines() if _STDOUT_SAMPLE_RE.match(line))


def count_cpu_samples_in_file(path):
    return len(_sample_timestamps(path))


def count_cpu_samples_in_text(text):
    return len(_sample_timestamps_from_text(text))


def count_stdout_cpu_samples_in_text(text):
    return _count_stdout_samples(text)


def log_text_should_contain_timestamped_cpu_sample(text):
    count = count_cpu_samples_in_text(text)
    if count < 1:
        raise AssertionError("Expected at least one timestamped CPU sample.")


def log_text_should_contain_at_least_timestamped_cpu_samples(text, minimum_count):
    count = count_cpu_samples_in_text(text)
    minimum_count = int(minimum_count)
    if count < minimum_count:
        raise AssertionError(
            f"Expected at least {minimum_count} timestamped CPU samples, got {count}."
        )


def log_file_should_have_exactly_timestamped_cpu_samples(path, expected_count):
    count = count_cpu_samples_in_file(path)
    expected_count = int(expected_count)
    if count != expected_count:
        raise AssertionError(
            f"Expected exactly {expected_count} timestamped CPU samples, got {count}."
        )


def log_file_should_contain_at_least_timestamped_cpu_samples(path, minimum_count):
    count = count_cpu_samples_in_file(path)
    minimum_count = int(minimum_count)
    if count < minimum_count:
        raise AssertionError(
            f"Expected at least {minimum_count} timestamped CPU samples, got {count}."
        )


def stdout_text_should_contain_cpu_sample(text):
    count = count_stdout_cpu_samples_in_text(text)
    if count < 1:
        raise AssertionError("Expected at least one stdout CPU sample.")


def stdout_text_should_contain_at_least_cpu_samples(text, minimum_count):
    count = count_stdout_cpu_samples_in_text(text)
    minimum_count = int(minimum_count)
    if count < minimum_count:
        raise AssertionError(
            f"Expected at least {minimum_count} stdout CPU samples, got {count}."
        )


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
