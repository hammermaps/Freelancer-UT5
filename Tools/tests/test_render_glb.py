import json, struct, sys, tempfile, unittest
from pathlib import Path
import numpy as np
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
import render_glb as R


def make_glb(path, parent_translation=(10, 0, 0)):
    pos = np.array([[0, 0, 0], [1, 0, 0], [0, 1, 0]], np.float32).tobytes()
    idx = np.array([0, 1, 2], np.uint16).tobytes() + b'\0\0'
    binary = pos + idx
    doc = {'asset': {'version': '2.0'}, 'scene': 0, 'scenes': [{'nodes': [0]}],
           'nodes': [{'name': 'p', 'translation': list(parent_translation), 'rotation': [0, 0, 0, 1], 'children': [1]},
                     {'name': 'c', 'translation': [0, 5, 0], 'rotation': [0, 0, 0, 1], 'mesh': 0}],
           'meshes': [{'primitives': [{'attributes': {'POSITION': 0}, 'indices': 1}]}],
           'accessors': [{'bufferView': 0, 'componentType': 5126, 'count': 3, 'type': 'VEC3'},
                         {'bufferView': 1, 'componentType': 5123, 'count': 3, 'type': 'SCALAR'}],
           'bufferViews': [{'buffer': 0, 'byteOffset': 0, 'byteLength': len(pos)}, {'buffer': 0, 'byteOffset': len(pos), 'byteLength': 6}],
           'buffers': [{'byteLength': len(binary)}]}
    j = json.dumps(doc).encode()
    j += b' ' * (-len(j) % 4)
    out = b'glTF' + struct.pack('<II', 2, 12 + 8 + len(j) + 8 + len(binary))
    out += struct.pack('<II', len(j), 0x4E4F534A) + j + struct.pack('<II', len(binary), 0x004E4942) + binary
    Path(path).write_bytes(out)


class RenderGlb(unittest.TestCase):
    def test_world_transform_applies_hierarchy(self):
        with tempfile.TemporaryDirectory() as t:
            f = Path(t, 'a.glb'); make_glb(f)
            tris = R.world_triangles(*R.load_glb(f))
            self.assertEqual(tris.shape, (1, 3, 3))
            self.assertTrue(np.allclose(tris[0, 0], [10, 5, 0]))     # Eltern + Kind
            self.assertTrue(np.allclose(tris[0, 2], [10, 6, 0]))

    def test_quaternion_rotation(self):
        m = R.quat_matrix([0, 0, np.sin(np.pi / 4), np.cos(np.pi / 4)])   # 90 Grad um Z
        self.assertTrue(np.allclose(m @ [1, 0, 0], [0, 1, 0], atol=1e-6))

    def test_rejects_non_glb(self):
        with tempfile.TemporaryDirectory() as t:
            f = Path(t, 'x.glb'); f.write_bytes(b'nope' * 10)
            with self.assertRaises(ValueError):
                R.load_glb(f)

    def test_main_writes_png(self):
        with tempfile.TemporaryDirectory() as t:
            f, o = Path(t, 'a.glb'), Path(t, 'o.png'); make_glb(f)
            self.assertEqual(R.main([str(f), str(o)]), 0)
            self.assertGreater(o.stat().st_size, 1000)


if __name__ == '__main__':
    unittest.main()
