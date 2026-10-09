#!/usr/bin/env python3
"""Testi da tradurre delle app native di Cirmolo (C, kit spruce/cirmolo-kit).

I testi nel codice sono in italiano e passano da tr("...") o, se stanno in una tabella, sono segnati con
N_("..."). Questo script li raccoglie da App/<app>/src/*.c (piu' i file del kit che l'app compila, indicati
in src/build.sh) e aggiorna App/<app>/lang/<Lingua>.json: conserva le traduzioni che ci sono, aggiunge le
chiavi nuove vuote (una traduzione vuota vale "resta in italiano") e toglie quelle non piu' usate.

Uso:
  python .github/i18n/estrai_native.py App/ClaudeChat            # aggiorna tutti i file di lang/
  python .github/i18n/estrai_native.py App/ClaudeChat --check    # esce con 1 se mancano traduzioni
  python .github/i18n/estrai_native.py App/ClaudeChat --list     # stampa le chiavi in JSON
"""
import json
import os
import re
import sys

STR = r'"((?:[^"\\]|\\.)*)"'
CALL = re.compile(r'\b(?:tr|N_)\(\s*' + STR + r'\s*\)')


def c_unescape(s):
    out = bytearray()
    i = 0
    b = s.encode('utf-8')
    while i < len(b):
        c = b[i]
        if c != 0x5C:
            out.append(c)
            i += 1
            continue
        n = chr(b[i + 1])
        i += 2
        if n == 'n':
            out.append(10)
        elif n == 't':
            out.append(9)
        elif n == 'r':
            out.append(13)
        elif n == 'x':
            m = re.match(rb'[0-9a-fA-F]{1,2}', b[i:])
            out.append(int(m.group(0), 16))
            i += len(m.group(0))
        elif n in '01234567':
            m = re.match(rb'[0-7]{0,2}', b[i:])
            out.append(int(n + m.group(0).decode(), 8))
            i += len(m.group(0))
        else:
            out.append(ord(n))
    return out.decode('utf-8')


def sources(app):
    src = os.path.join(app, 'src')
    files = [os.path.join(src, f) for f in sorted(os.listdir(src)) if f.endswith('.c') and not f.startswith('main_test')]
    build = os.path.join(src, 'build.sh')
    if os.path.exists(build):
        kit = os.path.normpath(os.path.join(src, '../../../spruce/cirmolo-kit'))
        for name in re.findall(r'\$KIT/(\w+\.c)', open(build, encoding='utf-8').read()):
            p = os.path.join(kit, name)
            if p not in files and os.path.exists(p):
                files.append(p)
    return files


def keys(app):
    seen = []
    for f in sources(app):
        text = open(f, encoding='utf-8').read()
        text = re.sub(r'/\*.*?\*/', '', text, flags=re.S)
        for m in CALL.finditer(text):
            k = c_unescape(m.group(1))
            if k and k not in seen:
                seen.append(k)
    return seen


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    if not args:
        print(__doc__)
        return 2
    app = args[0]
    ks = keys(app)
    if '--list' in sys.argv:
        print(json.dumps(ks, ensure_ascii=False, indent=2))
        return 0
    langdir = os.path.join(app, 'lang')
    os.makedirs(langdir, exist_ok=True)
    missing_any = False
    for name in sorted(os.listdir(langdir)):
        if not name.endswith('.json'):
            continue
        path = os.path.join(langdir, name)
        old = json.load(open(path, encoding='utf-8-sig'))
        new = {k: old.get(k, '') for k in ks}
        missing = [k for k in ks if not new[k]]
        bad = [k for k in ks if new[k] and re.findall(r'%[-0-9.]*[a-z]+', k) != re.findall(r'%[-0-9.]*[a-z]+', new[k])]
        if '--check' in sys.argv:
            if missing or bad:
                missing_any = True
            for k in bad:
                print(f'{name}: segnaposto diversi in {k!r}')
            print(f'{name}: {len(ks) - len(missing)}/{len(ks)} tradotti' + (f', {len(bad)} con segnaposto sbagliati' if bad else ''))
            continue
        with open(path, 'w', encoding='utf-8', newline='\n') as f:
            json.dump(new, f, ensure_ascii=False, indent=2)
            f.write('\n')
        print(f'{name}: {len(ks)} chiavi, {len(missing)} da tradurre, {len(set(old) - set(ks))} tolte')
    return 1 if missing_any else 0


if __name__ == '__main__':
    sys.exit(main())
