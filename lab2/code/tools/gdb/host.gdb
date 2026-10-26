set pagination off
set print pretty on
set breakpoint pending off
handle SIGPIPE nostop noprint pass
set $trace_va = 0x41234560
# Begin with only the architecture fill breakpoint enabled. Debugger memory
# reads also call get_physical_address, and must not stop QEMU's GDB server.
# GDB 9.2 does not skip CET endbr64 prologues reliably. Source lines ensure
# argument stack slots are initialized before evaluating conditions.
break target/riscv/cpu_helper.c:451 if mmu_idx == 1
break target/riscv/cpu_helper.c:163 if addr == $trace_va && mmu_idx == 1
disable 2
break accel/tcg/cputlb.c:847 if vaddr == ($trace_va & ~(unsigned long long)4095) && mmu_idx == 1
disable 3
printf "\nHost debugger attached to QEMU. Breakpoints 2/3 start disabled.\n"
printf "Use continue here before issuing any guest GDB command.\n"
printf "For the three-level probe: condition 1 address == $trace_va && mmu_idx == 1\n"
printf "Once breakpoint 1 hits, use enable 2 3; then step/next/finish here.\n\n"
info breakpoints
