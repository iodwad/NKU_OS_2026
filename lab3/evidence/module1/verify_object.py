#!/usr/bin/env python3
"""Audit decoded RV64 object instructions and the trap relocation."""
import json
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
dump = (HERE / 'objdump.log').read_text()
ret = dump.split('<__trapret>:', 1)[1]
ops = re.findall(r'^\s*[0-9a-f]+:\s+[0-9a-f]+\s+(.+)$', ret, re.M)
loads = [(int(reg), int(offset)) for reg, offset in re.findall(r'c\.ldsp\s+x(\d+),(\d+)\(x2\)', ret)]
expected = [(8, 256), (9, 264)] + [(i, i * 8) for i in range(1, 32) if i != 2] + [(2, 16)]
checks = {
    'ELF64 RISC-V object': 'file format elf64-littleriscv' in dump,
    'decoded CSR loads and all GPR offsets': loads == expected,
    'sstatus and sepc restored before GPR loads': len(ops) == 36 and ops[2:4] == ['csrrw\tx0,sstatus,x8', 'csrrw\tx0,sepc,x9'],
    'only expected restore loads CSR writes and sret': len(ops) == 36 and all(op.startswith(('c.ldsp\t', 'csrrw\t', 'sret')) for op in ops),
    'sp restored last immediately before sret': ops[-2:] == ['c.ldsp\tx2,16(x2)', 'sret'],
    'frame pointer passed in x10/a0': 'c.mv\tx10,x2' in dump,
    'call uses ra with R_RISCV_JAL trap relocation': bool(re.search(r'jal\s+x1,.*\n\s*[0-9a-f]+: R_RISCV_JAL\s+trap\s*\n\s*\n[0-9a-f]+ <__trapret>:', dump)),
    'SAVE_ALL subtracts 288 bytes': 'c.addi16sp\tx2,-288' in dump,
}
for name, passed in checks.items():
    print(('PASS ' if passed else 'FAIL ') + name)
print(json.dumps({'checks': checks, 'passed': sum(checks.values()), 'total': len(checks),
                  'scope': 'object disassembly/relocation audit; no CPU execution'}, indent=2))
sys.exit(0 if all(checks.values()) else 1)
