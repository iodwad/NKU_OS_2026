# 本次交付完整文件清单

基线：`b2298c6f0be7b2b5001b3d29d7b039cb7f72c19b`。Lab2 最终共 97 个文件，一级目录仅 code 与 report。

## 原有代码迁移

原有 39 个文件均保留；除 pmm.c 与 init.c 的最小接入外，其余 Git 内容不变。

- `lab2/Makefile` → `lab2/code/Makefile`
- `lab2/kern/debug/assert.h` → `lab2/code/kern/debug/assert.h`
- `lab2/kern/debug/panic.c` → `lab2/code/kern/debug/panic.c`
- `lab2/kern/driver/console.c` → `lab2/code/kern/driver/console.c`
- `lab2/kern/driver/console.h` → `lab2/code/kern/driver/console.h`
- `lab2/kern/driver/dtb.c` → `lab2/code/kern/driver/dtb.c`
- `lab2/kern/driver/dtb.h` → `lab2/code/kern/driver/dtb.h`
- `lab2/kern/init/entry.S` → `lab2/code/kern/init/entry.S`
- `lab2/kern/init/init.c` → `lab2/code/kern/init/init.c`（最小接入修改）
- `lab2/kern/libs/stdio.c` → `lab2/code/kern/libs/stdio.c`
- `lab2/kern/mm/best_fit_pmm.c` → `lab2/code/kern/mm/best_fit_pmm.c`
- `lab2/kern/mm/best_fit_pmm.h` → `lab2/code/kern/mm/best_fit_pmm.h`
- `lab2/kern/mm/default_pmm.c` → `lab2/code/kern/mm/default_pmm.c`
- `lab2/kern/mm/default_pmm.h` → `lab2/code/kern/mm/default_pmm.h`
- `lab2/kern/mm/memlayout.h` → `lab2/code/kern/mm/memlayout.h`
- `lab2/kern/mm/mmu.h` → `lab2/code/kern/mm/mmu.h`
- `lab2/kern/mm/pmm.c` → `lab2/code/kern/mm/pmm.c`（最小接入修改）
- `lab2/kern/mm/pmm.h` → `lab2/code/kern/mm/pmm.h`
- `lab2/libs/defs.h` → `lab2/code/libs/defs.h`
- `lab2/libs/error.h` → `lab2/code/libs/error.h`
- `lab2/libs/list.h` → `lab2/code/libs/list.h`
- `lab2/libs/printfmt.c` → `lab2/code/libs/printfmt.c`
- `lab2/libs/readline.c` → `lab2/code/libs/readline.c`
- `lab2/libs/riscv.h` → `lab2/code/libs/riscv.h`
- `lab2/libs/sbi.c` → `lab2/code/libs/sbi.c`
- `lab2/libs/sbi.h` → `lab2/code/libs/sbi.h`
- `lab2/libs/stdarg.h` → `lab2/code/libs/stdarg.h`
- `lab2/libs/stdio.h` → `lab2/code/libs/stdio.h`
- `lab2/libs/string.c` → `lab2/code/libs/string.c`
- `lab2/libs/string.h` → `lab2/code/libs/string.h`
- `lab2/tools/boot.ld` → `lab2/code/tools/boot.ld`
- `lab2/tools/function.mk` → `lab2/code/tools/function.mk`
- `lab2/tools/gdbinit` → `lab2/code/tools/gdbinit`
- `lab2/tools/grade.sh` → `lab2/code/tools/grade.sh`
- `lab2/tools/inject.py` → `lab2/code/tools/inject.py`
- `lab2/tools/kernel.ld` → `lab2/code/tools/kernel.ld`
- `lab2/tools/kernel_nopage.ld` → `lab2/code/tools/kernel_nopage.ld`
- `lab2/tools/test_lab2.c` → `lab2/code/tools/test_lab2.c`
- `lab2/tools/test_lab2.h` → `lab2/code/tools/test_lab2.h`

## 新增文件

