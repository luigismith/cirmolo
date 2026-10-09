"""Stampa SONAME, DT_NEEDED e stringhe di versione di un ELF a 64 bit (ARM64/x86_64).
Uso: python -I elfdeps.py file [file...]
Serve per verificare che il binario per la Flip dipenda solo dalle librerie presenti sulla console."""
import re
import struct
import sys


def deps(path):
    d = open(path, 'rb').read()
    if d[:4] != b'\x7fELF' or d[4] != 2:
        return None, [], [], None
    machine = struct.unpack_from('<H', d, 0x12)[0]
    e_shoff = struct.unpack_from('<Q', d, 0x28)[0]
    e_shentsize, e_shnum, _ = struct.unpack_from('<HHH', d, 0x3a)
    secs = [struct.unpack_from('<IIQQQQIIQQ', d, e_shoff + i * e_shentsize) for i in range(e_shnum)]
    dyn = [s for s in secs if s[1] == 6]
    if not dyn:
        return machine, [], [], None
    dyn = dyn[0]
    dynstr = secs[dyn[6]]
    strtab = d[dynstr[4]:dynstr[4] + dynstr[5]]

    def s(off):
        return strtab[off:strtab.index(b'\0', off)].decode()

    need, soname = [], None
    for off in range(dyn[4], dyn[4] + dyn[5], 16):
        tag, val = struct.unpack_from('<qQ', d, off)
        if tag == 1:
            need.append(s(val))
        elif tag == 14:
            soname = s(val)
        elif tag == 0:
            break
    vers = sorted(set(m.decode() for m in re.findall(rb'GLIBC_2\.[0-9]+|GLIBCXX_3\.4\.[0-9]+|CXXABI_1\.3\.[0-9]+', d)))
    return machine, need, vers, soname


for f in sys.argv[1:]:
    machine, need, vers, soname = deps(f)
    print(f)
    print('  machine: %s' % {183: 'aarch64', 62: 'x86_64'}.get(machine, machine))
    print('  soname :', soname)
    print('  needed :', ', '.join(need))
    print('  simboli versionati:', ', '.join(vers))
