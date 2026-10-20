#!/bin/bash
# Lasttest M2: Server + N Bot-Clients (ohne Grafik) im Level L_FlightTest. Misst Server-Last, Speicher und Vorhersagefehler.
#   Tools/load_flight_net.sh [Clients=12] [Sekunden=60] [Archive-Ordner]
# Voraussetzung: gekochte Pakete in ~/UEBuild/Archive. Exit-Code 0 = bestanden (Besitzer ohne Abweichung > 150 cm bzw. > 1000 cm nach Kollisionen, keine Ablehnungen, Server-Tick <= 40 ms).
export LC_ALL=C   # Dezimalpunkt statt -komma (awk/sort)
N=${1:-12}
T=${2:-60}
A=${3:-$HOME/UEBuild/Archive}
SLOG=$A/LinuxServer/Freelancer/Saved/Logs/Freelancer.log
pkill -x FreelancerServe; pkill -x FreelancerClien; sleep 2
rm -f "$A"/LinuxServer/Freelancer/Saved/Logs/*.log "$A"/LinuxClient/Freelancer/Saved/Logs/*.log
"$A/LinuxServer/FreelancerServer.sh" /Game/Freelancer/Maps/L_FlightTest -log -port=7777 -ForceLogFlush > /dev/null 2>&1 &
sleep 40
for i in $(seq 1 "$N"); do
  # eigenes Log je Client (-LOG=...), damit alle ausgewertet werden koennen
  "$A/LinuxClient/FreelancerClient.sh" 127.0.0.1:7777 -nullrhi -nosound -log -LOG=load_client_$i.log -ForceLogFlush -FLBot -ExecCmds="t.MaxFPS 60" -ResX=320 -ResY=200 > /dev/null 2>&1 &
  sleep 2
done
LO0=$(awk '/^ *lo:/ {print $2+$10}' /proc/net/dev)   # Bytes gesamt (rx+tx) auf Loopback
sleep "$T"
LO1=$(awk '/^ *lo:/ {print $2+$10}' /proc/net/dev)
KBPS=$(awk "BEGIN{printf \"%.0f\", ($LO1-$LO0)/$T/1024/2}")   # rx+tx gezaehlt, Paket einmal gesendet = einmal empfangen
# Der Starter-Prozess (Shell-Skript) heisst gleich wie das Spiel: je Name den Prozess mit dem groessten RSS nehmen
SBEST=$(ps -C FreelancerServe -o rss=,pcpu= 2>/dev/null | sort -rn | head -1)
SRSS=$(echo "$SBEST" | awk '{printf "%.0f", $1/1024}')
SCPU=$(echo "$SBEST" | awk '{print $2}')
CRSS=$(ps -C FreelancerClien -o rss= 2>/dev/null | awk '$1>200000 {s+=$1;n++} END{ if(n) printf "%.0f", s/n/1024; else print 0}')
CCOUNT=$(ps -C FreelancerClien -o rss= 2>/dev/null | awk '$1>200000' | wc -l)
pkill -x FreelancerClien; pkill -x FreelancerServe; sleep 2
LOGS=$(ls "$A"/LinuxClient/Freelancer/Saved/Logs/load_client_*.log 2>/dev/null)
OWN=$(cat $LOGS 2>/dev/null | grep "LogFLShip" | grep -c "Rolle=2")
SIM=$(cat $LOGS 2>/dev/null | grep "LogFLShip" | grep -c "Rolle=1")
MAXERR=$(cat $LOGS 2>/dev/null | grep "Rolle=2" | sed -E 's/.*MaxFehler=([0-9.]+) cm.*/\1/' | sort -n | tail -1)
CORR=$(cat $LOGS 2>/dev/null | grep "Rolle=2" | sed -E 's/.*Korrekturen=([0-9]+) .*/\1/' | sort -n | tail -1)
REJ=$(grep "Rolle=3" "$SLOG" | sed -E 's/.*Abgelehnt=([0-9]+) .*/\1/' | sort -n | tail -1)
DT=$(grep "Rolle=3" "$SLOG" | sed -E 's/.* dt=([0-9.]+) .*/\1/' | tail -20 | awk '{s+=$1;n++} END{ if(n) printf "%.4f", s/n; else print 0}')
JOIN=$(grep -c "Join succeeded" "$SLOG")
DEAD=$(grep -c "Zerstoert:" "$SLOG")
COLL=$(grep -c "LogFLShip: Display: Kollision" "$SLOG")   # Zusammenstoesse (Server korrigiert die Clients, das ist erlaubt)
ERR=$(cat "$SLOG" $LOGS 2>/dev/null | grep -c "LogOutputDevice: Error")
echo "Clients gestartet=$N laufend=$CCOUNT Joins=$JOIN Besitzer-Zeilen=$OWN Fremde-Zeilen=$SIM"
echo "Server: CPU=${SCPU}% RAM=${SRSS} MB mittlere Frame-Zeit=${DT} s (${DT}s -> $(awk "BEGIN{ if (${DT:-0}>0) printf \"%.0f\", 1/${DT}; else print 0}") FPS) abgelehnte Bewegungen max=${REJ:-n/a}"
echo "Netz (Loopback, gesamt Server<->Clients): ${KBPS} KB/s = $(awk "BEGIN{printf \"%.1f\", $KBPS/$N}") KB/s je Client"
echo "Kollisionen (Server)=$COLL Zerstoert=$DEAD"
echo "Clients: RAM je Client=${CRSS} MB, max. Vorhersagefehler=${MAXERR:-n/a} cm, max. Korrekturen je Client=${CORR:-n/a}, Fehlerzeilen=$ERR"
OK=1
[ "$JOIN" -ge "$N" ] || OK=0
# ohne Kollisionen < 150 cm, mit Kollisionen (Server verschiebt Schiffe) < 1000 cm
LIMIT=150; [ "$COLL" -gt 0 ] && LIMIT=1000; [ "$DEAD" -gt 0 ] && LIMIT=1000000   # Neustarts teleportieren Schiffe
[ -n "$MAXERR" ] && awk "BEGIN{exit !($MAXERR < $LIMIT)}" || OK=0
[ "${REJ:-1}" = "0" ] || OK=0
awk "BEGIN{exit !(${DT:-1} <= 0.04)}" || OK=0
[ "$ERR" -eq 0 ] || OK=0
[ "$OK" -eq 1 ] && { echo BESTANDEN; exit 0; }
echo DURCHGEFALLEN; exit 1
