#!/bin/bash
# Abnahmetest M1: Server und Client laden das Schiff im Level L_ShipView identisch.
# Voraussetzung: gekochte Pakete in ~/UEBuild/Archive (Engine/cook_freelancer.sh). Ausgabe 0 = bestanden.
A=${1:-$HOME/UEBuild/Archive}
SLOG=$A/LinuxServer/Freelancer/Saved/Logs/Freelancer.log
CLOG=$A/LinuxClient/Freelancer/Saved/Logs/Freelancer.log
pkill -x FreelancerServe; pkill -x FreelancerClien; sleep 2
rm -f "$SLOG" "$CLOG"
"$A/LinuxServer/FreelancerServer.sh" /Game/Freelancer/Maps/L_ShipView -log -port=7777 -ForceLogFlush > /dev/null 2>&1 &
sleep 45
"$A/LinuxClient/FreelancerClient.sh" 127.0.0.1:7777 -nullrhi -nosound -log -ForceLogFlush -ResX=640 -ResY=360 > /dev/null 2>&1 &
sleep 40
pkill -x FreelancerClien; pkill -x FreelancerServe
S=$(grep -h "FLShipVisual FLShipVisual_0: Modell" "$SLOG" | sed -E 's/^\[[^]]*\]\[[^]]*\]//' | head -1)
C=$(grep -h "FLShipVisual FLShipVisual_0: Modell" "$CLOG" | sed -E 's/^\[[^]]*\]\[[^]]*\]//' | head -1)
ERR=$(grep -c "LogOutputDevice: Error" "$SLOG" "$CLOG" | tr '\n' ' ')
echo "Server: $S"; echo "Client: $C"; echo "Fehlerzeilen (Server/Client): $ERR"
JOIN=$(grep -c "Join succeeded" "$SLOG")
[ -n "$S" ] && [ "$S" = "$C" ] && [ "$JOIN" -ge 1 ] && ! grep -q "LogOutputDevice: Error" "$SLOG" "$CLOG" && { echo BESTANDEN; exit 0; }
echo DURCHGEFALLEN; exit 1
