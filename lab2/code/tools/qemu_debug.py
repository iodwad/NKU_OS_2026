#!/usr/bin/env python3
"""Launch one traceable QEMU and connect either debugger to that exact session."""
import argparse
import ctypes
import json
import os
from pathlib import Path
import shutil
import socket
import sys

ROOT = Path(__file__).resolve().parents[1]
STATE = ROOT / '.debug' / 'session.json'
DEFAULT_QEMU = ROOT / '.debug/qemu-build/riscv64-softmmu/qemu-system-riscv64'
DEFAULT_DATA = ROOT / '.debug/qemu-4.1.1/pc-bios'


def session():
    try:
        state = json.loads(STATE.read_text())
        executable = Path('/proc/{}/exe'.format(state['pid'])).resolve(strict=True)
        arguments = Path('/proc/{}/cmdline'.format(state['pid'])).read_bytes()
        if executable != Path(state['qemu']).resolve() or state['stub'].encode() not in arguments.split(b'\0'):
            raise ValueError('PID no longer belongs to this QEMU session')
        return state
    except (OSError, ValueError, KeyError) as error:
        raise SystemExit('No live project QEMU session. Run make debug first. ({})'.format(error))


def allow_host_debugger():
    # Yama scope=1 normally rejects a sibling GDB. Opt in only this inferior;
    # do not change ptrace_scope, capabilities, or any system-wide setting.
    if sys.platform.startswith('linux'):
        libc = ctypes.CDLL(None, use_errno=True)
        if libc.prctl(0x59616D61, ctypes.c_ulong(-1).value, 0, 0, 0):
            error = ctypes.get_errno()
            raise OSError(error, os.strerror(error))


def run(args):
    qemu = Path(args.qemu).expanduser().resolve()
    if not qemu.is_file():
        raise SystemExit('Debug QEMU missing: {}\nRun make debug-qemu.'.format(qemu))
    if STATE.exists():
        try:
            old = session()
        except SystemExit:
            old = None
        if old:
            raise SystemExit('Existing session PID {} is still running.'.format(old['pid']))
    with socket.socket() as check:
        check.bind(('127.0.0.1', args.port))
    allow_host_debugger()
    STATE.parent.mkdir(parents=True, exist_ok=True)
    stub = 'tcp:127.0.0.1:{}'.format(args.port)
    command = [str(qemu), '-L', str(Path(args.data).resolve()),
               '-machine', 'virt,accel=tcg', '-smp', '1', '-m', '128M',
               '-nographic', '-bios', 'default', '-kernel', str(ROOT / 'bin/ucore.img'),
               '-S', '-gdb', stub]
    state = {'pid': os.getpid(), 'qemu': str(qemu), 'port': args.port,
             'stub': stub, 'command': command}
    temporary = STATE.with_suffix('.tmp')
    temporary.write_text(json.dumps(state, indent=2) + '\n')
    temporary.replace(STATE)
    print('QEMU PID {}; guest paused; stub 127.0.0.1:{}'.format(os.getpid(), args.port), flush=True)
    print('Next: make gdb; after kern_init stops, run make debug-host.', flush=True)
    os.execv(str(qemu), command)


def debugger(args):
    state = session()
    gdb = shutil.which(args.gdb)
    if not gdb:
        raise SystemExit('Debugger unavailable: {}'.format(args.gdb))
    if args.mode == 'host':
        command = [gdb, '-q', '-nx', state['qemu'], '-p', str(state['pid']),
                   '-x', str(ROOT / 'tools/gdb/host.gdb')]
    else:
        command = [gdb, '-q', '-nx', str(ROOT / 'bin/kernel'),
                   '-ex', 'set architecture riscv:rv64',
                   '-ex', 'target remote 127.0.0.1:{}'.format(state['port']),
                   '-x', str(ROOT / 'tools/gdb/guest.gdb')]
    os.execv(gdb, command)


def main():
    os.chdir(ROOT)
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='mode', required=True)
    launch = sub.add_parser('run')
    launch.add_argument('--qemu', default=str(DEFAULT_QEMU))
    launch.add_argument('--data', default=str(DEFAULT_DATA))
    launch.add_argument('--port', type=int, default=1234)
    for mode, default in [('host', 'gdb'), ('guest', 'riscv64-unknown-elf-gdb')]:
        sub.add_parser(mode).add_argument('--gdb', default=default)
    args = parser.parse_args()
    if args.mode == 'run':
        run(args)
    else:
        debugger(args)


if __name__ == '__main__':
    main()