- `lab2/code/.gitattributes`
- `lab2/code/.gitignore`
- `lab2/code/kern/mm/buddy_checks.inc`
- `lab2/code/kern/mm/buddy_pmm.c`
- `lab2/code/kern/mm/buddy_pmm.h`
- `lab2/code/kern/mm/slub.c`
- `lab2/code/kern/mm/slub.h`
- `lab2/code/kern/mm/slub_checks.inc`
- `lab2/code/kern/mm/slub_test_cases.inc`
- `lab2/code/tests/buddy_host_tests.c`
- `lab2/code/tests/confirm_slub_test_resident.py`
- `lab2/code/tests/run_buddy_tests.py`
- `lab2/code/tests/run_slub_host_tests.py`
- `lab2/code/tests/run_slub_kernel_tests.py`
- `lab2/code/tests/slub_host_tests.c`
- `lab2/code/tests/test_catalog.json`
- `lab2/report/.gitattributes`
- `lab2/report/docs/2411560/BUDDY_FINAL_AUDIT.md`
- `lab2/report/docs/2411560/BUDDY_IMPLEMENTATION.md`
- `lab2/report/docs/2411560/BUDDY_INTERFACE_SPEC.md`
- `lab2/report/docs/2411560/BUDDY_TEST_REPORT.md`
- `lab2/report/docs/2411560/FILE_MANIFEST.md`
- `lab2/report/docs/2411560/PUBLISH_VALIDATION.md`
- `lab2/report/docs/2411560/README.md`
- `lab2/report/docs/2411560/SLUB_IMPLEMENTATION.md`
- `lab2/report/docs/2411560/SLUB_TEST_REPORT.md`
- `lab2/report/docs/2411560/historical/slub_capacity_first_failure.log`
- `lab2/report/docs/2411560/historical/slub_compile_fix.diff`
- `lab2/report/docs/2411560/historical/slub_harness_corrections.diff`
- `lab2/report/docs/2411560/historical/slub_initial_compile.log`
- `lab2/report/docs/2411560/prompt_provenance.json`
- `lab2/report/docs/2411560/validation/buddy_host_compile.log`
- `lab2/report/docs/2411560/validation/buddy_host_results.json`
- `lab2/report/docs/2411560/validation/buddy_host_summary.json`
- `lab2/report/docs/2411560/validation/gdb_live_object.json`
- `lab2/report/docs/2411560/validation/gdb_resident.json`
- `lab2/report/docs/2411560/validation/gdb_results.json`
- `lab2/report/docs/2411560/validation/gdb_terminal.log`
- `lab2/report/docs/2411560/validation/grader_inject.log`
- `lab2/report/docs/2411560/validation/grader_injected_build.log`
- `lab2/report/docs/2411560/validation/grader_path_check.json`
- `lab2/report/docs/2411560/validation/grader_shell_syntax.log`
- `lab2/report/docs/2411560/validation/grader_windows_crlf_failure.log`
- `lab2/report/docs/2411560/validation/kernel_build.log`
- `lab2/report/docs/2411560/validation/kernel_results.json`
- `lab2/report/docs/2411560/validation/qemu.log`
- `lab2/report/docs/2411560/validation/results.json`
- `lab2/report/docs/2411560/validation/slub_host_compile.log`
- `lab2/report/docs/2411560/validation/slub_host_results.json`
- `lab2/report/docs/2411560/validation/slub_host_summary.json`
- `lab2/report/docs/2411560/validation/slub_object_build.log`
- `lab2/report/images/.gitkeep`
- `lab2/report/prompt.md`
- `lab2/report/prompts/2411560/Buddy_Implementation_Prompt.md`
- `lab2/report/prompts/2411560/Buddy_Test_Prompt.md`
- `lab2/report/prompts/2411560/SLUB_Implementation_Prompt.md`
- `lab2/report/prompts/2411560/SLUB_Test_Prompt.md`
- `lab2/report/report.md`
