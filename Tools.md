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
| `fltool tex decode <bild> <out.pam>` | DDS/TGA mit dem eigenen Decoder (`Image`) nach RGBA (PAM) |
| `fltool model info <datei>` | Materialien (CRC, Typ, Textur) und Hardpoints |
| `fltool mesh export <datei> <out.glb> [--all-lods] [--mat <datei.mat>]` | VMesh-Bibliothek flach nach glTF (Teile liegen **nicht** an ihrer Position, nur zum Ansehen) |
| `fltool cmp export <datei.cmp> <out.glb> [--mat <datei.mat>]` | Compound-Modell mit Teilen, Konstrukt-Transformationen (Fix/Rev/Pris im Ruhezustand) und Hardpoints als Knoten `HP_<name>` |

**Quellen:** die Formatbibliothek liegt als UE-Modul in `Source/FreelancerFormats/` (Public/Private) und wird von `Tools/fltool/CMakeLists.txt` mitgebaut; die CLI steht in `Tools/fltool/src/main.cpp`. Module: `Utf` (Container), `Ini` (Textform; BINI wird abgelehnt), `Vms` (VMeshData, Szenen, GLB-Schreiber), `Model` (Material-CRC, Materialien, Hardpoints, Konstrukte, Teile), `ShipData` (shiparch.ini → `FlShipRecord`), `Records` (allgemeine INI-Datensätze: Ausrüstung, Waren), `Image` (TGA/DDS-Decoder → RGBA8, unterstützt TGA Typ 1/2/3/9/10/11 mit Palette, 8/16/24/32 bpp, DXT1/3/5 und unkomprimiertes DDS 16/24/32 bpp). Der Decoder wurde gegen Pillow an 1122 Original-Texturen verglichen (0 Fehler, mittlere Abweichung ≤ 0,71 von 255).
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

## render_glb.py (Python, numpy, matplotlib) – `Tools/render_glb.py`

```
python3 -I Tools/render_glb.py <datei.glb> <out.png>
```
Software-Darstellung einer `.glb` (z. B. von `fltool cmp export`) in drei Ansichten (Seite, Draufsicht, Vorn), schattiert nach Normalen mit Teile-Hierarchie. Dient der schnellen Prüfung von Form und Ausrichtung ohne Blender/Engine; hat den Index-Fehler beim VMesh-Import sichtbar gemacht. Test: `~/UEBuild/pyenv/bin/python -I Tools/tests/test_render_glb.py` (matplotlib in der venv installiert).

## Schiff im Level ansehen (Screenshot) – `Tools/create_ship_view_level.py`

1. Level erzeugen (alte Datei vorher verschieben, nicht löschen): `UnrealEditor-Cmd Freelancer.uproject -ExecutePythonScript=Tools/create_ship_view_level.py -unattended -nullrhi`. Es legt `L_ShipView` mit `AFLShipVisual` (Modell `DA_Model_li_fighter`), beweglichen Lichtern und einem `PlayerStart` mit Blick aufs Schiff an.
2. Screenshot aus dem Spiel mit Verzögerung: `DISPLAY=:0 UnrealEditor Freelancer.uproject /Game/Freelancer/Maps/L_ShipView -game -windowed -ResX=1280 -ResY=720 -nosound -log -ExecCmds="r.HighResScreenshotDelay 600,HighResShot 1280x720"`. Das Bild landet in `~/UEBuild/Freelancer/Saved/Screenshots/LinuxEditor/`; der Prozess endet nicht von selbst (danach `pkill -x UnrealEditor`). Screenshots aus Originaldaten **nicht ins Repo** legen.
3. Fallstricke, die zu schwarzen Bildern führten: (a) `EditorLevelLibrary.new_level` wechselte das Level nicht, die Actors landeten im Startlevel – `LevelEditorSubsystem.new_level` verwenden und das aktive Level prüfen; (b) Modell-Eigenschaft per Python mit dem geladenen Asset setzen, nicht mit `SoftObjectPath`; (c) statische Lichter sind ohne Lighting-Build schwarz – Mobilität `MOVABLE`; (d) Python-`Rotator` ist `(roll, pitch, yaw)`; (e) ohne `r.HighResScreenshotDelay` entsteht das Bild vor dem Shader-Kompilieren; (f) ältere Spielprozesse beenden, sie belegen die CPU.

