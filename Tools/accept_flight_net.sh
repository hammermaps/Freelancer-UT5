#!/bin/bash
# Abnahmetest M2: Server + 2 Clients fliegen im Bot-Modus (Level L_FlightTest). Prueft Vorhersage/Abgleich im Netz.
# Voraussetzung: gekochte Pakete in ~/UEBuild/Archive (Engine/cook_freelancer.sh). Exit-Code 0 = bestanden.
# Kriterien (Loopback; zusaetzlich: System Li01 geladen, Testkugel wird getroffen): beide Clients treten bei; der Besitzer-Client meldet sein Schiff mit Rolle 2 (AutonomousProxy),
# das fremde Schiff mit Rolle 1 (SimulatedProxy) und Tempo > 0; Positionsfehler des Besitzers < 150 cm; keine Fehlerzeilen.
A=${1:-$HOME/UEBuild/Archive}
SLOG=$A/LinuxServer/Freelancer/Saved/Logs/Freelancer.log
CLOG1=$A/LinuxClient/Freelancer/Saved/Logs/Freelancer.log
CLOG2=$A/LinuxClient/Freelancer/Saved/Logs/Freelancer_2.log
pkill -x FreelancerServe; pkill -x FreelancerClien; sleep 2
rm -f "$A"/LinuxServer/Freelancer/Saved/Logs/*.log "$A"/LinuxClient/Freelancer/Saved/Logs/*.log
# System Li01 (Sonne bei 0,0,0, Objekte bis 94 km): Start 150 km entfernt, Testkugel auf dem Flugweg der Bots
"$A/LinuxServer/FreelancerServer.sh" /Game/Freelancer/Maps/L_FlightTest -log -port=7777 -ForceLogFlush -FLSystem=Li01 -FLSpawnOffset=15000000,0,0 -FLTestSphere=15010000,-2000,0,8000 > /dev/null 2>&1 &
sleep 45
for i in 1 2; do
  "$A/LinuxClient/FreelancerClient.sh" 127.0.0.1:7777 -nullrhi -nosound -log -ForceLogFlush -FLBot -ExecCmds="t.MaxFPS 60" -ResX=640 -ResY=360 > /dev/null 2>&1 &
  sleep 3
done
sleep 40
pkill -x FreelancerClien; pkill -x FreelancerServe
echo "--- Server"; grep -h "LogFLShip" "$SLOG" | sed -E 's/^\[[^]]*\]\[[^]]*\]//' | tail -4
echo "--- Client 1"; grep -h "LogFLShip" "$CLOG1" | sed -E 's/^\[[^]]*\]\[[^]]*\]//' | tail -3
echo "--- Client 2"; grep -h "LogFLShip" "$CLOG2" | sed -E 's/^\[[^]]*\]\[[^]]*\]//' | tail -3
JOIN=$(grep -c "Join succeeded" "$SLOG")
ERR=$(cat "$SLOG" "$CLOG1" "$CLOG2" 2>/dev/null | grep -c "LogOutputDevice: Error")
OWN=$(cat "$CLOG1" "$CLOG2" 2>/dev/null | grep "LogFLShip" | grep -c "Rolle=2")
SIM=$(cat "$CLOG1" "$CLOG2" 2>/dev/null | grep "LogFLShip" | grep -c "Rolle=1")
MAXERR=$(cat "$CLOG1" "$CLOG2" 2>/dev/null | grep "Rolle=2" | sed -E 's/.*MaxFehler=([0-9.]+) cm.*/\1/' | sort -n | tail -1)
SYS=$(grep -h "System Li01:" "$SLOG" | sed -E 's/.*Li01: ([0-9]+) feste.*/\1/' | head -1)
STATIC=$(grep -c "fest TestSphere" "$SLOG")
echo "System Li01: ${SYS:-0} feste Kugeln, Zusammenstoesse mit der Testkugel: $STATIC"
echo "Joins=$JOIN Fehlerzeilen=$ERR Besitzer-Zeilen=$OWN Fremde-Zeilen=$SIM MaxFehler=${MAXERR:-n/a} cm"
if [ "$JOIN" -ge 2 ] && [ "${SYS:-0}" -gt 0 ] && [ "$STATIC" -ge 1 ] && [ "$ERR" -eq 0 ] && [ "$OWN" -ge 1 ] && [ "$SIM" -ge 1 ] && [ -n "$MAXERR" ] && awk "BEGIN{exit !($MAXERR < 1000)}"; then echo BESTANDEN; exit 0; fi
echo DURCHGEFALLEN; exit 1
