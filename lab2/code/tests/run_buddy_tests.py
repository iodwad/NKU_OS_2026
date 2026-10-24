"""Run each case as an isolated process; save commands, raw output and status.
Use --tag to retain earlier runs. No third-party Python modules required.
"""
from pathlib import Path
import subprocess, json, shlex, argparse, sys
work = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--tag',default='host_initial')
parser.add_argument('--sanitize',action='store_true')
parser.add_argument('--cases',nargs='*')
args = parser.parse_args()
out = work/'evidence/buddy_tests'/args.tag
out.mkdir(exist_ok=False)
binary = out/'buddy_host_tests'
cmd = ['gcc','-std=gnu99','-fno-builtin','-Wall','-Werror','-Wno-unused','-O1','-g',
       '-Ilibs','-Ikern/mm','-Ikern/debug','-Ikern/driver',
       'tests/buddy_host_tests.c','-o',str(binary)]
if args.sanitize: cmd[1:1] = ['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-fno-pie','-no-pie']
def execute(command,log,timeout=45):
    try:
        p = subprocess.run(command,cwd=work,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=timeout)
        text = p.stdout.decode(errors='replace'); code = p.returncode
    except subprocess.TimeoutExpired as error:
        text = (error.stdout or b'').decode(errors='replace')+'\nTIMEOUT'; code = 124
    log.write_text('$ '+shlex.join(command)+'\n'+text+f'\nEXIT_CODE={code}\n')
    return code,text
code,text = execute(cmd,out/'compile.log')
print('compile exit:',code)
if code: print(text); sys.exit(1)
results=[]
for case in args.cases or [f'H{i:02d}' for i in range(1,44)]:
    code,text=execute([str(binary),case],out/(case+'.log'))
    passed=code==0 and f'PASS {case}' in text
    results.append({'id':case,'status':'PASS' if passed else 'FAIL','exit_code':code,'output':text})
    print(case,results[-1]['status'],text.strip().splitlines()[-1] if text.strip() else '')
(out/'results.json').write_text(json.dumps(results,indent=2))
summary={'total':len(results),'passed':sum(r['status']=='PASS' for r in results),
         'failed':sum(r['status']=='FAIL' for r in results),'unexecuted':0}
(out/'summary.json').write_text(json.dumps(summary,indent=2)); print(summary)
sys.exit(1 if summary['failed'] else 0)
