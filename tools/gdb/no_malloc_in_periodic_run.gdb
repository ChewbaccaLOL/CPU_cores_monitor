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

run --interval-sec 1 --output /tmp/cpu-monitor-gdb-periodic.log < /dev/null

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

break src/app.cc:404
commands
  silent
  printf "Periodic log sample completed; asking Run loop to stop.\n"
  set variable g_stop_requested = 1
  continue
end

continue

printf "Periodic CpuMonitorApp::Run completed without hitting allocation breakpoints.\n"
quit 0
