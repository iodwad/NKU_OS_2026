#!/usr/bin/env python3
"""Check module 1 against the source layout; this is not a CPU/QEMU test."""
import hashlib
import json
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
CODE = ROOT / 'code'
source = (Path(sys.argv[1]) if len(sys.argv) > 1 else CODE / 'kern/trap/trapentry.S').read_text()
before = (HERE / 'trapentry.before.S').read_text()
checks = []


def check(name, condition):
    checks.append({'name': name, 'pass': bool(condition)})
    print(('PASS ' if condition else 'FAIL ') + name)


def macro(text, name):
    return re.search(r'\.macro ' + name + r'\b(.*?)\.endm', text, re.S).group(1)


def instructions(text):
    return [line.split('#', 1)[0].strip() for line in text.splitlines()
            if line.split('#', 1)[0].strip()]


restore = macro(source, 'RESTORE_ALL')
entry = source.split('__alltraps:', 1)[1].split('.globl __trapret', 1)[0]
check('SAVE_ALL unchanged from task-start file', macro(source, 'SAVE_ALL') == macro(before, 'SAVE_ALL'))
check('__trapret and sret unchanged from task-start file', source.split('__trapret:', 1)[1] == before.split('__trapret:', 1)[1])
pattern = r'(#lab3 function1 )(?:YOUR CODE|2411560).*?(?=\n    \.endm|\n    # sp should)'
normalize = lambda text: re.sub(pattern, r'\1<implementation>', text, flags=re.S)
check('only two authorized implementation regions changed', normalize(source) == normalize(before))
check('both module1 markers use 2411560', source.count('#lab3 function1 2411560') == 2 and 'YOUR CODE' not in source)
save = macro(source, 'SAVE_ALL')
saved = {(int(reg), int(slot)) for reg, slot in re.findall(r'STORE x(\d+), (\d+)\*REGBYTES\(sp\)', save)}
check('SAVE_ALL x0/x1/x3..x31 slots', saved == {(i, i) for i in range(32) if i != 2})
check('SAVE_ALL original sp slot and CSR slots', all(t in save for t in [
    'csrw sscratch, sp', 'csrrw s0, sscratch, x0', 'STORE s0, 2*REGBYTES(sp)',
    'csrr s1, sstatus', 'csrr s2, sepc', 'csrr s3, sbadaddr', 'csrr s4, scause',
    *[f'STORE s{i-31}, {i}*REGBYTES(sp)' for i in range(32, 36)]]))
restored = [(int(reg), int(slot)) for reg, slot in re.findall(r'LOAD x(\d+), (\d+)\*REGBYTES\(sp\)', restore)]
check('restore all x1..x31 from matching slots exactly once', sorted(restored) == [(i, i) for i in range(1, 32)])
ops = instructions(restore)
check('sp restored by final frame access/instruction', bool(ops) and ops[-1] == 'LOAD x2, 2*REGBYTES(sp)')
check('restore only sstatus/sepc, no frame stores or internal sret',
      sorted(re.findall(r'csrw\s+(\w+),', restore)) == ['sepc', 'sstatus']
      and not re.search(r'\b(?:STORE|sret|addi)\b', restore))
check('trapframe address passed in a0, call then fall through', instructions(entry) == ['SAVE_ALL', 'move a0, sp', 'jal trap'])
check('288-byte frame preserves 16-byte stack alignment', 'addi sp, sp, -36 * REGBYTES' in save and 36 * 8 == 288 and 288 % 16 == 0)
header = (CODE / 'kern/trap/trap.h').read_text()
check('trap takes struct trapframe pointer', 'void trap(struct trapframe *tf);' in header)
riscv = (CODE / 'libs/riscv.h').read_text().split('#else', 1)[0]
check('actual RV64 macros use sd/ld and 8-byte slots', all(re.search(p, riscv) for p in [r'# define STORE\s+sd', r'# define LOAD\s+ld', r'# define LOG_REGBYTES\s+3']))


def simulate(epc):
    # Execute the source's restoration operations against distinct slot values.
    # The second case represents an epc changed by the C handler.
    frame_base = 0x80010000
    frame = {frame_base + i * 8: 0x1234000000000000 + i * 0x101 for i in range(36)}
    frame[frame_base + 2 * 8] = frame_base + 288
    frame[frame_base + 32 * 8] = 0x120
    frame[frame_base + 33 * 8] = epc
    snapshot = dict(frame)
    registers = {f'x{i}': 0xDEAD0000 + i for i in range(32)}
    registers['x2'] = frame_base
    aliases = {'sp': 'x2', 's0': 'x8', 's1': 'x9', 't0': 'x5', 't1': 'x6'}
    csr = {}
    try:
        for op in ops:
            load = re.fullmatch(r'LOAD (\w+), (\d+)\*REGBYTES\(sp\)', op)
            write = re.fullmatch(r'csrw (sstatus|sepc), (\w+)', op)
            if load:
                reg, slot = load.groups()
                registers[aliases.get(reg, reg)] = frame[registers['x2'] + int(slot) * 8]
            elif write:
                name, reg = write.groups()
                csr[name] = registers[aliases.get(reg, reg)]
            else:
                return False
        return (all(registers[f'x{i}'] == snapshot[frame_base + i * 8] for i in range(1, 32))
                and csr == {'sstatus': snapshot[frame_base + 256], 'sepc': epc}
                and frame == snapshot)
    except KeyError:
        return False


check('symbolic source execution restores all registers/CSR and preserves frame', simulate(0x80201234))
check('symbolic source execution uses handler-modified epc', simulate(0x80205678))
manifest = json.loads((HERE / 'code-before-sha256.json').read_text(encoding='utf-8-sig'))
changed = [item['path'] for item in manifest
           if hashlib.sha256((CODE / item['path']).read_bytes()).hexdigest() != item['sha256']]
check('no other pre-existing code file changed', set(changed) <= {'kern/trap/trapentry.S'})
result = {'checks': checks, 'passed': sum(c['pass'] for c in checks), 'total': len(checks),
          'changed_code_files': changed, 'scope': 'source checks and symbolic model; not QEMU execution'}
print(json.dumps(result, ensure_ascii=False, indent=2))
sys.exit(0 if all(c['pass'] for c in checks) else 1)
