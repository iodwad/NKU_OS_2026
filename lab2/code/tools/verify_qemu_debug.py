#!/usr/bin/env python3
"""Verify two live GDBs, a reversible Sv39 walk and the original kernel checks."""
import argparse
from collections import deque
from datetime import datetime
import hashlib
import json
import os
from pathlib import Path
import queue
import re
import shutil
import socket
import subprocess
import sys
import threading
import time

ROOT = Path(__file__).resolve().parents[1]
TRACE_VA = 0x41234560
VALUE = 0x1122334455667788


class MI:
    def __init__(self, executable, arguments, log):
        self.output = queue.Queue()
        self.stops = deque()
        self.token = 0
        self.log = log.open('w')
        self.process = subprocess.Popen([executable, '-q', '-nx', '--interpreter=mi2'] + arguments,
                                        cwd=ROOT, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                        stderr=subprocess.STDOUT, text=True, bufsize=1)
        threading.Thread(target=self.read, daemon=True).start()

    def read(self):
        for line in self.process.stdout:
            self.log.write(line)
            self.log.flush()
            self.output.put(line.rstrip())
        self.output.put(None)

    def receive(self, deadline):
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise TimeoutError('GDB timeout; see {}'.format(self.log.name))
        line = self.output.get(timeout=remaining)
        if line is None:
            raise RuntimeError('GDB exited; see {}'.format(self.log.name))
        if line.startswith('*stopped'):
            self.stops.append(line)
        return line

    def command(self, command, timeout=40):
        self.token += 1
        token = str(self.token)
        self.log.write('COMMAND ' + command + '\n')
        self.log.flush()
        self.process.stdin.write(token + command + '\n')
        self.process.stdin.flush()
        deadline = time.monotonic() + timeout
        while True:
            line = self.receive(deadline)
            if line.startswith(token + '^'):
                if line.startswith(token + '^error'):
                    raise RuntimeError(line)
                return line

    def console(self, command, timeout=40):
        return self.command('-interpreter-exec console ' + json.dumps(command), timeout)

    def value(self, expression):
        result = self.command('-data-evaluate-expression ' + json.dumps(expression))
        match = re.search(r'value=("(?:\\.|[^"\\])*")', result)
        if not match:
            raise RuntimeError('No value: ' + result)
        return json.loads(match.group(1))

    def integer(self, expression):
        value = self.value('(unsigned long long)(' + expression + ')')
        match = re.match(r'(0x[0-9a-fA-F]+|[+-]?\d+)', value)
        if not match:
            raise RuntimeError('Not an integer: ' + value)
        # GDB 9.2 displays high-bit CSR values as signed integers.
        value = int(match.group(1), 0)
        return value & ((1 << 64) - 1) if value < 0 else value

    def resume(self):
        self.stops.clear()
        self.command('-exec-continue')

    def step(self, next_line=False):
        self.stops.clear()
        self.command('-exec-next' if next_line else '-exec-step-instruction')

    def stopped(self, timeout=40):
        deadline = time.monotonic() + timeout
        while not self.stops:
            self.receive(deadline)
        return self.stops.popleft()

    def interrupt(self):
        self.stops.clear()
        self.command('-exec-interrupt --all')
        return self.stopped()

    def close(self):
        if self.process.poll() is None:
            try:
                self.process.stdin.write('-gdb-exit\n')
                self.process.stdin.flush()
                self.process.wait(timeout=3)
            except (OSError, subprocess.TimeoutExpired):
                self.process.terminate()
                try:
                    self.process.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    self.process.kill()