## accept_ship_load.sh – Abnahmetest M1

`Tools/accept_ship_load.sh [Archive-Ordner]` startet den gekochten Server (`L_ShipView`, UDP 7777) und einen Client (`-nullrhi`), wartet und vergleicht die Log-Zeile `FLShipVisual …: Modell …, N Teile, N Meshes, N Hardpoints` beider Seiten. Bestanden = Zeilen gleich, mindestens ein `Join succeeded`, keine `LogOutputDevice: Error`-Zeilen. Voraussetzung: `Engine/cook_freelancer.sh` lief mit dem aktuellen Content (das Level-Skript erzeugt `L_ShipView`; der Cook-Aufruf enthält beide Karten). Exit-Code 0 = bestanden.

## accept_flight_net.sh – Abnahmetest M2 (Netzwerk-Flug)

`Tools/accept_flight_net.sh [Archive-Ordner]`: gekochter Server (`L_FlightTest`) + 2 Clients (`-nullrhi`, `-FLBot`, `t.MaxFPS 60`). Bestanden = 2 Joins, 0 Fehlerzeilen, Besitzer-Schiff Rolle 2 und fremdes Schiff Rolle 1 gemeldet, maximaler Vorhersagefehler des Besitzers < 150 cm. Level mit `Tools/create_flight_test_level.py` erzeugen (alte Datei vorher verschieben). Voraussetzung: `Engine/cook_freelancer.sh` mit aktuellem Code.

## load_flight_net.sh – Lasttest M2

`Tools/load_flight_net.sh [Clients=12] [Sekunden=60]`: gekochter Server + N Bot-Clients (`-nullrhi`, `-FLBot`, `t.MaxFPS 60`, je eigenes Log `load_client_<i>.log`). Gibt aus: Joins, Server-CPU/RAM/Frame-Zeit, Netz (Loopback-Bytes je Client), Client-RAM, max. Vorhersagefehler/Korrekturen, abgelehnte Bewegungen, Fehlerzeilen. Bestanden = alle Clients beigetreten, Fehler < 150 cm, keine Ablehnungen, Server-Tick ≤ 40 ms, keine Fehlerzeilen. Messwerte stehen in `PLAN.md` (M2). Grenze auf einem Rechner: RAM (ca. 350 MB je Bot-Client).

## accept_asteroids_net.sh – Abnahmetest Asteroidenfelder

`Tools/accept_asteroids_net.sh`: Server mit `-FLSystem=Li01` und Startregion mitten im Trümmerfeld `Zone_Li01_Pittsburgh_debris_001` (`-FLSpawnOffset=6338000,4767500,0`), 2 Bot-Clients. Bestanden = Felder aktiv, Bots kollidieren mit Asteroiden (`fest li01_…` im Server-Log), 2 Joins, 0 Fehlerzeilen, Server-Tick ≤ 40 ms.

## Typischer Ablauf: Schiff nach Blender

```
fltool tex extract SHIPS/ORDER/or_ships.mat tex/ && python3 -I Tools/tex_to_png.py tex/ glb/
fltool cmp export SHIPS/ORDER/OR_ELITE/or_elite.cmp glb/or_elite.glb --mat SHIPS/ORDER/or_ships.mat
```

## create_test_level.py – `Tools/create_test_level.py`

Erzeugt `/Game/Freelancer/Maps/L_Test_Space` im Editor (`UnrealEditor-Cmd Freelancer.uproject -ExecutePythonScript=... -unattended -nullrhi`).

## REA (Reverse Engineering) – MCP `rea`

Zum Analysieren der Original-EXE/-DLLs; Einrichtung, Werkzeuge und Regeln in `CLAUDE.md`, Abschnitt „Reverse Engineering (REA)“.

## Geplant

- Emissive-Ableitung; Qualitätsvergleich der Modelle (Real-ESRGAN, PBRify, GTAV) an einer größeren Stichprobe; Stapel-Lauf über alle importierten Texturen.
- Manifest-Werkzeug (alle Schiffe/Stationen mit ihren Dateien), Loose-/Sphere-Konstrukte, animierte Rev/Pris-Posen, Wireframes (VMeshWire).
