#!/usr/bin/env python3
"""Wandelt die von `fltool tex extract` erzeugten .dds/.tga in PNG um (Offline-Werkzeug, nicht Teil des Spiels).

  tex_to_png.py <eingabeordner> <ausgabeordner>
"""
import sys
from pathlib import Path
from PIL import Image

def main(src, dst):
    src, dst = Path(src), Path(dst)
    ok = bad = 0
    for f in sorted(src.rglob('*')):
        if f.suffix.lower() not in ('.dds', '.tga'):
            continue
        out = dst / f.relative_to(src).with_suffix('.png')
        try:
            im = Image.open(f)
            im.load()
            if im.mode not in ('RGB', 'RGBA', 'L', 'LA'):
                im = im.convert('RGBA')
            out.parent.mkdir(parents=True, exist_ok=True)
            im.save(out)
            ok += 1
        except Exception as e:
            bad += 1
            print(f'FEHLER {f}: {e}', file=sys.stderr)
    print(f'{ok} konvertiert, {bad} Fehler')
    return 1 if bad else 0

if __name__ == '__main__':
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    sys.exit(main(*sys.argv[1:]))
