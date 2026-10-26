from pathlib import Path
import subprocess,json,shlex,sys,argparse,shutil
work=Path(__file__).resolve().parents[1]
small=[1,8,15,16,17,24,31,32,33,60,63,64,65,100,127,128,129,255,256,257,511,512,513,1023,1024,1025,2047,2048]
large=[2049,3000,4095,4096,4097,5000,8192,8193,8200]
extra=['zero','same_isolation','mixed_isolation','reuse','expand','classes','reclaim','large_pair','huge','no_pages',
       'oom_small','oom_16','free_slots_oom','oom_large','fragmented','raw_coexist','basic','stress_a','stress_b']
default=[f'size_{n}' for n in small+large]+extra+[f'N{i:02d}' for i in range(1,18)]
p=argparse.ArgumentParser(); p.add_argument('--tag',required=True); p.add_argument('--cases',nargs='*')
a=p.parse_args(); out=work/'evidence/slub_tests'/a.tag; out.mkdir(exist_ok=False)
shutil.copyfile(work/'tests/slub_host_tests.c',out/'slub_host_tests.c')
binary=out/'slub_host_tests'
command=['gcc','-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-fno-pie','-no-pie',
         '-std=gnu99','-fno-builtin','-Wall','-Werror','-Wno-unused','-O1','-g',
         '-Ilibs','-Ikern/mm','-Ikern/debug','-Ikern/driver','tests/slub_host_tests.c','-o',str(binary)]
def run(cmd,log,timeout=180):
    try:
        r=subprocess.run(cmd,cwd=work,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=timeout)
        text=r.stdout.decode(errors='replace'); code=r.returncode
    except subprocess.TimeoutExpired as e: text=(e.stdout or b'').decode(errors='replace')+'\nHARNESS_TIMEOUT'; code=124
    log.write_text('$ '+shlex.join(cmd)+'\n'+text+f'\nEXIT_CODE={code}\n')
    return code,text
code,text=run(command,out/'compile.log'); print('compile exit',code,flush=True)
if code: print(text); sys.exit(1)
results=[]
for case in a.cases or default:
    code,text=run([str(binary),case],out/(case+'.log'))
    ok=code==0 and f'PASS {case} ' in text and 'runtime error:' not in text and 'ERROR: AddressSanitizer' not in text
    result={'id':case,'status':'PASS' if ok else 'FAIL','exit_code':code,'last_line':text.strip().splitlines()[-1] if text.strip() else ''}
    results.append(result); print(case,result['status'],result['last_line'],flush=True)
    (out/'results.json').write_text(json.dumps(results,indent=2))
summary={'total':len(results),'passed':sum(r['status']=='PASS' for r in results),'failed':sum(r['status']=='FAIL' for r in results),
         'unexecuted':0,'negative_cases':sum(r['id'].startswith('N') for r in results)}
(out/'summary.json').write_text(json.dumps(summary,indent=2)); print(summary,flush=True)
sys.exit(0 if not summary['failed'] else 1)
