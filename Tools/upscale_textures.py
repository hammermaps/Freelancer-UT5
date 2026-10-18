#!/usr/bin/env python3
"""Texturen hochskalieren und PBR-Maps ableiten (Offline-Werkzeug, nicht Teil des Spiels).

  upscale_textures.py <eingabe-png-ordner> <ausgabeordner> [--model <modell.pth>] [--scale 4] [--device cuda|cpu] [--no-ml]

Je Eingabe `name.png` entstehen:
  name.png    hochskaliert (Real-ESRGAN/PBRify ueber spandrel; mit --no-ml nur Lanczos)
  name_n.png  Normal Map (OpenGL-Konvention, aus der Helligkeit abgeleitet)
  name_r.png  Roughness (Grauwert)
  name_m.png  Metallic (Grauwert)
Torch und spandrel werden nur geladen, wenn ein Modell verwendet wird.
Die Modelle liegen in GameData/Work/Freelancer-Textures/models/.
"""
import argparse
import sys
from pathlib import Path

import numpy as np
from PIL import Image


def luminance(rgb):
    return rgb[..., 0] * 0.2126 + rgb[..., 1] * 0.7152 + rgb[..., 2] * 0.0722


def derive_normal(rgb, strength=2.0):
    """Normal Map aus Helligkeit als Hoehenfeld (Sobel). rgb: float32 HxWx3 in 0..1. Rueckgabe uint8 HxWx3."""
    h = luminance(rgb)
    p = np.pad(h, 1, mode='edge')
    dx = (p[:-2, 2:] + 2 * p[1:-1, 2:] + p[2:, 2:]) - (p[:-2, :-2] + 2 * p[1:-1, :-2] + p[2:, :-2])
    dy = (p[2:, :-2] + 2 * p[2:, 1:-1] + p[2:, 2:]) - (p[:-2, :-2] + 2 * p[:-2, 1:-1] + p[:-2, 2:])
    n = np.stack([-dx * strength, dy * strength, np.ones_like(h)], axis=-1)
    n /= np.linalg.norm(n, axis=-1, keepdims=True)
    return ((n * 0.5 + 0.5) * 255).round().astype(np.uint8)


def derive_roughness(rgb):
    """Dunkle, kontrastarme Flaechen rauer; helle, kontrastreiche glatter. Grauwert uint8."""
    h = luminance(rgb)
    return ((1.0 - 0.5 * h) * 255).clip(0, 255).round().astype(np.uint8)


def derive_metallic(rgb):
    """Konservativer Standard: nichts metallisch. Feintuning geschieht pro Material im Editor."""
    return np.zeros(rgb.shape[:2], np.uint8)


def to_rgb_alpha(im):
    im = im.convert('RGBA')
    a = np.asarray(im)
    return a[..., :3].astype(np.float32) / 255.0, a[..., 3]


def resolve_device(requested, cuda_available):
    """Faellt auf CPU zurueck, wenn CUDA gewuenscht, aber nicht verfuegbar ist."""
    return 'cpu' if requested.startswith('cuda') and not cuda_available else requested


def load_model(path, device):
    import torch
    import spandrel
    m = spandrel.ModelLoader().load_from_file(str(path)).model.eval().to(device)
    return m, torch


def ml_upscale(model_and_torch, im, device):
    model, torch = model_and_torch
    x = torch.from_numpy(np.array(im.convert('RGB'))).permute(2, 0, 1).unsqueeze(0).float().to(device) / 255
    with torch.inference_mode():
        y = model(x)
    return Image.fromarray((y[0].clamp(0, 1).permute(1, 2, 0).cpu().numpy() * 255).round().astype(np.uint8))


def process(src, dst, model, scale, device):
    im = Image.open(src)
    im.load()
    size = (im.width * scale, im.height * scale)
    alpha = im.getchannel('A') if im.mode in ('RGBA', 'LA') else None
    rgb = ml_upscale(model, im, device) if model else im.convert('RGB').resize(size, Image.LANCZOS)
    if rgb.size != size:
        rgb = rgb.resize(size, Image.LANCZOS)
    arr = np.asarray(rgb).astype(np.float32) / 255.0
    out = rgb.convert('RGBA')
    if alpha is not None:
        out.putalpha(alpha.resize(size, Image.LANCZOS))
    out.save(dst / f'{src.stem}.png')
    Image.fromarray(derive_normal(arr)).save(dst / f'{src.stem}_n.png')
    Image.fromarray(derive_roughness(arr)).save(dst / f'{src.stem}_r.png')
    Image.fromarray(derive_metallic(arr)).save(dst / f'{src.stem}_m.png')


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('src')
    ap.add_argument('dst')
    ap.add_argument('--model')
    ap.add_argument('--scale', type=int, default=4)
    ap.add_argument('--device', default='cuda')
    ap.add_argument('--no-ml', action='store_true', help='nur Lanczos statt KI-Modell')
    a = ap.parse_args(argv)
    if not a.no_ml and not a.model:
        ap.error('--model angeben oder --no-ml verwenden')
    device = a.device
    if not a.no_ml:
        import torch
        device = resolve_device(a.device, torch.cuda.is_available())
    model = None if a.no_ml else load_model(a.model, device)
    src, dst = Path(a.src), Path(a.dst)
    dst.mkdir(parents=True, exist_ok=True)
    ok = bad = 0
    for f in sorted(src.rglob('*.png')):
        try:
            rel = dst / f.relative_to(src).parent
            rel.mkdir(parents=True, exist_ok=True)
            process(f, rel, model, a.scale, device)
            ok += 1
        except Exception as e:
            bad += 1
            print(f'FEHLER {f}: {e}', file=sys.stderr)
    print(f'{ok} verarbeitet, {bad} Fehler')
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
