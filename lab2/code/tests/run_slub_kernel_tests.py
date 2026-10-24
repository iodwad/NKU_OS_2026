from pathlib import Path
import subprocess,json,shlex,argparse,sys
work=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(); p.add_argument('--tag',required=True); a=p.parse_args()
out=work/'evidence/slub_tests'/a.tag; out.mkdir(exist_ok=False)
def run(command,name,timeout=70):
    r=subprocess.run(command,cwd=work,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=timeout)
    text=r.stdout.decode(errors='replace')
    (out/(name+'.log')).write_text('$ '+shlex.join(command)+'\n'+text+f'\nEXIT_CODE={r.returncode}\n')
    return r.returncode,text
build=[]
for cmd,name in [(['make','obj/kern/mm/slub.o'],'object'),(['make','-j2'],'kernel')]:
    code,text=run(cmd,name); build.append({'name':name,'exit_code':code}); print(name,code,text,flush=True)
    if code: sys.exit(1)
cmd=['timeout','--signal=TERM','45s','qemu-system-riscv64','-machine','virt','-nographic','-bios','default','-kernel','bin/ucore.img']
code,text=run(cmd,'qemu')
slub=[{'id':f'T{i:02d}','status':'PASS' if f'slub_tests T{i:02d}: PASS' in text else 'FAIL'} for i in range(1,8)]
buddy=[{'id':f'K{i:02d}','status':'PASS' if f'buddy_tests K{i:02d}: PASS' in text else 'FAIL'} for i in range(1,6)]
ok=code==124 and 'kernel panic' not in text and 'slub_tests: all 7 checks passed' in text and all(r['status']=='PASS' for r in slub+buddy)
result={'build':build,'qemu_exit_code':code,'slub':slub,'buddy':buddy,'normal_completion_observed':ok}
(out/'results.json').write_text(json.dumps(result,indent=2)); print(text); print(json.dumps(result,indent=2))
sys.exit(0 if ok else 1)
