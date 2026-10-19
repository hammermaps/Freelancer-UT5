# Tools.md – Hilfswerkzeuge

Offline-Werkzeuge für Entwicklung und Asset-Pipeline. Sie sind **nicht Teil des Spiels**. Kontext: `CLAUDE.md`, Plan: `PLAN.md` (Entscheidung 14, M1b).
Regel: Zu jedem Werkzeug gehören Tests im selben Commit (siehe `CLAUDE.md`, Konventionen).

## fltool (C++17, CMake) – `Tools/fltool/`

Bibliothek `flformats` (wird später vom `FLImport`-Modul mitverwendet) plus CLI `fltool`.

**Bauen und testen** (Build-Ordner liegt lokal, nicht auf dem NFS-Share):
```
cmake -S Tools/fltool -B ~/UEBuild/fltool -DCMAKE_BUILD_TYPE=Release
cmake --build ~/UEBuild/fltool
ctest --test-dir ~/UEBuild/fltool --output-on-failure
```

| Befehl | Zweck |
|---|---|
| `fltool utf ls <datei>` | UTF-Baum ausgeben (`.cmp .3db .mat .txm .utf`) |
| `fltool utf extract <datei> <ordner>` | alle Blattdaten als Dateien schreiben |
| `fltool ini sections <datei>` | INI-Sektionen mit Eintragsanzahl |
| `fltool tex extract <datei> <ordner>` | je Textur die größte MIP-Stufe als `.dds`/`.tga` |
| `fltool model info <datei>` | Materialien (CRC, Typ, Textur) und Hardpoints |
| `fltool mesh export <datei> <out.glb> [--all-lods] [--mat <datei.mat>]` | VMesh-Bibliothek flach nach glTF (Teile liegen **nicht** an ihrer Position, nur zum Ansehen) |
| `fltool cmp export <datei.cmp> <out.glb> [--mat <datei.mat>]` | Compound-Modell mit Teilen, Konstrukt-Transformationen (Fix/Rev/Pris im Ruhezustand) und Hardpoints als Knoten `HP_<name>` |

**Quellen:** die Formatbibliothek liegt als UE-Modul in `Source/FreelancerFormats/` (Public/Private) und wird von `Tools/fltool/CMakeLists.txt` mitgebaut; die CLI steht in `Tools/fltool/src/main.cpp`. Module: `Utf` (Container), `Ini` (Textform; BINI wird abgelehnt), `Vms` (VMeshData, Szenen, GLB-Schreiber), `Model` (Material-CRC, Materialien, Hardpoints, Konstrukte, Teile), `ShipData` (shiparch.ini → `FlShipRecord`).
**Tests**: `Tools/fltool/tests/test_main.cpp` (nur synthetische Daten).
**Formatreferenz**: Librelancer (MIT), `Reference/Librelancer/src/LibreLancer/Utf/`. Die CRC-Tabelle in `src/CrcTable.inc` stammt von dort.

**Geprüft gegen die Originaldaten** (lokal, `GameData/FL/DATA`, nicht im Repo):
UTF 400/400, INI 1252/1252, Texturen 2062 aus 600 Containern, Meshes 1478/1478 (`.cmp`/`.3db`), Compound-Export 511/511 `.cmp`, Material-CRCs von `or_elite` vollständig in den `.mat` gefunden.

**Konventionen im Export**: Koordinaten wie im Original (keine Achsenumrechnung, Blender-Ausrichtung noch ungeprüft); Rotationen sind 3×3 zeilenweise (Spaltenvektor-Konvention) und werden zu Quaternionen; Texturverweise im glTF lauten `<name>.png` (neben die GLB legen, siehe `tex_to_png.py`).

## tex_to_png.py (Python, Pillow) – `Tools/tex_to_png.py`

```
python3 -I Tools/tex_to_png.py <eingabeordner> <ausgabeordner>
```
Wandelt die von `fltool tex extract` erzeugten `.dds`/`.tga` (inkl. 16-Bit-TGA, DXT) in PNG. Exit-Code 1, wenn Dateien fehlschlagen.
Test: `python3 -I Tools/tests/test_tex_to_png.py`.

## upscale_textures.py (Python, Pillow, numpy; Torch/spandrel nur mit Modell) – `Tools/upscale_textures.py`

```
python3 -I Tools/upscale_textures.py <png-ordner> <ausgabe> --model GameData/Work/Freelancer-Textures/models/RealESRGAN_x4plus.pth
python3 -I Tools/upscale_textures.py <png-ordner> <ausgabe> --no-ml     # nur Lanczos, ohne GPU
```
Je `name.png` entstehen `name.png` (hochskaliert, Alpha bleibt), `name_n.png` (Normal Map aus der Helligkeit, OpenGL-Konvention), `name_r.png` (Roughness), `name_m.png` (Metallic, vorerst 0; Feintuning pro Material im Editor).
Modelle: Real-ESRGAN x4plus, 4x-PBRify, 4xTextures_GTAV (liegen in `GameData/Work/Freelancer-Textures/models/`); `GameData/Work/Freelancer-Textures/upscale_batch.py` bleibt als Vergleichs-/Auswahl-Skript.
Umgebung: lokale venv `~/UEBuild/pyenv` (`python3 -m venv --system-site-packages ~/UEBuild/pyenv && ~/UEBuild/pyenv/bin/pip install numpy`); Torch 2.14.1 (CUDA 13) und spandrel sind dort installiert (`pip install torch spandrel --index-url https://download.pytorch.org/whl/cu128 --extra-index-url https://pypi.org/simple`); GPU RTX 5060 (8 GB) wird erkannt, ohne CUDA fällt das Skript auf die CPU zurück. Getestet mit Real-ESRGAN x4plus und 4x-PBRify (3 Texturen, ca. 11 s inkl. Modellladen).
Test: `~/UEBuild/pyenv/bin/python -I Tools/tests/test_upscale_textures.py`.

## Typischer Ablauf: Schiff nach Blender

```
fltool tex extract SHIPS/ORDER/or_ships.mat tex/ && python3 -I Tools/tex_to_png.py tex/ glb/
fltool cmp export SHIPS/ORDER/OR_ELITE/or_elite.cmp glb/or_elite.glb --mat SHIPS/ORDER/or_ships.mat
```

## create_test_level.py – `Tools/create_test_level.py`

Erzeugt `/Game/Freelancer/Maps/L_Test_Space` im Editor (`UnrealEditor-Cmd Freelancer.uproject -ExecutePythonScript=... -unattended -nullrhi`).

## Geplant

- Emissive-Ableitung; Qualitätsvergleich der Modelle (Real-ESRGAN, PBRify, GTAV) an einer größeren Stichprobe; Stapel-Lauf über alle importierten Texturen.
- Manifest-Werkzeug (alle Schiffe/Stationen mit ihren Dateien), Loose-/Sphere-Konstrukte, animierte Rev/Pris-Posen, Wireframes (VMeshWire).
