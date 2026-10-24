"""Temporary, reversible three-level page walk at kern_init; no source changes."""
import gdb
import struct

OFFSET = 0xFFFFFFFF40000000
TRACE_VA = 0x41234560
CODE_PA = 0x87FFA000
DATA_PA = 0x87FFB000
L1_PA = 0x87FFC000
L0_PA = 0x87FFD000
VALUE = 0x1122334455667788
saved = None


def register(name):
    return int(gdb.parse_and_eval('$' + name)) & ((1 << 64) - 1)


class Prepare(gdb.Command):
    def __init__(self):
        super().__init__('sv39-probe-prepare', gdb.COMMAND_USER)

    def invoke(self, argument, from_tty):
        global saved
        if saved is not None:
            raise gdb.GdbError('Restore the existing probe first.')
        if register('pc') != int(gdb.parse_and_eval('(unsigned long long)kern_init')):
            raise gdb.GdbError('Prepare only at the initial kern_init entry, before PMM initialization.')
        satp = register('satp')
        if satp >> 60 != 8:
            raise gdb.GdbError('Expected Sv39 SATP.MODE=8.')
        if int(gdb.parse_and_eval('(unsigned long long)&end')) >= CODE_PA + OFFSET:
            raise gdb.GdbError('Scratch pages overlap the kernel.')
        inferior = gdb.selected_inferior()
        root = (satp & ((1 << 44) - 1)) << 12
        root_slot = root + OFFSET + ((TRACE_VA >> 30) & 511) * 8
        old_slot = inferior.read_memory(root_slot, 8).tobytes()
        if struct.unpack('<Q', old_slot)[0] != 0:
            raise gdb.GdbError('Selected root slot is already in use.')
        memory = inferior.read_memory(CODE_PA + OFFSET, 4 * 4096).tobytes()
        saved = {'registers': {name: register(name) for name in ('pc', 't0', 't1', 't2')},
                 'memory': memory, 'root_slot': root_slot, 'old_slot': old_slot}
        code = struct.pack('<IIII', 0x12000073, 0x00033283, 0x00033383, 0x12000073)
        l1 = bytearray(4096)
        l0 = bytearray(4096)
        struct.pack_into('<Q', l1, ((TRACE_VA >> 21) & 511) * 8, (L0_PA >> 12) << 10 | 1)
        struct.pack_into('<Q', l0, ((TRACE_VA >> 12) & 511) * 8, (DATA_PA >> 12) << 10 | 0xC7)
        inferior.write_memory(CODE_PA + OFFSET, code)
        inferior.write_memory(DATA_PA + OFFSET + (TRACE_VA & 4095), struct.pack('<Q', VALUE))
        inferior.write_memory(L1_PA + OFFSET, l1)
        inferior.write_memory(L0_PA + OFFSET, l0)
        inferior.write_memory(root_slot, struct.pack('<Q', (L1_PA >> 12) << 10 | 1))
        gdb.execute('set $t1 = {}'.format(TRACE_VA))
        gdb.execute('set $pc = {}'.format(CODE_PA + OFFSET))
        print('Probe ready: VA={:#x}, PA={:#x}, SATP={:#x}'.format(
            TRACE_VA, DATA_PA + (TRACE_VA & 4095), satp))
        print('Steps: sfence.vma; ld t0,0(t1) [miss]; ld t2,0(t1) [hit].')
        print('Return to the host debugger and continue it before guest stepi.')


class Restore(gdb.Command):
    def __init__(self):
        super().__init__('sv39-probe-restore', gdb.COMMAND_USER)

    def invoke(self, argument, from_tty):
        global saved
        if saved is None:
            raise gdb.GdbError('No active probe.')
        inferior = gdb.selected_inferior()
        inferior.write_memory(saved['root_slot'], saved['old_slot'])
        # Flush after restoring the PTE, while our scratch code is still present.
        gdb.execute('set $pc = {}'.format(CODE_PA + OFFSET + 12))
        gdb.execute('stepi')
        inferior.write_memory(CODE_PA + OFFSET, saved['memory'])
        for name, value in saved['registers'].items():
            gdb.execute('set ${} = {}'.format(name, value))
        saved = None
        print('Original root entry, scratch memory and registers restored at kern_init.')


Prepare()
Restore()
