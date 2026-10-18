# CLAUDE.md

Dieses Verzeichnis ist ein **Unreal-Engine-5.8-C++-Projekt** (`Freelancer.uproject`, Modul `Freelancer`), das das Weltraumspiel
**Freelancer (2003, Digital Anvil/Microsoft)** nachbaut. Siehe `PLAN.md` für Ziele, Meilensteine und Risiken.

**Grundsatz: alles wird vollständig in Unreal Engine 5 umgesetzt** (Client, Server, Importer, Werkzeuge). Keine C#-/.NET-/Python-Laufzeit
im Produkt. Librelancer und Lancer-Nexus (https://github.com/orgs/Lancer-Nexus/repositories) sind reine Referenzen.

**Fokus: MMORPG.** Persistente gemeinsame Welt, viele Spieler, Accounts/Charaktere, geteilte Wirtschaft, Chat, Gruppen, Spielerfraktionen.
Alles ist von Anfang an **server-autoritativ** (Dedicated Server pro Sonnensystem); es gibt keinen reinen Singleplayer-Pfad.
**Entscheidungen (18.10.2026):** öffentlicher Server · 100–150 Spieler pro Instanz, Event-Server 500–1000 · MMO-typisch mit Spielgefühl nah am Original ·
Backend aus getrennten UE-Headless-Diensten (Gateway, Coordinator, Agent, Events, Admin) nach Vorbild Lancer-Nexus, aber für UE5 angepasst ·
Spiel-Instanzen und Instanzmanager bleiben **dynamisch**, das Gateway ist der öffentliche Einstieg. Details in `PLAN.md`.

## Stand

Clean Project: leeres Modul-Gerüst (`Source/Freelancer/`), `Content/` ohne Assets, kein Git, keine Tests.
Rendering-Defaults: Lumen, Virtual Shadow Maps, Hardware-Raytracing, Substrate. Standardkarte = Engine-Vorlage OpenWorld
(wird durch eigene Weltraumkarte ersetzt, siehe M0).

## Struktur

| Pfad | Inhalt |
|---|---|
| `Source/Freelancer/` | Spielmodul (C++). Abhängigkeiten: Core, CoreUObject, Engine, InputCore, EnhancedInput |
| `Source/*.Target.cs` | Targets `Freelancer` und `FreelancerEditor` |
| `Config/` | `DefaultEngine.ini`, `DefaultGame.ini`, `DefaultInput.ini`, `DefaultEditor.ini` |
| `Content/` | UE-Assets (aktuell leer) |
| `/media/Games/Freelancer/Reference/Librelancer/` | Klon von https://codeberg.org/Librelancer/Librelancer (C#, MIT). **Nur Referenz, nicht ändern, nicht bauen.** |
| `/media/Games/Freelancer/Reference/Lancer-Nexus/` | Klone aller Repos der Organisation (`Gateway`, `Coordinator`, `Agent`, `Protocol`, `Cluster`, `Events`, `Administration`, `Client`, `Data`, `Scripts`, `Download`, `Updater`; C#, MMO-Architektur, Lizenz ungeklärt). **Nur Referenz zum Lesen; keinen Code übernehmen, solange die Lizenz offen ist.** |
| `PLAN.md` | Projektplan |
| `Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/` | Generiert, nie bearbeiten |

## Speicherorte (seit 18.10.2026)

Alles liegt auf dem NFS-Share `/media/Games` (fstab: `10.10.10.2:/volume1/games`, `noauto` – vor der Arbeit `mount /media/Games`):

| Pfad | Inhalt |
|---|---|
| `/media/Games/Freelancer/Project/Freelancer/` | **Dieses UE-Projekt** (Git-Repo `main`) |
| `/media/Games/Freelancer/GameData/FL/` | Kopie der installierten Original-Daten (inkl. `reverse-engineering/`); Original bleibt in `~/Dokumente/FL` (Wine-Start) |
| `/media/Games/Freelancer/GameData/Freelancer.iso` | Disc-Image |
| `/media/Games/Freelancer/GameData/Work/` | Arbeitsdaten: `Freelancer-Daten`, `-Textures`, `-HD-Staging`, `-Audio-Stage`, `-Backup-*`, `-Video-Test` (Kopien; Originale noch in `~/Dokumente/`) |
| `/media/Games/Freelancer/Tools/` | `Freelancer-Plugins-Src` |
| `/media/Games/Freelancer/Reference/` | `Librelancer/`, `Lancer-Nexus/<Repo>/` (Referenzklone) |
| `/media/Games/Freelancer/Engine/` | vorgesehen für UE-5.8-Quellcode (noch leer, siehe PLAN.md M0) |

Hinweis: Builds über NFS sind langsam. Wenn nötig, `Intermediate/`, `Binaries/` und `DerivedDataCache/` lokal halten (z. B. per Symlink auf die lokale Platte).
Nach dem Umzug `Makefile`/Projektdateien neu generieren (enthalten alte Pfade).

## Notizen & Gedächtnis (Graphiti-Memory-Server)

- Server: `http://10.8.0.28` (nginx, Port 80). MCP-Endpoint: **`http://10.8.0.28/mcp`** (Streamable HTTP, Dienst „Graphiti Agent Memory"), Health: `/health`.
- **Nur über VPN erreichbar.** Ohne aktive VPN-Verbindung laufen Aufrufe in einen Timeout (Test: `curl -m 5 http://10.8.0.28/health`). Dann ohne Gedächtnis weiterarbeiten, nicht wiederholt versuchen, und den Eintrag nachholen, sobald das VPN steht.
- In diesem Projekt als MCP-Server `graphiti` in `.mcp.json` eingetragen (nach Neustart der Claude-Code-Sitzung verfügbar; bei Rückfrage zur Freigabe zustimmen).
- Projektbezogene Einträge immer mit **`group_id: "freelancer"`** schreiben und lesen.
- Tools: `add_memory` (Episoden/Notizen), `search_nodes`, `search_memory_facts`, `get_episodes`, `add_triplet`, `build_communities`, `delete_episode`, `delete_entity_edge`, `clear_graph` (**nie** ohne ausdrückliche Anweisung).
- Zu Sitzungsbeginn: `search_memory_facts`/`get_episodes` für `freelancer` lesen. Nach Entscheidungen oder Meilensteinen: kurze Episode mit `add_memory` ablegen. Keine Zugangsdaten, Tokens oder Originaldaten-Inhalte speichern.
- Erster Eintrag („Grundentscheidungen 2026-10-18") wurde am 18.10.2026 angelegt.

## Externe Ressourcen (außerhalb des Projekts)

- **Originaldaten:** `~/Dokumente/FL` – installierte Freelancer-Kopie (ENU). **Read-only behandeln.** Dort gibt es eine eigene
  `CLAUDE.md` mit Details (Text-INIs statt BINI, Wine-Start, Backups). Vor jeder Änderung dort ein Backup anlegen.
  - `DATA/UNIVERSE`, `SOLAR`, `SHIPS`, `EQUIPMENT`, `MISSIONS`, `RANDOMMISSIONS`, `SCRIPTS` (`.thn`), `AUDIO`, `FX`, `INTERFACE`.
  - `EXE/freelancer.ini` listet alle Daten-INIs in `[Data]`. Pfade nutzen `\`, Dateien CRLF, Kommentare mit `;`.
- **Reverse-Engineering:** `~/Dokumente/FL/reverse-engineering/librelancer/` (Ghidra-Notizen und Exports).
- **Werkzeuge:** `~/Dokumente/FL-tools/` (`bini2ini.py` …).
- **Original starten zum Vergleich:** `~/Dokumente/FL/EXE/start_freelancer.sh` (Wine).

## Quellen

**Lokal:**
- `~/Dokumente/FL` – Originaldaten (siehe oben)
- `/media/Games/Freelancer/Reference/Librelancer/` – Klon von Librelancer (C#, MIT); `src/LibreLancer.Data`, `.Physics`, `.Entities`, `.Thorn`, `docs/`
- `~/Dokumente/FL/reverse-engineering/librelancer/` – Ghidra-Notizen und Exports
- `~/Downloads/Eigene Bibliothek _ Fab.com.html` – gespeicherte Fab-Bibliothek (426 Einträge)

**Referenzprojekte (online):**
- Librelancer: https://codeberg.org/Librelancer/Librelancer (MIT)
- Lancer-Nexus (MMO-Variante, C#, Lizenz ungeklärt, nicht geklont): https://github.com/orgs/Lancer-Nexus/repositories
  (`Client`, `Data`, `Gateway`, `Coordinator`, `Protocol`, `Events`, `Cluster`, `Agent`, `Scripts`, `Download`, `Updater`, `Administration`)

**Über das Original:**
- Wikipedia: https://en.wikipedia.org/wiki/Freelancer_(video_game)
- Filfre (Retrospektive Chris Roberts/Digital Anvil): https://www.filfre.net/2025/12/mr-roberts-goes-to-hollywood-part-2-the-producer
- Unseen64 (Digital Anvil, Freelancer 2): https://www.unseen64.net/tag/digital-anvil/

**Mod-Community:**
- Discovery Freelancer: https://www.moddb.com/mods/discovery-freelancer/stats
- Discovery Freelancer v4.83: https://www.moddb.com/downloads/discovery-freelancer-v483
- DualShockers Community Spotlight: https://www.dualshockers.com/community-spotlight-discovery-freelancer/

**Unreal Engine:**
- Large World Coordinates: https://dev.epicgames.com/documentation/en-us/unreal-engine/large-world-coordinates-in-unreal-engine-5
- World Partition: https://80.lv/articles/a-first-look-at-the-world-partition-system-in-ue5

**Fab-Pakete (Auswahl aus der Bibliothek, UE-5.8-Kompatibilität jeweils prüfen):**
- M3/M4: Procedural Asteroid Field Tool, [FREE] Planet Project for Unreal Engine 5, Planet 385CY, Space Nebula Skyboxes – Project Sample
- M2/M3: Ultimate Spaceship System: Space Shooter Template (Referenz), Lyra Starter Game (Architekturreferenz)
- M3: Realistic Starter VFX Pack Vol 2, FX Variety Pack, Particle Effects, Blink and Dash VFX
- M4/M7: Async Loading Screen, Niagara UI Renderer
- M5 (Platzhalter): Modular SciFi Season 1/2 Starter Bundle, High Tech Base, Underground Sci-Fi, Scifi Hallway, High-Tech Pack 1, Spaceship Interior Environment Set, Elementus Inventory
- M8 (nur Nachschlagen): MMO Kit Code, UETOPIA Online Subsystem

## Werkzeuge

Hilfswerkzeuge (`fltool`, `tex_to_png.py`, Testlevel-Skript) sind in **[Tools.md](Tools.md)** dokumentiert. Neue Werkzeuge dort eintragen.

## Build & Start (Linux)

- Projektdateien/Makefile erzeugen: `GenerateProjectFiles.sh` der UE-Installation (`-project="<Pfad>/Freelancer.uproject" -game -engine`), bzw. Rechtsklick auf `.uproject`.
- Editor-Target bauen: `make FreelancerEditor` (Makefile liegt im Projekt) oder über VS Code (`Freelancer.code-workspace`).
- Nach Änderungen an `.h`/`.cpp`: neu bauen; bei neuen Dateien Projektdateien neu generieren.
- Engine: `~/Linux_Unreal_Engine_5.8.3` (Installed Build). Build direkt: `~/Linux_Unreal_Engine_5.8.3/Engine/Build/BatchFiles/Linux/Build.sh FreelancerEditor Linux Development -Project="<Pfad>/Freelancer.uproject" -WaitMutex`.
- Stand 18.10.2026: `FreelancerEditor` baut. **`FreelancerServer`/`FreelancerClient` lassen sich mit dieser Engine nicht bauen** („Server targets are not currently supported from this engine distribution"); dafür ist eine aus dem Quellcode gebaute UE 5.8 nötig (siehe `PLAN.md`, M0). Bis dahin: Spiel-Instanz mit `UnrealEditor Freelancer.uproject <Map> -server -game -log` testen.
- Git: Repo `main` angelegt, `git-lfs` noch zu installieren (`sudo apt install git-lfs && git lfs install`).

## Konventionen

- C++-Klassen mit Präfix **`FL`** (z. B. `AFLShip`, `UFLShipData`); Ordner nach Thema: `Flight/`, `Combat/`, `World/`, `Economy/`, `Missions/`, `UI/`, `Data/`.
- **C++ und Blueprints werden gemeinsam verwendet** (Details: `PLAN.md`, Abschnitt 3, Punkt 11):
  - **C++:** Flug-/Bewegungs-Netcode, Server-Logik (Schaden, Handel, Missionen, Ruf, Validierung), Replikation, Datenmodell, Importer, Backend-Dienste, alles Performance-Kritische.
  - **Blueprints (auf C++-Basisklassen):** UI/HUD (UMG/CommonUI), Niagara-Effekte, Materialien, Audio-Zuordnung, Schiffs-/Waffen-/Stations-Konfiguration, Level-Gestaltung, Prototyping.
  - Logik wird in C++ geschrieben und per `UPROPERTY`/`UFUNCTION(BlueprintCallable/BlueprintImplementableEvent)` freigegeben. Blueprints leiten von `AFL*`/`UFL*` ab und enthalten keine autoritative Spiellogik.
  - Blueprint-Namen: `BP_`-Präfix, Widgets `WBP_`, Daten `DA_`/`DT_`. Ablage unter `Content/Freelancer/<Thema>/`.
  - Blueprint-Assets sind binär (schlecht mergebar): pro Asset nur eine Person gleichzeitig, kleine Blueprints bevorzugen, Git LFS für `.uasset`/`.umap`.
- Unreal-Coding-Standard (Tabs, `UPROPERTY`/`UFUNCTION`-Makros, Header mit Copyright-Zeile wie im Template).
- Spiellogik **daten-getrieben**: Werte kommen aus importierten Data Assets/Tables, nicht hardcoded.
- Original-INIs werden **nie** zur Laufzeit gelesen. Der Importer ist ein C++-Editor-Modul `FreelancerEditor` mit Commandlet `FLImport` und erzeugt UE-Assets.
- **Server-autoritativ:** Gameplay-Logik (Schaden, Handel, Missionen, Ruf) läuft nur auf dem Server; Clients senden Eingaben per RPC. Alles Neue wird mit Replikation gebaut und mit mindestens zwei Clients gegen einen Dedicated Server getestet.
- Server validiert jede Client-Eingabe (Geschwindigkeit, Feuerrate, Reichweite, Handelsmengen).
- Persistente Daten (Account, Charakter, Inventar, Geld, Ruf, Missionen) gehören ins Backend (M5), nie nur in den Client oder in `SaveGame`-Dateien.
- Targets: `FreelancerClient`, `FreelancerServer` (Spiel-Instanz), `FreelancerEditor`, später Programm-Targets `FreelancerGateway`, `FreelancerCoordinator`, `FreelancerAgent`, `FreelancerEvents`, `FreelancerAdmin` (anzulegen ab M0/M4).
- Dienste sprechen über das geteilte Modul `FreelancerProtocol` (versionierte Nachrichten). Spiel-Instanzen greifen nie direkt auf die Datenbank zu.
- Öffentlicher Betrieb: Sicherheit (TLS/mTLS, Rate-Limits, Audit) von Anfang an mitdenken. **Keine Originaldaten in Builds, die verteilt werden**, solange die Rechtsfrage offen ist.
- **Asset-Pipeline (PLAN.md Punkt 14, M1b):** Original importieren -> Texturen per Offline-Upscaling aufwerten (Normal/Roughness/Metallic/Emissive) -> eigene HD-Modelle (Nanite) nach Vorlage. Datensätze verweisen auf Mesh/Material, ein HD-Asset gleicher ID ersetzt das Original. Der Server lädt keine Meshes. Rechtlich: Ableitungen sind Microsoft-Werke, nicht verteilen.
- **Tests sind Pflicht** (Werkzeuge: siehe [Tools.md](Tools.md)): Zu jeder neuen Funktion, jedem Werkzeug und jedem Bugfix gehören Tests im selben Commit. Tests nutzen nur synthetische Daten (keine Originaldateien im Repo) und decken Fehlerfälle ab (kaputte/abgeschnittene Eingaben). Ein Commit ohne grüne Tests wird nicht gepusht.
  - `Tools/fltool`: CTest, `cmake -S Tools/fltool -B ~/UEBuild/fltool && cmake --build ~/UEBuild/fltool && ctest --test-dir ~/UEBuild/fltool` (Tests in `tests/`).
  - Python-Werkzeuge: `unittest` unter `Tools/tests/`, Aufruf `python3 -I Tools/tests/test_<name>.py`.
  - UE-Code: Automation Tests (`IMPLEMENT_SIMPLE_AUTOMATION_TEST`) im Modul, Aufruf mit `UnrealEditor-Cmd Freelancer.uproject -ExecCmds="Automation RunTests Freelancer; Quit" -unattended -nullrhi`. Netcode/Replikation zusätzlich mit Functional Tests oder Mehr-Client-Läufen.
  - Zusätzlich gegen die Originaldaten prüfen (lokal, nicht im Repo), z. B. Parser über alle `GameData/FL`-Dateien laufen lassen.
- Eingabe über **Enhanced Input** (Input Actions + Mapping Contexts), nicht über Legacy-Axis-Mappings.
- Große Distanzen: Large World Coordinates, `double`/`FVector` (UE5 ist standardmäßig double).

## Arbeitsregeln

- **Keine Originaldaten committen** (Modelle, Texturen, Audio, INIs, Filme). `Content/` darf nur selbst erzeugte oder lizenzfreie Assets enthalten. Importierte Original-Assets in einen eigenen, git-ignorierten Ordner (`Content/Imported/`).
- Kein Löschen/Umbenennen in `GameData/FL`, `~/Dokumente/FL` oder `Reference/`.
- Librelancer-Code darf als MIT-lizenzierte Referenz dienen. Bei Übernahme von Code den Lizenzhinweis erhalten. Librelancer nimmt keine KI-generierten Issues/PRs an.
- Vor größeren Strukturänderungen im Projekt zuerst Git-Repo anlegen (M0) – aktuell gibt es keine Rückgängig-Möglichkeit.
- Meilensteine in `PLAN.md` strikt der Reihe nach, jeder soll spielbar enden. Nach Abschluss Status dort aktualisieren.
- Kommentare im Code auf Englisch, Doku (`*.md`) auf Deutsch.
