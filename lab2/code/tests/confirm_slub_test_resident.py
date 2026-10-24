from pathlib import Path
import subprocess,socket,shlex,re,json
work=Path(__file__).resolve().parents[1]
out=work/'evidence/slub_tests/gdb'; out.mkdir(exist_ok=False)
cmd=['riscv64-unknown-elf-objdump','-d','--disassemble=kern_init','bin/kernel']
raw=subprocess.check_output(cmd,cwd=work,text=True)
(out/'kern_init.log').write_text('$ '+shlex.join(cmd)+'\n'+raw)
loops=re.findall(r'^([0-9a-f]+):\s+[0-9a-f]+\s+j\s+\1\b',raw,re.M)
assert len(loops)==1
loop=loops[0]
with socket.socket() as s:
    s.bind(('127.0.0.1',0)); port=s.getsockname()[1]
sample_path=str(out/'live_object.json'); final_path=str(out/'resident.json')
script=f'''set pagination off
set confirm off
file bin/kernel
target remote 127.0.0.1:{port}
hbreak slub_free if ptr != 0
continue
info args
python
import gdb,json
p=int(gdb.parse_and_eval('(uintptr_t)ptr'))
offset=int(gdb.parse_and_eval('va_pa_offset'))
index=((p-offset)>>12)-int(gdb.parse_and_eval('nbase'))
h=int(gdb.parse_and_eval('(uintptr_t)owner[%d]'%index))
descriptor=int(gdb.parse_and_eval('(uintptr_t)owner[%d]->page'%index))
slot_size=int(gdb.parse_and_eval('owner[%d]->object_bytes'%index))
header=(int(gdb.parse_and_eval('sizeof(struct slub_slab)'))+15)&~15
assert (p-h-header)%slot_size==0 and p%16==0
assert descriptor != p and descriptor != h
assert bytes(gdb.selected_inferior().read_memory(p,24))==bytes([0x55])*24
sample={{'object_va':hex(p),'object_pa':hex(p-offset),'slab_va':hex(h),'slab_pa':hex(h-offset),'descriptor_va':hex(descriptor),'descriptor_index':index,'slot_size':slot_size,'header_bytes':header,'data_pattern_verified':True}}
open({sample_path!r},'w').write(json.dumps(sample,indent=2))
print(sample)
end
delete 1
hbreak *0x{loop}
continue
info registers pc
x/i $pc
p is_panic
p free_count
p live_objects
p slab_count
p sizeof(struct slub_slab)
p sizeof(owner)
p pmm_manager->name
python
assert int(gdb.parse_and_eval('$pc'))==0x{loop}
assert int(gdb.parse_and_eval('is_panic'))==0
assert int(gdb.parse_and_eval('live_objects'))==0
assert int(gdb.parse_and_eval('slab_count'))==0
assert int(gdb.parse_and_eval('slub_ready'))==1
for i in range(251):
    assert int(gdb.parse_and_eval('(uintptr_t)slub_class_test_objects[%d].ptr'%i))==0
address=int(gdb.parse_and_eval('(uintptr_t)owner'))
length=int(gdb.parse_and_eval('sizeof(owner)'))
assert not any(bytes(gdb.selected_inferior().read_memory(address,length)))
for i in range(8):
    assert bool(gdb.parse_and_eval('partial[%d].next == &partial[%d] && partial[%d].prev == &partial[%d]'%(i,i,i,i)))
state={{'pc':hex(int(gdb.parse_and_eval('$pc'))),'panic':False,'live_objects':0,'slab_count':0,'owner_all_zero':True,'all_partial_lists_empty':True,'free_pages':int(gdb.parse_and_eval('free_count')),'slab_header_struct_bytes':int(gdb.parse_and_eval('sizeof(struct slub_slab)')),'owner_bytes':length}}
open({final_path!r},'w').write(json.dumps(state,indent=2))
end
detach
quit
'''
(out/'inspect.gdb').write_text(script)
qcmd=['qemu-system-riscv64','-machine','virt','-nographic','-bios','default','-kernel','bin/ucore.img','-S','-gdb',f'tcp:127.0.0.1:{port}']
gcmd=['gdb-multiarch','-q','-batch','-x',str(out/'inspect.gdb')]
with (out/'serial.log').open('w') as serial:
    serial.write('$ '+shlex.join(qcmd)+'\n'); serial.flush()
    qemu=subprocess.Popen(qcmd,cwd=work,stdout=serial,stderr=subprocess.STDOUT)
    try:
        r=subprocess.run(gcmd,cwd=work,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=40)
        text=r.stdout.decode(errors='replace')
        (out/'terminal.log').write_text('$ '+shlex.join(gcmd)+'\n'+text+f'\nEXIT_CODE={r.returncode}\n')
        print(text)
        assert r.returncode==0 and 'Traceback' not in text and 'Error in sourced command' not in text
    finally:
        qemu.terminate(); qemu.wait(timeout=5)
        serial.write(f'\nQEMU_EXIT_CODE={qemu.returncode} (controlled termination)\n')
serial=(out/'serial.log').read_text()
assert 'slub_check: all basic checks passed' in serial and 'kernel panic' not in serial
assert all('slub_tests T%02d: PASS'%i in serial for i in range(1,8))
assert all('buddy_tests K%02d: PASS'%i in serial for i in range(1,6))
state=json.loads((out/'resident.json').read_text())
counts=re.findall(r'restored (\d+) free pages',serial)
assert len(counts)>=2 and all(int(n)==state['free_pages'] for n in counts)
(out/'results.json').write_text(json.dumps({'gdb_exit':r.returncode,'qemu_exit':qemu.returncode,'sample_and_final_state_verified':True},indent=2))
print(json.dumps(state,indent=2))
