#!/usr/bin/env python3
"""Persist commands, stdout/stderr, and actual subprocess exit codes."""
import json
import shlex
import subprocess
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
CODE = ROOT / 'code'
commands = []


def run(name, argv, cwd=ROOT):
    result = subprocess.run(argv, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    (HERE / (name + '.log')).write_text(result.stdout)
    (HERE / (name + '.exit')).write_text(str(result.returncode) + '\n')
    commands.append({'name': name, 'cwd': str(cwd), 'argv': argv, 'command': shlex.join(argv), 'exit_code': result.returncode})
    (HERE / 'commands.json').write_text(json.dumps(commands, ensure_ascii=False, indent=2) + '\n')
    print(f'{name}: exit={result.returncode}\n{result.stdout}', flush=True)
    return result.returncode


run('static-before', ['python3', str(HERE / 'verify_static.py'), str(HERE / 'trapentry.before.S')])
run('static-after', ['python3', str(HERE / 'verify_static.py')])
run('compiler-version', ['riscv64-unknown-elf-gcc', '--version'])
run('compiler-target', ['riscv64-unknown-elf-gcc', '-dumpmachine'])
run('compiler-defaults', ['riscv64-unknown-elf-gcc', '-Q', '--help=target'])
objdir = '../evidence/module1/build/obj'
bindir = '../evidence/module1/build/bin'
obj = objdir + '/kern/trap/trapentry.o'
make_args = ['make', 'V=', 'OBJDIR=' + objdir, 'BINDIR=' + bindir]
run('assemble', make_args + ['-B', obj], CODE)
run('objdump', ['riscv64-unknown-elf-objdump', '-dr', '-M', 'no-aliases,numeric', str(HERE / 'build/obj/kern/trap/trapentry.o')])
run('object-check', ['python3', str(HERE / 'verify_object.py')])
run('readelf', ['riscv64-unknown-elf-readelf', '-h', '-A', '-s', '-r', str(HERE / 'build/obj/kern/trap/trapentry.o')])

names = ['zero', 'ra', 'sp', 'gp', 'tp', 't0', 't1', 't2', 's0', 's1',
         'a0', 'a1', 'a2', 'a3', 'a4', 'a5', 'a6', 'a7', 's2', 's3',
         's4', 's5', 's6', 's7', 's8', 's9', 's10', 's11', 't3', 't4', 't5', 't6']
layout = '#include <trap.h>\n#include <riscv.h>\n'
layout += '_Static_assert(sizeof(uintptr_t) == 8 && REGBYTES == 8, "RV64 slots");\n'
for slot, name in enumerate(names):
    layout += f'_Static_assert(__builtin_offsetof(struct trapframe, gpr.{name}) == {slot * 8}, "{name} slot");\n'
for slot, name in enumerate(['status', 'epc', 'badvaddr', 'cause'], 32):
    layout += f'_Static_assert(__builtin_offsetof(struct trapframe, {name}) == {slot * 8}, "{name} slot");\n'
layout += '_Static_assert(sizeof(struct pushregs) == 256, "pushregs size");\n'
layout += '_Static_assert(sizeof(struct trapframe) == 288, "trapframe size");\n'
layout += '_Static_assert(sizeof(struct trapframe) % 16 == 0, "ABI stack alignment preserved");\n'
layout += 'void (*const module1_trap_signature)(struct trapframe *) = trap;\n'
(HERE / 'layout_check.c').write_text(layout)
run('layout-compile', ['riscv64-unknown-elf-gcc', '-mcmodel=medany', '-std=gnu99', '-Wall', '-Werror', '-nostdinc',
                      '-fno-builtin', '-Ilibs', '-Ikern/trap', '-c', str(HERE / 'layout_check.c'), '-o', str(HERE / 'build/layout_check.o')], CODE)
run('kernel-build', make_args + ['-j2'], CODE)
