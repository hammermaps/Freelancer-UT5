#!/usr/bin/env python3
"""Einfache Software-Darstellung einer .glb (von `fltool cmp export`) in drei Ansichten – zum schnellen Pruefen von Form und Ausrichtung ohne Blender/Engine.

  render_glb.py <datei.glb> <out.png>
Ansichten (Originalkoordinaten: x rechts, y oben, -z vorn): Seite (z, y), Draufsicht (x, z), Vorderansicht (x, y). Flaechen nach Normale schattiert, Painter-Sortierung.
"""
import json
import struct
import sys

import numpy as np


def load_glb(path):
    data = open(path, 'rb').read()
    if data[:4] != b'glTF':
        raise ValueError('keine GLB-Datei')
    n = struct.unpack('<I', data[12:16])[0]
    doc = json.loads(data[20:20 + n])
    off = 20 + n
    binary = b''
    if off + 8 <= len(data):
        bn = struct.unpack('<I', data[off:off + 4])[0]
        binary = data[off + 8:off + 8 + bn]
    return doc, binary


def accessor(doc, binary, idx):
    a = doc['accessors'][idx]
    v = doc['bufferViews'][a['bufferView']]
    comps = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3}[a['type']]
    dt = {5126: np.float32, 5123: np.uint16}[a['componentType']]
    start = v.get('byteOffset', 0) + a.get('byteOffset', 0)
    return np.frombuffer(binary, dt, a['count'] * comps, start).reshape(a['count'], comps)


def quat_matrix(q):
    x, y, z, w = q
    return np.array([
        [1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
        [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
        [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])


def world_triangles(doc, binary):
    """Alle Dreiecke in Weltkoordinaten: Array (N, 3, 3)."""
    nodes = doc['nodes']
    parent = {}
    for i, n in enumerate(nodes):
        for c in n.get('children', []):
            parent[c] = i

    def world(i):
        n = nodes[i]
        m = np.eye(4)
        m[:3, :3] = quat_matrix(n.get('rotation', [0, 0, 0, 1]))
        m[:3, 3] = n.get('translation', [0, 0, 0])
        return world(parent[i]) @ m if i in parent else m

    tris = []
    for i, n in enumerate(nodes):
        if 'mesh' not in n:
            continue
        m = world(i)
        for p in doc['meshes'][n['mesh']]['primitives']:
            pos = accessor(doc, binary, p['attributes']['POSITION']).astype(float)
            idx = accessor(doc, binary, p['indices'])[:, 0].astype(int)
            pw = pos @ m[:3, :3].T + m[:3, 3]
            tris.append(pw[idx.reshape(-1, 3)])
    return np.concatenate(tris) if tris else np.zeros((0, 3, 3))


def render(tris, out):
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    from matplotlib.collections import PolyCollection

    views = [('Seite (z, y) - Nase links', 2, 1, 0), ('Draufsicht (x, z)', 0, 2, 1), ('Vorn (x, y)', 0, 1, 2)]
    n = np.cross(tris[:, 1] - tris[:, 0], tris[:, 2] - tris[:, 0])
    ln = np.linalg.norm(n, axis=1)
    n = n / np.where(ln == 0, 1, ln)[:, None]
    light = np.array([0.4, 0.8, 0.5]); light /= np.linalg.norm(light)
    shade = 0.25 + 0.75 * np.abs(n @ light)
    fig, axes = plt.subplots(1, 3, figsize=(18, 6), facecolor='#202428')
    for ax, (title, a, b, depth) in zip(axes, views):
        order = np.argsort(tris[:, :, depth].mean(axis=1))
        polys = tris[order][:, :, [a, b]]
        cols = np.repeat(shade[order][:, None], 3, axis=1)
        cols = np.concatenate([cols, np.ones((len(cols), 1))], axis=1)
        ax.add_collection(PolyCollection(polys, facecolors=cols, edgecolors='none'))
        lo, hi = tris[:, :, [a, b]].reshape(-1, 2).min(0), tris[:, :, [a, b]].reshape(-1, 2).max(0)
        pad = (hi - lo).max() * 0.05
        ax.set_xlim(lo[0] - pad, hi[0] + pad); ax.set_ylim(lo[1] - pad, hi[1] + pad)
        ax.set_aspect('equal'); ax.set_title(title, color='white'); ax.set_facecolor('#101316')
        ax.tick_params(colors='gray')
    fig.savefig(out, dpi=80, facecolor=fig.get_facecolor())


def main(argv):
    if len(argv) != 2:
        sys.exit(__doc__)
    doc, binary = load_glb(argv[0])
    tris = world_triangles(doc, binary)
    if not len(tris):
        print('keine Dreiecke', file=sys.stderr)
        return 1
    render(tris, argv[1])
    lo, hi = tris.reshape(-1, 3).min(0), tris.reshape(-1, 3).max(0)
    print(f'{len(tris)} Dreiecke, Ausdehnung x={hi[0]-lo[0]:.2f} y={hi[1]-lo[1]:.2f} z={hi[2]-lo[2]:.2f}')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
