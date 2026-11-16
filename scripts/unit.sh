#!/usr/bin/env bash
# SPDX-License-Identifier: MPL-2.0
# Drive an EvilGenius started with the test hook (scripts/deploy.sh --test) over ssh, with nobody at the unit.
#   scripts/unit.sh run SCRIPT [OUT_DIR]   a simulator script (tests/*.txt: wait, note, cc, tap, shot, state) played
#                                          on the unit; shots are its real screen, state its real state
#   scripts/unit.sh state                  the state line now
#   scripts/unit.sh log                    the host's log
set -euo pipefail
DIR="$(cd "$(dirname "$0")/.." && pwd)"
HOST="${MOVE:-ableton@move.local}"
SSH="ssh -o BatchMode=yes -o ConnectTimeout=8 -o LogLevel=ERROR"
case "${1:-}" in
state) $SSH "$HOST" 'cat /dev/shm/egm-state' ;;
log) $SSH "$HOST" 'cat /data/UserData/schwung/evilgenius-host.log' ;;
run)
    OUT="${3:-$DIR/build/unit}"
    mkdir -p "$OUT"
    rm -f "$OUT"/*.bin "$OUT"/*.pgm
    # the script becomes one shell program run on the unit: no ssh round trip a step
    {
        echo 'C=/dev/shm/egm-cmd; mkdir -p /dev/shm/egm-shots; rm -f /dev/shm/egm-shots/*'
        while read -r cmd a b _; do
            case "$cmd" in
            wait) echo "usleep ${a}000 2>/dev/null || sleep 0.$(printf '%03d' "$a")" ;;
            note | cc) echo "echo '$cmd $a $b' >\$C" ;;
            tap) echo "echo 'cc $a 127' >\$C; usleep 30000; echo 'cc $a 0' >\$C; usleep 30000" ;;
            shot) echo "usleep 80000; cp /dev/shm/schwung-display-live /dev/shm/egm-shots/$a.bin" ;;
            state) echo "usleep 300000; echo \"state $a \$(cat /dev/shm/egm-state)\"" ;;
            esac
        done < <(sed 's/#.*//' "$2")
    } >"$OUT/remote.sh"
    $SSH "$HOST" 'sh -s' <"$OUT/remote.sh"
    $SSH "$HOST" 'tar -cf - -C /dev/shm/egm-shots .' | tar -xf - -C "$OUT" 2>/dev/null || true
    python3 - "$OUT" <<'PY'
import glob, os, sys
for f in glob.glob(os.path.join(sys.argv[1], '*.bin')):
    d = open(f, 'rb').read()
    if len(d) != 1024:
        continue
    S = 4
    out = bytearray()
    for y in range(64 * S):
        for x in range(128 * S):
            out.append(255 if (d[(y // S // 8) * 128 + x // S] >> ((y // S) & 7)) & 1 else 0)
    open(f[:-4] + '.pgm', 'wb').write(b'P5\n%d %d\n255\n' % (128 * S, 64 * S) + bytes(out))
PY
    python3 "$DIR/scripts/sheet.py" "$OUT" "$OUT/sheet.png" 4
    ;;
*) echo "usage: unit.sh run SCRIPT [OUT_DIR] | state | log" >&2; exit 2 ;;
esac
