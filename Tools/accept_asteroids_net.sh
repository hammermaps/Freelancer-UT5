#!/bin/bash
# Abnahmetest M2: Asteroidenfelder als Hindernisse. Server (System Li01) + 2 Bot-Clients starten mitten im Truemmerfeld
# Zone_Li01_Pittsburgh_debris_001 (Zonenmitte FL (47675, 0, -63380) m = Unreal (6338000, 4767500, 0) cm).
# Bestanden = Felder aktiv, Bots kollidieren mit Asteroiden, 2 Joins, keine Fehlerzeilen, Server >= ~25 FPS.
export LC_ALL=C
A=${1:-$HOME/UEBuild/Archive}
SLOG=$A/LinuxServer/Freelancer/Saved/Logs/Freelancer.log
pkill -x FreelancerServe; pkill -x FreelancerClien; sleep 2
rm -f "$A"/LinuxServer/Freelancer/Saved/Logs/*.log "$A"/LinuxClient/Freelancer/Saved/Logs/*.log
"$A/LinuxServer/FreelancerServer.sh" /Game/Freelancer/Maps/L_FlightTest -log -port=7777 -ForceLogFlush -FLSystem=Li01 -FLSpawnOffset=6338000,4767500,0 > /dev/null 2>&1 &
sleep 45
for i in 1 2; do
  "$A/LinuxClient/FreelancerClient.sh" 127.0.0.1:7777 -nullrhi -nosound -log -ForceLogFlush -FLBot -ExecCmds="t.MaxFPS 60" -ResX=640 -ResY=360 > /dev/null 2>&1 &
  sleep 3
done
sleep 40
pkill -x FreelancerClien; pkill -x FreelancerServe; sleep 2
FIELDS=$(grep -h "Asteroidenfelder aktiv" "$SLOG" | sed -E 's/.*: ([0-9]+) Asteroidenfelder.*/\1/' | head -1)
AST=$(grep "Kollision" "$SLOG" | grep -c "fest li01_")
OTHER=$(grep "Kollision" "$SLOG" | grep -c "fest Sun_\|fest TestSphere")
JOIN=$(grep -c "Join succeeded" "$SLOG")
ERR=$(cat "$SLOG" "$A"/LinuxClient/Freelancer/Saved/Logs/*.log 2>/dev/null | grep -c "LogOutputDevice: Error")
DT=$(grep "Rolle=3" "$SLOG" | sed -E 's/.* dt=([0-9.]+) .*/\1/' | tail -10 | awk '{s+=$1;n++} END{ if(n) printf "%.4f", s/n; else print 1}')
echo "Felder aktiv=${FIELDS:-0} Zusammenstoesse mit Asteroiden=$AST (sonstige feste=$OTHER) Joins=$JOIN Fehlerzeilen=$ERR Server-Frame-Zeit=${DT}s"
grep "Kollision" "$SLOG" | grep "fest li01_" | head -3 | sed -E 's/^\[[^]]*\]\[[^]]*\]//' | cut -c1-160
if [ "${FIELDS:-0}" -ge 1 ] && [ "$AST" -ge 1 ] && [ "$JOIN" -ge 2 ] && [ "$ERR" -eq 0 ] && awk "BEGIN{exit !($DT <= 0.04)}"; then echo BESTANDEN; exit 0; fi
echo DURCHGEFALLEN; exit 1
