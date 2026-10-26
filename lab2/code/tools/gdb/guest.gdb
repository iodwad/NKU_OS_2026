set pagination off
set print pretty on
set remotetimeout 120
python
import gdb
from pathlib import Path
pc = int(gdb.parse_and_eval('$pc'))
if pc < 0x80000000:
    gdb.execute('thbreak kern_init')
    gdb.execute('continue')
elif pc < 0xffffffffc0000000:
    gdb.execute('thbreak kern_init')
    gdb.execute('continue')
gdb.execute('source ' + str(Path('tools/gdb/sv39_probe.py').resolve()))
end
printf "\nGuest debugger ready. Resume host GDB before every guest command.\n"
info registers pc satp
x/4i $pc
printf "Optional controlled test: sv39-probe-prepare; stepi three times; sv39-probe-restore; stepi; sv39-probe-restore-finish\n"
