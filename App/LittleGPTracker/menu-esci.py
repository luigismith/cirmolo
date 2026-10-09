"""Uscita da LittleGPTracker con il tasto MENU della Flip, tenuto premuto un secondo.
LGPT non ha un tasto di uscita per il solo gamepad (dentro un progetto serve Esc). SDL trasforma il SIGTERM
in un evento di chiusura regolare, quindi basta mandarlo. Salva prima: le modifiche non salvate si perdono.
Legge il pad "MIYOO Player1" per nome, perche' il numero di event* cambia con un dispositivo USB."""
import os
import select
import signal
import struct
import subprocess
import time

MENU = 316
HOLD = 1.0


def pad_path():
    found = False
    try:
        for line in open('/proc/bus/input/devices'):
            if line.startswith('N: Name="MIYOO Player1"'):
                found = True
            elif found and line.startswith('H: Handlers='):
                for tok in line.split('=', 1)[1].split():
                    if tok.startswith('event'):
                        return '/dev/input/' + tok
            elif not line.strip():
                found = False
    except OSError:
        pass
    return os.environ.get('EVENT_PATH_READ_INPUTS_SPRUCE', '/dev/input/event5')


fmt = 'llHHi'
size = struct.calcsize(fmt)
with open(pad_path(), 'rb', buffering=0) as f:
    down = None
    while True:
        r, _, _ = select.select([f], [], [], 0.2)
        if r:
            data = f.read(size)
            if len(data) == size:
                _, _, etype, code, value = struct.unpack(fmt, data)
                if etype == 1 and code == MENU:
                    down = time.time() if value else None
        if down is not None and time.time() - down >= HOLD:
            subprocess.call(['killall', '-q', '-15', 'lgpt'])
            break