def source_line(source, relative, fragment):
    for number, line in enumerate((source / relative).read_text().splitlines(), 1):
        if fragment in line:
            return '{}:{}'.format(source / relative, number)
    raise RuntimeError('Source marker missing: ' + fragment)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--qemu', default=str(ROOT / '.debug/qemu-build/riscv64-softmmu/qemu-system-riscv64'))
    parser.add_argument('--data', default=str(ROOT / '.debug/qemu-4.1.1/pc-bios'))
    args = parser.parse_args()
    os.chdir(ROOT)
    stamp = datetime.now().strftime('%Y%m%d-%H%M%S')
    evidence = ROOT / '.debug/evidence' / ('verify-' + stamp)
    evidence.mkdir(parents=True, exist_ok=False)
    result = {'status': 'FAIL', 'evidence': str(evidence), 'checks': {}, 'walk': []}
    host = guest = qemu = None
    qemu_log = (evidence / 'qemu.log').open('w')
    try:
        qemu_binary = Path(args.qemu).resolve()
        version = subprocess.check_output([str(qemu_binary), '--version'], text=True)
        assert 'version 4.1.1' in version, version
        result['qemu_version'] = version.splitlines()[0]
        result['checks']['qemu_4_1_1'] = True
        sections = subprocess.check_output(['readelf', '-S', str(qemu_binary)], text=True)
        assert '.debug_info' in sections and '.debug_line' in sections
        result['checks']['qemu_debug_symbols'] = True
        for relative in ['bin/kernel', 'bin/ucore.img', 'kern/init/init.c', 'kern/init/entry.S', 'kern/mm/pmm.c']:
            result.setdefault('sha256', {})[relative] = hashlib.sha256((ROOT / relative).read_bytes()).hexdigest()
        with socket.socket() as reservation:
            reservation.bind(('127.0.0.1', 0))
            port = reservation.getsockname()[1]
        qemu = subprocess.Popen([sys.executable, str(ROOT / 'tools/qemu_debug.py'), 'run',
                                 '--qemu', str(qemu_binary), '--data', args.data, '--port', str(port)],
                                cwd=ROOT, stdin=subprocess.DEVNULL, stdout=qemu_log, stderr=subprocess.STDOUT)
        for attempt in range(100):
            if qemu.poll() is not None:
                raise RuntimeError('QEMU launch failed; see ' + str(evidence / 'qemu.log'))
            try:
                with socket.create_connection(('127.0.0.1', port), timeout=.2):
                    break
            except OSError:
                time.sleep(.1)
        else:
            raise TimeoutError('QEMU stub did not start')

        guest = MI(shutil.which('riscv64-unknown-elf-gdb'), [str(ROOT / 'bin/kernel')], evidence / 'guest-gdb.log')
        guest.console('set pagination off')
        guest.console('set architecture riscv:rv64')
        guest.console('set remotetimeout 120')
        guest.command('-target-select remote 127.0.0.1:{}'.format(port))
        guest.console('source ' + str(ROOT / 'tools/gdb/guest.gdb'), timeout=90)
        guest.stops.clear()
        result['kernel_pc'] = hex(guest.integer('$pc'))
        assert guest.integer('$pc') == guest.integer('(unsigned long long)kern_init')
        result['satp'] = hex(guest.integer('$satp'))
        assert guest.integer('$satp') >> 60 == 8
        result['checks']['guest_gdb_kern_init'] = True

        host = MI(shutil.which('gdb'), [str(qemu_binary), '-p', str(qemu.pid)], evidence / 'host-gdb.log')
        host.console('set pagination off')
        host.console('handle SIGPIPE nostop noprint pass')
        for function in ['riscv_cpu_tlb_fill', 'get_physical_address', 'tlb_set_page']:
            host.console('info address ' + function)
            host.console('list ' + function)
        source = Path(args.data).resolve().parent
        pte_line = source_line(source, 'target/riscv/cpu_helper.c', 'target_ulong ppn = pte >> PTE_PPN_SHIFT;')
        copy_line = source_line(source, 'accel/tcg/cputlb.c', 'copy_tlb_helper_locked(te, &tn);')
        fill_line = source_line(source, 'target/riscv/cpu_helper.c', 'ret = get_physical_address(env, &pa, &prot, address, access_type, mmu_idx);')
        physical_line = source_line(source, 'target/riscv/cpu_helper.c', 'int mode = mmu_idx;')
        set_page_line = source_line(source, 'accel/tcg/cputlb.c', 'tlb_set_page_with_attrs(cpu, vaddr, paddr, MEMTXATTRS_UNSPECIFIED,')
        definitions = [
            '{} if address == {} && mmu_idx == 1'.format(fill_line, TRACE_VA),
            '{} if addr == {} && mmu_idx == 1'.format(physical_line, TRACE_VA),
            '{} if addr == {} && mmu_idx == 1'.format(pte_line, TRACE_VA),
            '{} if vaddr == {} && mmu_idx == 1'.format(set_page_line, TRACE_VA & ~4095),
            '{} if vaddr == {} && mmu_idx == 1'.format(copy_line, TRACE_VA & ~4095),
        ]
        for definition in definitions:
            host.console('break ' + definition)
        result['checks']['host_gdb_attach_and_breakpoints'] = True
        host.resume()
        guest.console('sv39-probe-prepare')
        # Flush old translations, then execute a load through the new mapping.
        guest.step()
        guest.stopped()
        guest.step()
        stop = host.stopped()
        assert 'riscv_cpu_tlb_fill' in stop, stop
        host.console('info args')
        host.console('info locals')
        host.console('bt 12')
        result['fill'] = {name: hex(host.integer(name)) for name in ['address', 'size', 'access_type', 'mmu_idx']}
        result['checks']['riscv_cpu_tlb_fill_hit'] = True
        host.resume()
        stop = host.stopped()
        assert 'get_physical_address' in stop, stop
        result['satp'] = hex(host.integer('env->satp'))
        result['root_pa'] = hex((int(result['satp'], 16) & ((1 << 44) - 1)) << 12)
        result['checks']['get_physical_address_hit'] = True
        for expected in range(3):
            host.resume()
            stop = host.stopped()
            item = {name: host.integer(name) for name in ['i', 'ptshift', 'idx', 'pte_addr', 'pte']}
            assert item['i'] == expected, (stop, item)
            result['walk'].append(item)
        host.resume()
        stop = host.stopped()
        assert 'tlb_set_page' in stop, stop
        result['tlb_set_page'] = {name: host.integer(name) for name in ['vaddr', 'paddr', 'prot', 'mmu_idx', 'size']}
        assert result['tlb_set_page']['paddr'] == 0x87FFB000
        result['checks']['tlb_set_page_hit'] = True
        host.resume()
        host.stopped()
        # The entry is fully assembled here; execute its copy and inspect it.
        host.console('print tn')
        host.stops.clear()
        host.command('-exec-next')
        host.stopped()
        result['tlb_entry'] = {name: host.integer('te->' + name) for name in ['addr_read', 'addr_write', 'addr_code', 'addend']}
        host.console('print *te')
        host.resume()
        guest.stopped()
        assert guest.integer('$t0') == VALUE
        result['checks']['first_load_value'] = True
        guest.step()
        guest.stopped(timeout=20)
        assert guest.integer('$t2') == VALUE
        hits = host.command('-break-info 1')
        assert 'times="1"' in hits, hits
        result['checks']['second_load_without_refill'] = True
        result['va'] = hex(TRACE_VA)
        result['pa'] = hex(result['tlb_set_page']['paddr'] | (TRACE_VA & 4095))
        result['loaded_value'] = hex(VALUE)
        guest.console('sv39-probe-restore', timeout=90)
        assert guest.integer('$pc') == int(result['kernel_pc'], 16)
        result['checks']['probe_restored'] = True
        host.interrupt()
        host.console('disable breakpoints')
        host.command('-target-detach')
        host.close()
        host = None
        guest.resume()
        deadline = time.monotonic() + 150
        while time.monotonic() < deadline:
            text = (evidence / 'qemu.log').read_text(errors='replace')
            if 'slub_tests: all 7 checks passed' in text:
                assert 'buddy_tests: all 5 live checks passed' in text
                result['checks']['original_buddy_5_and_slub_7'] = True
                break
            if 'kernel panic' in text.lower():
                raise RuntimeError('Kernel panic; inspect qemu.log')
            time.sleep(.2)
        else:
            raise TimeoutError('Original kernel checks did not finish in 150 seconds')
        guest.interrupt()
        guest.command('-target-detach')
        result['status'] = 'PASS'
    except Exception as error:
        result['error'] = '{}: {}'.format(type(error).__name__, error)
        raise
    finally:
        for debugger in [host, guest]:
            if debugger:
                debugger.close()
        if qemu and qemu.poll() is None:
            qemu.terminate()
            try:
                qemu.wait(timeout=5)
            except subprocess.TimeoutExpired:
                qemu.kill()
                qemu.wait()
        qemu_log.close()
        (evidence / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
        (ROOT / '.debug/evidence/latest-result.json').write_text(json.dumps(result, indent=2) + '\n')
        print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
