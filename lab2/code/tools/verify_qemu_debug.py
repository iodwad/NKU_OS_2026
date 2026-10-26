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
    def __init__(self, executable, arguments, log, asynchronous=True):
        self.output = queue.Queue()
        self.stops = deque()
        self.token = 0
        self.log = log.open('w')
        self.process = subprocess.Popen([executable, '-q', '-nx', '--interpreter=mi2'] +
                                        (['-iex', 'set mi-async on'] if asynchronous else []) + arguments,
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
    result = {'status': 'FAIL', 'evidence': str(evidence), 'checks': {}, 'walk': [], 'source_steps': []}
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

        host = MI(shutil.which('gdb'), [str(qemu_binary), '-p', str(qemu.pid)], evidence / 'host-gdb.log', asynchronous=True)
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
        # Observe the real data slow path, not the instruction-lookup helper.
        for definition in [
            str(source / 'accel/tcg/cputlb.c') + ':1271 if addr == 0x41234560',
            str(source / 'accel/tcg/cputlb.c') + ':1010 if page == 0x41234000 && mmu_idx == 1',
            str(source / 'accel/tcg/cputlb.c') + ':1338 if addr == 0x41234560',
        ]:
            host.console('break ' + definition)
        host.resume()
        result['scratch_audit'] = {n: hex(guest.integer(expr)) for n, expr in [
            ('kernel_end', '(unsigned long long)&end'), ('pages_before', '(unsigned long long)pages'),
            ('npage_before', 'npage'), ('dtb_pa', 'boot_dtb'), ('satp', '$satp')]}
        assert guest.integer('(unsigned long long)&end') < 0xffffffffc7ffa000
        assert guest.integer('(unsigned long long)pages') == 0 and guest.integer('npage') == 0
        assert not 0x87ffa000 <= guest.integer('boot_dtb') < 0x87ffe000
        original_regs = {n: guest.integer('$' + n) for n in ['pc', 't0', 't1', 't2', 'satp']}
        root_slot = ((original_regs['satp'] & ((1 << 44)-1)) << 12) + 0xffffffff40000000 + 8
        for name, start, size in [('scratch', 0xffffffffc7ffa000, 16384), ('root', root_slot, 8)]:
            guest.console('dump binary memory {} {} {}'.format(evidence / (name+'-before.bin'), hex(start), hex(start+size)))
        guest.console('sv39-probe-prepare')
        guest.console('x/4i 0xffffffffc7ffa000')
        result['instruction_words'] = [hex(guest.integer('*(unsigned int*)'+hex(0xffffffffc7ffa000+i*4))) for i in range(4)]
        assert result['instruction_words'] == ['0x12000073', '0x33283', '0x33383', '0x12000073']
        # Flush old translations, then execute a load through the new mapping.
        guest.step()
        guest.stopped()
        guest.step()
        stop = host.stopped()
        assert 'load_helper' in stop, stop
        result['first_lookup'] = {n: host.integer(n) for n in ['addr', 'mmu_idx', 'index', 'tlb_addr', 'code_read']}
        result['first_lookup']['tag_matches'] = (result['first_lookup']['tlb_addr'] & 0xfffffffffffff800) == (TRACE_VA & ~4095)
        assert not result['first_lookup']['tag_matches']
        host.step(next_line=True)
        result['source_steps'].append(host.stopped())
        host.resume()
        stop = host.stopped()
        assert 'victim_tlb_hit' in stop, stop
        result['victim_miss'] = {n: host.integer(n) for n in ['mmu_idx', 'index', 'vidx', 'page']}
        host.command('-exec-finish')
        result['victim_return'] = host.stopped()
        assert 'return-value="false"' in result['victim_return'] or 'return-value="0"' in result['victim_return']
        host.resume()
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
        result['sv39_context'] = {n:host.integer(expr) for n,expr in [('priv','env->priv'),('satp','env->satp'),('access_type','access_type'),('mmu_idx','mmu_idx')]}
        assert result['sv39_context']['priv'] == 1
        host.console('set $case_out_pa = physical')
        host.console('set $case_out_prot = prot')
        for expected in range(3):
            host.resume()
            stop = host.stopped()
            host.step(next_line=True)  # execute the ppn assignment before observing it
            result['source_steps'].append(host.stopped())
            item = {name: host.integer(name) for name in ['i', 'levels', 'ptidxbits', 'ptesize', 'ptshift', 'idx', 'base', 'pte_addr', 'pte', 'ppn']}
            assert item['i'] == expected, (stop, item)
            item['flags'] = {n: bool(item['pte'] & (1 << bit)) for n, bit in [('V',0),('R',1),('W',2),('X',3),('U',4),('A',6),('D',7)]}
            item['leaf'] = bool(item['pte'] & 14)
            host.console('info locals')
            if expected < 2:
                for unused in range(3):
                    host.step(next_line=True)
                    result['source_steps'].append(host.stopped())
                item['next_base_observed'] = host.integer('base')
                assert item['next_base_observed'] == item['ppn'] << 12
            else:
                for unused in range(24):
                    host.step(next_line=True)
                    stopped = host.stopped()
                    result['source_steps'].append(stopped)
                    if 'line="337"' in stopped:
                        break
                else:
                    raise RuntimeError('Leaf never reached computed physical output')
                result['physical_output_observed'] = host.integer('*physical')
            result['walk'].append(item)
        host.command('-exec-finish')
        result['translate_return_stop'] = host.stopped()
        result['translate_output'] = {'pa': host.integer('*$case_out_pa'), 'prot': host.integer('*$case_out_prot')}
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
        host.console('set $data_te = te')
        host.resume()
        stop = host.stopped()
        assert 'load_helper' in stop, stop
        host.step(next_line=True)
        result['source_steps'].append(host.stopped())
        result['host_ram_read'] = {'haddr': host.integer('(uintptr_t)haddr'), 'value': host.integer('*(uint64_t*)haddr')}
        assert result['host_ram_read']['value'] == VALUE
        host.console('disable 6 7 8')
        host.resume()
        guest.stopped()
        assert guest.integer('$t0') == VALUE
        result['checks']['first_load_value'] = True
        host.interrupt()
        result['before_second'] = {n: host.integer('$data_te->'+n) for n in ['addr_read','addr_write','addr_code','addend']}
        result['before_second']['tag_matches'] = (result['before_second']['addr_read'] & 0xfffffffffffff800) == (TRACE_VA & ~4095)
        assert result['before_second']['tag_matches']
        host.resume()
        guest.step()
        guest.stopped(timeout=20)
        assert guest.integer('$t2') == VALUE
        hits = host.command('-break-info 1')
        assert 'times="1"' in hits, hits
        result['checks']['second_load_without_refill'] = True
        result['va'] = hex(TRACE_VA)
        result['pa'] = hex(result['tlb_set_page']['paddr'] | (TRACE_VA & 4095))
        result['loaded_value'] = hex(VALUE)
        result['second_loaded_value'] = hex(guest.integer('$t2'))
        host.interrupt()
        result['after_second'] = {n: host.integer('$data_te->'+n) for n in ['addr_read','addr_write','addr_code','addend']}
        result['sv39_breakpoints'] = host.command('-break-list')
        # Same privilege and access width, same physical bytes, now Bare.
        host.console('disable breakpoints')
        for definition in [
            '{} if address == 0x87ffb560 && access_type == 0 && mmu_idx == 1'.format(fill_line),
            '{} if addr == 0x87ffb560 && access_type == 0 && mmu_idx == 1'.format(physical_line),
            '{} if addr == 0x87ffb560 && mmu_idx == 1'.format(pte_line),
            '{} if vaddr == 0x87ffb000 && mmu_idx == 1'.format(set_page_line),
        ]:
            host.console('break '+definition)
        host.resume()
        guest.console('set $pc = (unsigned long long)0x87ffa000')
        guest.console('set $t1 = (unsigned long long)0x87ffb560')
        guest.console('set $satp = (unsigned long long)0')
        guest.console('maintenance flush register-cache')
        assert guest.integer('$satp') == 0
        guest.step(); guest.stopped()
        guest.step()
        stop = host.stopped()
        assert 'riscv_cpu_tlb_fill' in stop, stop
        host.resume()
        stop = host.stopped()
        assert 'get_physical_address' in stop, stop
        result['bare'] = {'priv':host.integer('env->priv'), 'satp':host.integer('env->satp'), 'mmu_idx':host.integer('mmu_idx'), 'addr':host.integer('addr'), 'steps':[]}
        assert result['bare']['priv'] == 1 and result['bare']['satp'] == 0
        for unused in range(20):
            host.step(next_line=True)
            stopped = host.stopped(); result['bare']['steps'].append(stopped)
            if 'line="199"' in stopped:
                break
        else:
            raise RuntimeError('Bare path did not reach direct return')
        result['bare']['physical_return'] = host.integer('*physical')
        assert result['bare']['physical_return'] == 0x87ffb560
        host.command('-exec-finish'); result['bare']['return_stop'] = host.stopped()
        host.resume(); stopped = host.stopped()
        assert 'tlb_set_page' in stopped, stopped
        result['bare']['tlb_set_page'] = {n:host.integer(n) for n in ['vaddr','paddr','prot','mmu_idx','size']}
        host.console('disable 9 10 11 12'); host.resume()
        guest.stopped()
        result['bare']['t0'] = hex(guest.integer('$t0'))
        assert guest.integer('$t0') == VALUE
        guest.step(); guest.stopped()
        result['bare']['t2'] = hex(guest.integer('$t2'))
        assert guest.integer('$t2') == VALUE
        host.interrupt()
        result['bare']['breakpoints'] = host.command('-break-list')
        host.console('disable breakpoints'); host.resume()
        guest.console('set $satp = (unsigned long long)'+result['satp'])
        guest.console('maintenance flush register-cache')
        assert guest.integer('$satp') == original_regs['satp']
        guest.console('sv39-probe-restore', timeout=90)
        guest.step()
        guest.stopped()
        guest.console('sv39-probe-restore-finish')
        assert guest.integer('$pc') == int(result['kernel_pc'], 16)
        result['checks']['probe_restored'] = True
        result['restored_registers'] = {n: hex(guest.integer('$'+n)) for n in original_regs}
        assert all(guest.integer('$'+n) == value for n,value in original_regs.items())
        result['restored_memory'] = {}
        for name, start, size in [('scratch', 0xffffffffc7ffa000, 16384), ('root', root_slot, 8)]:
            guest.console('dump binary memory {} {} {}'.format(evidence / (name+'-after.bin'),hex(start),hex(start+size)))
            before=(evidence/(name+'-before.bin')).read_bytes();after=(evidence/(name+'-after.bin')).read_bytes()
            assert before == after
            result['restored_memory'][name]={'bytes':size,'before_sha256':hashlib.sha256(before).hexdigest(),'after_sha256':hashlib.sha256(after).hexdigest()}
        result['checks']['bare_s_mode_load'] = True
        result['checks']['scratch_and_root_byte_identical'] = True
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
        result['kernel_final'] = {n: guest.integer(n) for n in ['is_panic','live_objects','slab_count','free_count']}
        assert all(result['kernel_final'][n] == 0 for n in ['is_panic','live_objects','slab_count'])
        restored = re.findall(r'(?:all 5 live checks|all 7 checks) passed; restored (\d+) free pages', text)
        assert len(restored) == 2 and all(int(n) == result['kernel_final']['free_count'] for n in restored)
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
        if qemu:
            result['cleanup'] = {'qemu_pid':qemu.pid, 'exit_code':qemu.poll(),
                                 'pid_released':not Path('/proc/'+str(qemu.pid)).exists()}
            listeners = subprocess.check_output(['ss','-H','-ltn'],text=True)
            result['cleanup']['port_released'] = not any(line.split()[3].endswith(':'+str(port)) for line in listeners.splitlines())
        result['finished_at'] = datetime.now().astimezone().isoformat()
        qemu_log.close()
        (evidence / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
        (ROOT / '.debug/evidence/latest-result.json').write_text(json.dumps(result, indent=2) + '\n')
        print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
