import sys, tempfile, unittest
from pathlib import Path
from PIL import Image
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
import tex_to_png

class TexToPng(unittest.TestCase):
    def test_converts_tga_and_reports_bad_files(self):
        with tempfile.TemporaryDirectory() as t:
            src, dst = Path(t, 'src'), Path(t, 'dst')
            (src / 'sub').mkdir(parents=True)
            Image.new('RGB', (4, 4), (255, 0, 0)).save(src / 'sub' / 'a.tga')
            self.assertEqual(tex_to_png.main(src, dst), 0)
            self.assertEqual(Image.open(dst / 'sub' / 'a.png').size, (4, 4))
            (src / 'broken.dds').write_bytes(b'nope')
            self.assertEqual(tex_to_png.main(src, dst), 1)

if __name__ == '__main__':
    unittest.main()
