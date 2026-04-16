set pagination off
set breakpoint pending on
set confirm off

define allocation_hit
  printf "Dynamic allocation function was called after CpuMonitorApp::Run started.\n"
  bt 12
  quit 23
end

break CpuMonitorApp::Initialize
break CpuMonitorApp::Run

run < /tmp/cpu-monitor-gdb-input

printf "Reached CpuMonitorApp::Initialize. Initialization allocations are allowed here.\n"
continue

printf "Reached CpuMonitorApp::Run. Arming allocation breakpoints now.\n"
delete breakpoints

break malloc
commands
  silent
  allocation_hit
end

break calloc
commands
  silent
  allocation_hit
end

break realloc
commands
  silent
  allocation_hit
end

break aligned_alloc
commands
  silent
  allocation_hit
end

break posix_memalign
commands
  silent
  allocation_hit
end

continue

printf "CpuMonitorApp::Run completed without hitting allocation breakpoints.\n"
quit 0
