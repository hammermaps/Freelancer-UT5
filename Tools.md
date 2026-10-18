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

**Quellen** (`Tools/fltool/src/`): `Utf` (Container), `Ini` (Textform; BINI wird abgelehnt), `Vms` (VMeshData, Szenen, GLB-Schreiber), `Model` (Material-CRC, Materialien, Hardpoints, Konstrukte, Teile).
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

## Typischer Ablauf: Schiff nach Blender

```
fltool tex extract SHIPS/ORDER/or_ships.mat tex/ && python3 -I Tools/tex_to_png.py tex/ glb/
fltool cmp export SHIPS/ORDER/OR_ELITE/or_elite.cmp glb/or_elite.glb --mat SHIPS/ORDER/or_ships.mat
```

## create_test_level.py – `Tools/create_test_level.py`

Erzeugt `/Game/Freelancer/Maps/L_Test_Space` im Editor (`UnrealEditor-Cmd Freelancer.uproject -ExecutePythonScript=... -unattended -nullrhi`).

## Geplant

- Upscaling-Skript (Real-ESRGAN/PBRify) aus `GameData/Work/Freelancer-Textures/upscale_batch.py` nach `Tools/`, mit Ableitung von Normal/Roughness/Metallic/Emissive.
- Manifest-Werkzeug (alle Schiffe/Stationen mit ihren Dateien), Loose-/Sphere-Konstrukte, animierte Rev/Pris-Posen, Wireframes (VMeshWire).
