# PLAN.md – Freelancer-Nachbau (MMORPG) in Unreal Engine 5.8

Stand: 18.10.2026 · Status: Planung, Projekt ist ein clean C++-Projekt (leeres Modul `Freelancer`).

## 1. Ziel

Freelancer (Digital Anvil/Microsoft, 2003) in UE5 als **MMORPG** neu aufbauen: eine persistente, gemeinsame Welt aus den
Freelancer-Sonnensystemen, in der viele Spieler gleichzeitig fliegen, kämpfen, handeln und Missionen erfüllen.
Die Originaldaten (INIs, Modelle, Texturen) werden lokal **konvertiert und importiert**, nicht im Projekt versioniert.

**Grundsatz: vollständig in Unreal Engine 5.** Client, Server, Datenimport und Werkzeuge entstehen in UE5 (C++/Blueprints,
Editor-Module, Commandlets, Dedicated Server). Es gibt keine C#-/.NET-Laufzeitkomponenten im Produkt. Librelancer
und Lancer-Nexus (C#, inkl. MMO-Server-Architektur) dienen nur als **Referenz**, ihr Code wird nicht eingebunden.

**MMO-Fokus bedeutet:**
- **Server-autoritativ von Tag 1.** Es gibt keinen „Singleplayer-Modus mit Netzwerk später". Auch lokal läuft Client + Dedicated Server.
- **Persistente Charaktere und Welt:** Accounts, Charaktere, Schiffe, Inventar, Geld, Ruf, Missionsfortschritt liegen serverseitig.
- **Gemeinsame Wirtschaft:** Märkte und Preise gelten für alle Spieler.
- **Soziale Systeme:** Chat, Gruppen, Spielerfraktionen (Gilden).
- **Skalierung:** ein Server pro Sonnensystem (oder pro Zone), Wechsel über Jump Gates/Holes.
- **Öffentlicher Server.** Das Projekt wird öffentlich betrieben. Das hat Folgen für Rechte (Abschnitt 5), Sicherheit und Betrieb.
- **Zielgröße:** normale Instanzen ca. **100–150 Spieler**; **Event-Server** für Schlachten mit **500–1000 Spielern**.
- **Spielgefühl:** MMO-typisch (Persistenz, Wirtschaft, Gilden, Events), aber **nah am Original** bei Flug, Kampf, Welt und Story.
- **Der Spielinhalt (Story, Missionen, Schiffe, Systeme) kommt aus dem Original.** Neu ist die MMO-Schicht darüber.

Nicht-Ziele (vorerst): Mod-Kompatibilität mit dem Original oder Discovery, Basen-Innenräume mit Charakteranimation,
exakte Wiedergabe der `content.dll`-Logik, Kompatibilität zu Original-Clients/-Servern (`flserver.exe`).

## 2. Quellen & Referenzen

| Zweck | Ort |
|---|---|
| Originaldaten (read-only, Backup vor Änderungen) | `~/Dokumente/FL` (`DATA/`, `EXE/`, `DLLS/BIN/content.dll`) |
| C#-Referenzimplementierung (MIT) | `Reference/Librelancer` (`src/LibreLancer.Data`, `.Physics`, `.Entities`, `.Thorn`, `LLServer`) |
| Weitere Referenz (MMO-Architektur, Lizenz ungeklärt) | `Reference/Lancer-Nexus/` (`Gateway`, `Coordinator`, `Agent`, `Protocol`, `Cluster`, `Events`); online: https://github.com/orgs/Lancer-Nexus/repositories. `Client`, `Data`, `Administration` u. a. nicht geklont. |
| Reverse-Engineering-Notizen (Ghidra) | `~/Dokumente/FL/reverse-engineering/librelancer/{notes,exports}` |
| Hilfswerkzeuge | `~/Dokumente/FL-tools/` (`bini2ini.py` u. a.) |
| Original starten (Wine) | `~/Dokumente/FL/EXE/start_freelancer.sh` – zum Vergleichen des Spielgefühls |
| UE-Doku | Large World Coordinates, Replication, Dedicated Server, Enhanced Input, Niagara (Epic Developer Docs) |

Datenmengen im Original: 58 Systemordner, 511 `.cmp`, 1341 `.3db`, 107 `.mat`, 104 `.txm`, 109 `.utf`.
Vollständige Quellenliste siehe `CLAUDE.md`.

## 3. Architekturentscheidungen

1. **Daten-getrieben:** Der Importer ist ein **UE-Editor-Modul** (`FreelancerEditor`, C++) mit Commandlet
   (`-run=FLImport`, auch headless nutzbar). Es liest die INIs direkt und erzeugt UE-Assets (DataAssets/DataTables).
   Spielcode kennt keine INI-Formate. Client und Server nutzen dieselben importierten Daten.
2. **Modelle:** `.cmp`/`.3db`/`.mat`/`.txm` (UTF-Container) werden im Editor-Modul in C++ gelesen
   (Referenz: `LibreLancer.Data` und `docs/model-importer.md`) und direkt als StaticMesh/Material/Texture-Assets
   erzeugt. Kein Umweg über externe Tools.
3. **Netzwerkmodell:** Server-autoritativ. Clients senden Eingaben (RPC), der Server simuliert und repliziert.
   Flug mit Client-Prediction und Server-Korrektur (eigene Bewegungskomponente, siehe 4). Gameplay-Logik (Schaden, Handel,
   Missionen, Ruf) läuft **nur auf dem Server**.
4. **Flugmodell:** Eigener `AFLShip` (Pawn) mit eigener `UFLShipMovementComponent` (Newton-Schub, Dämpfung, Cruise,
   Mausflug mit Cursor-Offset), deterministisch genug für Prediction/Reconciliation; nicht Chaos-Rigidbody.
   Werte aus `engine_equip.ini`/`shiparch.ini`. Vorbild für den Aufbau: Character-/Mover-Prediction in UE.
5. **Welt & Serverstruktur:** Jedes Freelancer-System = ein Level, gehostet von einer **Spiel-Instanz** (UE-Dedicated-Server-Prozess).
   Große Distanzen mit **Large World Coordinates**. Jump Gates/Holes übergeben den Spieler über das Gateway per Client-Travel
   an die Instanz des Zielsystems. **Instanzen sind dynamisch:** sie werden bei Bedarf gestartet, gedrosselt (drain) und beendet;
   Spiel-Instanz und Instanzmanager sind lose gekoppelt, die Spiel-Instanz kennt nur das Gateway/den Koordinator.
6. **Backend-Dienste (alle UE-Programm-Targets, headless, C++):** Aufbau angelehnt an Lancer-Nexus, für UE5 angepasst:

   | Dienst | Aufgabe | Lancer-Nexus-Vorbild |
   |---|---|---|
   | **Gateway** | Öffentlicher Einstieg: Login, Sessions, Charakterliste, Zuweisung zu Instanzen, kontrollierte Transfers, öffentliche API (z. B. Marktdaten) | `Gateway` |
   | **Coordinator** | Mitgliedschaft, Kapazität, Platzierung (Gruppen zusammenhalten), Event-Kapazität reservieren, Märkte/Wirtschaft, Transfers koordinieren | `Coordinator` |
   | **Agent** | Läuft pro Host, startet/stoppt/drained Spiel-Instanzen, meldet Heartbeats und Spielerzahlen | `Agent` |
   | **Events** | Schlachten/Events: Anmeldung, reservierte Event-Server, Start/Ende/Ergebnis, Rücktransfer | `Events` |
   | **Administration** | Rechte, Moderation, Audit, Admin-Befehle | `Administration` |
   | **Protocol** | Geteilte Nachrichtenverträge (Versionierung, Message-IDs) | `Protocol` |
   | **Spiel-Instanz** | UE-Dedicated-Server (`FreelancerServer`), simuliert ein System, meldet Status an den Agent | `LLServer` |

   Grundsätze: Spiel-Instanzen sind autoritativ für die Simulation, Coordinator/Gateway besitzen sie nicht. Transfers sind
   lease-basiert (Versionen/Sessions), idempotent und crash-sicher (Journal/Outbox). Ein Spieler gilt erst als übertragen, wenn der Zielserver bestätigt.
7. **Persistenz:** Zentrale Datenbank (Vorschlag: PostgreSQL oder MySQL über ein eigenes ThirdParty-Modul), nur über das Gateway
   zugänglich. Kein direkter DB-Zugriff aus Spiel-Instanzen. Event-Benachrichtigung zwischen Diensten (Vorschlag: Redis oder
   eigene Nachrichten über das Protocol). Entscheidung der DB: offene Frage 2.
8. **Rendering:** Für Weltraum prüfen, ob Hardware-Raytracing/Lumen sinnvoll sind (aktuell in
   `DefaultEngine.ini` an). Ggf. abschalten für Performance. Der Dedicated Server rendert nichts.
9. **Eingabe:** Enhanced Input (Modul bindet es schon ein), nur auf dem Client.
10. **Code-Struktur (Module):**
   - `Freelancer` (Runtime, Client und Spiel-Instanz): `Flight/`, `Combat/`, `World/`, `Economy/`, `Missions/`, `Social/`, `Net/`, `Data/`.
   - `FreelancerProtocol` (geteilt von Spiel und Diensten): Nachrichtenverträge, Serialisierung, Versionierung.
   - `FreelancerServices` (Dienste-Basis: HTTP/Sockets, DB, Konfiguration, Logging).
   - `FreelancerEditor` (Editor-only): Importer, Commandlet, Asset-Factories.
   - Targets: `FreelancerClient`, `FreelancerServer` (Spiel-Instanz), `FreelancerEditor`, sowie Programm-Targets
     `FreelancerGateway`, `FreelancerCoordinator`, `FreelancerAgent`, `FreelancerEvents`, `FreelancerAdmin`.
11. **C++ und Blueprints:** Beides wird verwendet, mit klarer Aufteilung.

    | Bereich | Technik | Grund |
    |---|---|---|
    | Flugmodell, Bewegungs-Netcode (Prediction/Reconciliation) | C++ | Performance, Determinismus, Netzwerk |
    | Server-Logik: Schaden, Handel, Missionen, Ruf, Validierung, Transfers | C++ | Sicherheit, Wartbarkeit, Server-Autorität |
    | Replikation, Relevanzfilter, Event-Server-Optimierung | C++ | Skalierung (500–1000 Spieler) |
    | Datenmodell (DataAssets, Strukturen), Importer, Commandlet | C++ | Editor-Modul, große Datenmengen |
    | Backend-Dienste (Gateway, Coordinator, Agent, Events, Admin) | C++ | Programm-Targets ohne Blueprints |
    | UI, HUD, Menüs, Basen-Screens (UMG/CommonUI) | Blueprint (`WBP_`) auf C++-Widget-Basis | Layout/Iteration, Designer-freundlich |
    | Niagara, Materialien, Audio-Zuordnung | Blueprint/Asset | Visuelle Bearbeitung |
    | Schiffe, Waffen, Stationen als Konfiguration | Blueprint-Kinder (`BP_`) von `AFL*` + importierte DataAssets | Werte ändern ohne Neubau |
    | Kamera-/Gefühl-Prototypen, Testlevel | Blueprint, später ggf. nach C++ portieren | Schnelles Ausprobieren |

    Regeln: Autoritative Logik gehört nach C++ und läuft nur auf dem Server. Blueprints leiten von C++-Basisklassen ab und rufen
    nur freigegebene Funktionen auf. Blueprints enthalten keine Sicherheits- oder Wirtschaftslogik. Performance-kritische
    Blueprint-Logik wird nach C++ portiert (Nativisierung). Blueprint-Assets sind binär: Git LFS, pro Asset eine Person gleichzeitig,
    kleine Blueprints bevorzugen. Namenspräfixe: `BP_`, `WBP_`, `DA_`, `DT_`, `NS_` (Niagara).
12. **Sicherheit/Anti-Cheat-Grundlage:** Server validiert alle Eingaben (Geschwindigkeit, Feuerrate, Reichweite, Handelsmengen).
    Der Client erhält nur, was er sehen darf. Für den öffentlichen Betrieb zusätzlich: TLS/mTLS zwischen Diensten, Rate-Limits am Gateway,
    Audit-Log für Admin-Aktionen, verschlüsselte Charakter-Snapshots, DDoS-Schutz vor dem Gateway.
13. **Event-Server (500–1000 Spieler):** Eigener Instanztyp mit reservierter Kapazität, stärker optimiertem Netzwerk
    (aggressive Relevanzfilter, reduzierte Replikationsrate für Entferntes, vereinfachte KI/Effekte, ggf. Replication Graph / Iris).
    Anmeldung, Start und Rücktransfer verwaltet der Events-Dienst.

## 4. Meilensteine

Jeder Meilenstein endet **mit mehreren verbundenen Clients**, nicht mit einem lokalen Test.

### M0 – Fundament (Woche 1)
- `CLAUDE.md` + `PLAN.md` im Projekt, `.gitignore`, Git-Repo (`Reference/`, `Intermediate/`, `Saved/`, `Binaries/` ignorieren,
  **keine Originaldaten committen**).
- Build prüfen (`FreelancerEditor` lädt unter Linux), Server-Target `FreelancerServer` anlegen und starten.
- Standardkarte auf eigene leere Weltraumkarte `L_Test_Space` umstellen.
- **Fertig, wenn:** Dedicated Server startet, zwei Editor-/Client-Instanzen verbinden sich mit dem Testlevel.

**Status M0 (18.10.2026):**
- [x] Git-Repo (`main`) mit `.gitignore` (ohne `Reference/`, `Content/Imported/`, generierte Ordner) und `.gitattributes` (LFS-Regeln); noch kein Commit.
- [x] `FreelancerEditor` baut unter Linux (`Build.sh FreelancerEditor Linux Development`, Ergebnis Succeeded).
- [x] Targets `FreelancerServer` und `FreelancerClient` angelegt (`Source/*.Target.cs`).
- [ ] **Blocker:** Die installierte Engine `~/Linux_Unreal_Engine_5.8.3` ist ein Installed Build und meldet beim Server-Build
      „Server targets are not currently supported from this engine distribution". Server-/Client-Targets und die headless
      Dienst-Programme (Gateway, Coordinator, Agent …) brauchen eine **aus dem Quellcode gebaute UE 5.8** (Epic-GitHub, Konto verknüpfen).
- [x] Übergangslösung: Spiel-Instanz mit dem Editor-Binary als Server getestet (`UnrealEditor Freelancer.uproject /Game/Freelancer/Maps/L_Test_Space -server -game -log -nullrhi`): startet, lauscht auf UDP 7777, lädt das Level.
- [ ] `git-lfs` installieren (`sudo apt install git-lfs`, danach `git lfs install`), ersten Commit anlegen.
- [x] Leeres Level `/Game/Freelancer/Maps/L_Test_Space` angelegt (`Tools/create_test_level.py`) und als Standard-, Editor- und Servermap gesetzt.
- [ ] Zwei Clients verbinden sich mit dem Server (noch nicht getestet).
- [x] Hinweis: `/media/Games` ist `noexec` gemountet, `.so`-Dateien lassen sich von dort nicht laden. `Binaries/`, `Intermediate/`, `DerivedDataCache/` und `Saved/` liegen deshalb lokal in `~/UEBuild/Freelancer/` (Symlinks im Projekt). Mit `exec` im fstab wäre das nicht mehr nötig.

### M1 – Datenimport-Pipeline (Wochen 2–4)
- INI-Parser (Sektionen mit wiederholten Keys, `;`-Kommentare, CRLF, `\`-Pfade). Referenz: `LibreLancer.Data`.
- Modul `FreelancerEditor` mit Commandlet `FLImport` (alles C++).
- Import direkt in `Content/Imported/`: Schiffe (`shiparch.ini`), Ausrüstung (`*_equip.ini`), Waren (`goods.ini`),
  Märkte, Systeme (`universe.ini`, `SYSTEMS/*`), Solar-Archetypen.
- Mesh-/Textur-Konverter für 1–2 Testschiffe (z. B. Liberty-Starter-Schiff) und Teile eines Systems.
- **Fertig, wenn:** Ein Original-Schiff mit korrekten Texturen steht im UE-Level, auf Server und Client identisch geladen.

### M2 – Netzwerk-Flug (Wochen 4–8)
- `AFLShip` + `UFLShipMovementComponent` mit Enhanced Input: Maussteuerung (Cursor-Offset), Schub, Strafe, Reverse, Cruise Engine.
- Replikation, Client-Prediction und Server-Reconciliation; Interpolation für fremde Schiffe.
- Kamera (Chase, Cockpit optional), HUD-Grundlagen.
- Werte aus Original (`constants.ini`, `engine_equip.ini`, `shiparch.ini`), Vergleich mit der laufenden Wine-Version.
- **Fertig, wenn:** Mehrere Spieler fliegen im selben Level, Bewegung wirkt bei 100 ms Latenz flüssig, Werte ähneln dem Original.

### M3 – Kampf (Wochen 8–12)
- Hardpoints, Waffen (Projektile, Strahlen, Raketen), Schilde, Rumpf, Schaden, Zielerfassung, Auto-Aim.
- Server-autoritative Treffer-Auflösung (mit Lag-Kompensation für schnelle Projektile).
- Effekte (Niagara), Sound-Platzhalter; einfache KI-Gegner (`DATA/AI`, `npcships.ini`) als Server-Actors.
- **Fertig, wenn:** Spieler kämpfen gegeneinander und gegen KI mit Original-Waffenwerten; Cheats wie Speedhack werden abgelehnt.

### M4 – Welt & Instanzen (Wochen 12–19)
- System-Loader aus importierten Daten: Sonne, Planeten, Stationen, Asteroidenfelder, Nebel, Zonen.
- Docking an Stationen (Übergang in Basen-Menü), Karte/Navigation.
- **Protocol** (`FreelancerProtocol`), erste Fassung von **Agent** und **Coordinator**: Instanzen registrieren sich, melden Heartbeats
  und Spielerzahl; der Agent startet/stoppt Spiel-Instanzen dynamisch.
- Jump Gates/Holes mit **Instanzwechsel** (Client-Travel) über das Gateway, Ladebildschirm.
- **Fertig, wenn:** Mindestens Liberty-Systeme begehbar und verbunden; Spieler reist zwischen zwei dynamisch gestarteten Instanzen.

### M5 – Gateway, Accounts & Persistenz (Wochen 19–26)
- **Gateway:** Login/Registrierung, Sessions, Charakter-Erstellung und -Auswahl, Instanz-Zuweisung, kontrollierte Transfers (Lease-Modell).
- Datenbank + Charakter-Snapshots (versioniert, verschlüsselt), Reconnect-Verhalten, Backups.
- Persistente Daten: Position/Basis, Schiff, Ausrüstung, Inventar, Geld, Ruf, Missionsstatus.
- **Fertig, wenn:** Ein Spieler loggt sich aus und wieder ein und findet seinen Stand unverändert vor; ein Instanzabsturz verliert nur wenige Sekunden Fortschritt.

### M6 – Wirtschaft & Basen (Wochen 22–28)
- Goods, Märkte (`market_*.ini`), Preise je Basis **serverweit geteilt** (Angebot/Nachfrage), Kaufen/Verkaufen, Frachtraum.
- Handel läuft transaktional über Gateway/Coordinator (Quote → Reservierung → Commit; idempotent, mit Ledger und Outbox).
- Ausrüstungs- und Schiffshändler, Ausrüstung montieren, Schiffskauf.
- Basen-Menü (UMG/CommonUI), Bar (zunächst Text/Bild).
- **Fertig, wenn:** Handelsroute fahrbar, Preise reagieren auf Spielerhandel, Inventar bleibt erhalten.

### M7 – Missionen & Fraktionen (Wochen 28–36)
- Fraktionsruf (`faction_prop.ini`, Empire-Tabellen), Bounty/Random Missions (`RANDOMMISSIONS/`).
- Story-Missionen M01A ff. als **instanziierte Missionen pro Spieler/Gruppe** auf dem Server (Mission-INI + Thorn/Skript-Logik).
  Entscheidung: eigene Mission-DSL oder State-Machine/Blueprint.
- **Fertig, wenn:** Erste Story-Missionen (M01) durchspielbar, auch in der Gruppe; Missionsfortschritt ist persistent.

### M8 – Soziales & Spielerfraktionen (Wochen 36–42)
- Chat (System/Lokal/Gruppe/Privat), Freundesliste, Gruppen.
- Spielergilden/-fraktionen, optional spielereigene Basen (inspiriert von Discovery Freelancer, noch zu entscheiden).
- Verwaltungswerkzeuge (Admin/GM-Befehle), Moderation, Logging.
- **Fertig, wenn:** Gruppen bilden sich, Gilden existieren persistent, Chat funktioniert serverübergreifend.

### M9 – Events & große Schlachten (Wochen 42–50)
- **Events-Dienst:** Event-Definitionen und Zeitpläne, Anmeldung, reservierte Event-Server, Start/Sperre/Ergebnis, Rücktransfer.
- Event-Instanz für 500–1000 Spieler: Relevanzfilter, reduzierte Replikation für Entferntes, Lasttest mit simulierten Clients.
- Gruppen- und Gildenzusammenhalt bei der Platzierung (Coordinator).
- **Fertig, wenn:** Ein Testevent mit mindestens 500 simulierten Clients läuft stabil (Ziel: 1000).

### M10 – Skalierung, Betrieb & Politur (laufend)
- Lasttests normaler Instanzen (100–150 Spieler), Interest-Management/Relevanz, Netzwerk-Optimierung.
- Server-Orchestrierung (Start/Stop pro System), Monitoring, Deployment, Updates.
- Audio, Musik, HUD-Optik, Cockpits, Einstellungsmenü, Packaging für Linux/Windows.

## 5. Risiken

| Risiko | Gegenmaßnahme |
|---|---|
| Proprietäre Modellformate (UTF) | Librelancer-Importer als Referenz, früh in M1 prüfen |
| Lizenz/Urheberrecht (Microsoft-Assets) | **Hoch, da öffentlicher Server.** Freelancer-Name, Modelle, Texturen, Musik, Texte und Story gehören Microsoft. Bis zur Klärung gilt: Originaldaten nie committen oder verteilen; Entwicklung nur lokal. Für den öffentlichen Betrieb früh Strategie festlegen (eigene/ersetzte Assets und eigener Name, oder Client liest Daten aus der Freelancer-Installation des Spielers; rechtliche Beratung einholen). |
| Event-Server mit 500–1000 Spielern | Früh messen (M3 Prototyp, M9 Lasttest); Iris/Replication Graph prüfen; Fallback: weniger Spieler oder Sharding |
| Dynamische Instanzen / Transfers | Lease-/Versionsmodell wie bei Lancer-Nexus, Idempotenz, Crash-Recovery-Tests |
| `content.dll` (Missionslogik) nicht quelloffen | Verhalten aus Librelancer + Reverse-Engineering-Notizen ableiten, nicht 1:1 |
| Flugmodell mit Prediction | Früh in M2 prototypisch testen, ggf. Mover-Plugin/eigene Netcode-Komponente |
| UE bringt keine MMO-Persistenz mit | Backend-Dienst selbst bauen (M5); früh Schnittstelle festlegen |
| Skalierung (viele Spieler pro System) | Relevanzfilter, Lasttests ab M3, Zonen-Aufteilung |
| Cheating | Server-Autorität, Validierung aller Eingaben (M3) |
| Betrieb/Kosten | Erst lokaler Betrieb, Hosting erst nach M5 klären |
| Zeitaufwand/Umfang | Strikt in Meilensteine schneiden; jeder Meilenstein endet mit mehreren Clients |
| Linux-Toolchain | Build/Editor/Server früh in M0 verifizieren |

## 6. Offene Fragen

Entschieden (18.10.2026):
1. ~~Zielgruppe~~ – **öffentlicher Server.**
2. ~~Persistenz-Backend~~ – **separate Backend-Dienste** (UE-Headless-Programme, Gateway/Coordinator/Agent), Spiel-Instanzen greifen nicht direkt auf die DB zu.
3. ~~Zielgröße~~ – **100–150** pro Instanz, **500–1000** auf Event-Servern.
4. ~~Spielstil~~ – MMO-typisch, Spielgefühl **nah am Original**.
5. ~~Lancer-Nexus~~ – dient als Orientierung (geklont in `Reference/Lancer-Nexus/`), Struktur wird für UE5 angepasst; Instanzmanager und Spiel-Instanz bleiben dynamisch, das Gateway ist der Einstieg.

Noch offen:
1. **Rechte/Strategie für den öffentlichen Betrieb:** eigene Assets und Namen, oder Daten aus der Spieler-Installation? Rechtliche Klärung nötig.
2. **Datenbank:** PostgreSQL oder MySQL (Lancer-Nexus nutzt MySQL + Redis)? Anbindung als ThirdParty-Modul in UE.
3. **Hosting:** eigene Hardware oder Cloud (bestimmt Orchestrierung, Kosten, DDoS-Schutz)?
4. **Zielplattform:** Server nur Linux? Client Linux + Windows?
5. **MMO-Erweiterungen über das Original hinaus:** Spielerbasen, Crafting, Levelsystem – ja/nein, und ab wann?
6. Reihenfolge nach M2: Kampf zuerst oder Welt/Instanzen zuerst?
7. **Netzwerk-Technik:** Standard-UE-Replikation, Iris oder Replication Graph für die Event-Server? (Prototyp-Messung in M3.)
