import sys, tempfile, unittest
from pathlib import Path
import numpy as np
from PIL import Image
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
import upscale_textures as U


class Derive(unittest.TestCase):
    def test_flat_image_gives_flat_normal(self):
        n = U.derive_normal(np.full((8, 8, 3), 0.5, np.float32))
        self.assertEqual(n.shape, (8, 8, 3))
        self.assertTrue((n[..., 0] == 128).all() and (n[..., 1] == 128).all() and (n[..., 2] == 255).all())

    def test_slope_tilts_normal(self):
        ramp = np.tile(np.linspace(0, 1, 16, dtype=np.float32), (16, 1))
        n = U.derive_normal(np.stack([ramp] * 3, -1))
        self.assertLess(n[8, 8, 0], 128)      # steigt nach rechts -> Normale zeigt nach links
        self.assertEqual(n[8, 8, 1], 128)

    def test_roughness_and_metallic(self):
        dark, bright = np.zeros((2, 2, 3), np.float32), np.ones((2, 2, 3), np.float32)
        self.assertGreater(U.derive_roughness(dark)[0, 0], U.derive_roughness(bright)[0, 0])
        self.assertTrue((U.derive_metallic(bright) == 0).all())


class Pipeline(unittest.TestCase):
    def test_no_ml_writes_four_maps_and_keeps_alpha(self):
        with tempfile.TemporaryDirectory() as t:
            src, dst = Path(t, 's'), Path(t, 'd')
            (src / 'sub').mkdir(parents=True)
            Image.new('RGBA', (4, 4), (200, 100, 50, 128)).save(src / 'sub' / 'a.png')
            self.assertEqual(U.main([str(src), str(dst), '--no-ml']), 0)
            for suffix in ('', '_n', '_r', '_m'):
                self.assertTrue((dst / 'sub' / f'a{suffix}.png').exists())
            big = Image.open(dst / 'sub' / 'a.png')
            self.assertEqual(big.size, (16, 16))
            self.assertEqual(big.getchannel('A').getpixel((0, 0)), 128)

    def test_broken_file_is_reported(self):
        with tempfile.TemporaryDirectory() as t:
            src, dst = Path(t, 's'), Path(t, 'd')
            src.mkdir()
            (src / 'x.png').write_bytes(b'kaputt')
            self.assertEqual(U.main([str(src), str(dst), '--no-ml']), 1)

    def test_requires_model_or_no_ml(self):
        with self.assertRaises(SystemExit):
            U.main(['a', 'b'])


if __name__ == '__main__':
    unittest.main()
