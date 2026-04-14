#include "cpu_reader.h"

#include <cerrno>
#include <cstdlib>
#include <cstring>

namespace {

bool ParseUnsigned(const char*& cursor, std::uint64_t* value) {
  while (*cursor == ' ' || *cursor == '\t') {
    ++cursor;
  }

  if (*cursor < '0' || *cursor > '9') {
    return false;
  }

  errno = 0;
  char* end = nullptr;
  const unsigned long long parsed = std::strtoull(cursor, &end, 10);
  if (errno != 0 || end == cursor) {
    return false;
  }

  *value = parsed;
  cursor = end;
  return true;
}

bool ParseCpuLine(const char* line_start, CpuTimes* out) {
  const char* cursor = line_start;
  std::uint64_t values[8] = {};
  for (std::size_t index = 0; index < 8; ++index) {
    if (!ParseUnsigned(cursor, &values[index])) {
      return false;
    }
  }

  out->user = values[0];
  out->nice = values[1];
  out->system = values[2];
  out->idle = values[3];
  out->iowait = values[4];
  out->irq = values[5];
  out->softirq = values[6];
  out->steal = values[7];
  return true;
}

std::uint64_t IdleTime(const CpuTimes& times) {
  return times.idle + times.iowait;
}

std::uint64_t TotalTime(const CpuTimes& times) {
  return times.user + times.nice + times.system + times.idle + times.iowait +
         times.irq + times.softirq + times.steal;
}

}  // namespace

bool ParseProcStatBuffer(const char* buffer, std::size_t length,
                         CpuTimes* per_core, std::size_t core_capacity,
                         std::size_t* parsed_core_count) {
  if (buffer == nullptr || per_core == nullptr || parsed_core_count == nullptr) {
    return false;
  }

  for (std::size_t index = 0; index < core_capacity; ++index) {
    per_core[index] = CpuTimes{};
  }
  *parsed_core_count = 0;

  const char* cursor = buffer;
  const char* end = buffer + length;

  while (cursor < end) {
    const char* line_end = static_cast<const char*>(
        std::memchr(cursor, '\n', static_cast<std::size_t>(end - cursor)));
    if (line_end == nullptr) {
      line_end = end;
    }

    // Only per-core lines are needed for this task; the aggregate cpu line and
    // the rest of /proc/stat are intentionally ignored.
    if ((line_end - cursor) > 3 && cursor[0] == 'c' && cursor[1] == 'p' &&
        cursor[2] == 'u' && cursor[3] >= '0' && cursor[3] <= '9') {
      errno = 0;
      char* core_end = nullptr;
      const unsigned long core_index = std::strtoul(cursor + 3, &core_end, 10);
      if (errno != 0 || core_end == cursor + 3 || core_index >= core_capacity) {
        return false;
      }

      if (!ParseCpuLine(core_end, &per_core[core_index])) {
        return false;
      }

      if (core_index + 1 > *parsed_core_count) {
        *parsed_core_count = core_index + 1;
      }
    }

    cursor = (line_end == end) ? end : line_end + 1;
  }

  return *parsed_core_count > 0;
}

double ComputeLoadPercent(const CpuTimes& previous, const CpuTimes& current) {
  const std::uint64_t previous_idle = IdleTime(previous);
  const std::uint64_t current_idle = IdleTime(current);
  const std::uint64_t previous_total = TotalTime(previous);
  const std::uint64_t current_total = TotalTime(current);

  if (current_total <= previous_total || current_idle < previous_idle) {
    return 0.0;
  }

  const std::uint64_t total_delta = current_total - previous_total;
  const std::uint64_t idle_delta = current_idle - previous_idle;
  if (total_delta == 0 || idle_delta > total_delta) {
    return 0.0;
  }

  const double active_delta = static_cast<double>(total_delta - idle_delta);
  return (active_delta * 100.0) / static_cast<double>(total_delta);
}
