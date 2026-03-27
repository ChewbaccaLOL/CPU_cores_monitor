#ifndef CPU_READER_H_
#define CPU_READER_H_

#include <cstddef>
#include <cstdint>

struct CpuTimes {
  std::uint64_t user = 0;
  std::uint64_t nice = 0;
  std::uint64_t system = 0;
  std::uint64_t idle = 0;
  std::uint64_t iowait = 0;
  std::uint64_t irq = 0;
  std::uint64_t softirq = 0;
  std::uint64_t steal = 0;
};

bool ParseProcStatBuffer(const char* buffer, std::size_t length,
                         CpuTimes* per_core, std::size_t core_capacity,
                         std::size_t* parsed_core_count);

double ComputeLoadPercent(const CpuTimes& previous, const CpuTimes& current);

#endif  // CPU_READER_H_
