#!/usr/bin/env bash
# SPDX-License-Identifier: MPL-2.0
# FM-1 projects onto the Move: an FM-1 EVILGENIUS backup (.egbak, or the store .bin fizz_xdata writes: the same 200704
# bytes) read with the FM-1's own core (fm1dump), its projects added as new Move projects (fm1load: the first free
# slots from --first; nothing there already is touched).
#
#   tools/fm1import/convert.sh BACKUP.egbak STORE.bin [--first N]   into a Move store image here (made if missing)
#   tools/fm1import/convert.sh BACKUP.egbak --unit [--first N]      into the Move's own store ($MOVE, as deploy.sh):
#       EvilGenius is stopped, its store copied here and kept on the unit as evilgenius-store.bin.before-fm1-DATE, the
#       projects added, the store put back, EvilGenius started again
#
# FM1= the FM-1's EvilGenius checkout the reader is built from (read only), ../../FM1/EvilGenius beside this one by
# default.
set -euo pipefail
DIR="$(cd "$(dirname "$0")/../.." && pwd)"
[[ $# -ge 2 ]] || { sed -n '3,12p' "$0"; exit 2; }
BAK="$1"; OUT="$2"; shift 2
FIRST=(); [[ "${1:-}" == --first ]] && FIRST=(--first "$2")
make -s -C "$DIR/tools/fm1import" ${FM1:+FM1="$FM1"} >/dev/null
TXT="$DIR/build/fm1/$(basename "$BAK").txt"
"$DIR/build/fm1/fm1dump" "$BAK" >"$TXT"
echo "convert: read $(grep -c '^PROJECT' "$TXT") project(s) from $(basename "$BAK")"
if [[ "$OUT" != --unit ]]; then
    "$DIR/build/fm1/fm1load" "$TXT" "$OUT" ${FIRST[@]+"${FIRST[@]}"}
    exit 0
fi
HOST="${MOVE:-ableton@move.local}"
DEST=/data/UserData/schwung/modules/tools/evilgenius
SSH="ssh -o BatchMode=yes -o ConnectTimeout=8 -o LogLevel=ERROR"
STAMP="$(date +%Y%m%d-%H%M%S)"
$SSH "$HOST" 'P=$(pidof evilgenius-host); if [ -n "$P" ]; then kill -TERM $P; n=0; while [ $n -lt 40 ] && pidof evilgenius-host >/dev/null; do sleep 0.25; n=$((n+1)); done; sleep 3; echo "convert: stopped EvilGenius"; fi'
$SSH "$HOST" "cp $DEST/evilgenius-store.bin $DEST/evilgenius-store.bin.before-fm1-$STAMP"
mkdir -p "$DIR/build/unit-backup"
scp -q -o BatchMode=yes -o LogLevel=ERROR "$HOST:$DEST/evilgenius-store.bin" "$DIR/build/unit-backup/evilgenius-store-before-fm1-$STAMP.bin"
cp "$DIR/build/unit-backup/evilgenius-store-before-fm1-$STAMP.bin" "$DIR/build/fm1/unit-store.bin"
echo "convert: the Move's store kept (on the unit: evilgenius-store.bin.before-fm1-$STAMP; here: build/unit-backup/)"
"$DIR/build/fm1/fm1load" "$TXT" "$DIR/build/fm1/unit-store.bin" ${FIRST[@]+"${FIRST[@]}"}
scp -q -o BatchMode=yes -o LogLevel=ERROR "$DIR/build/fm1/unit-store.bin" "$HOST:$DEST/.evilgenius-store.bin.new"
$SSH "$HOST" "mv $DEST/.evilgenius-store.bin.new $DEST/evilgenius-store.bin"
$SSH "$HOST" "/data/UserData/schwung/launch-standalone.sh $DEST/standalone; sleep 9; pidof evilgenius-host >/dev/null && echo 'convert: EvilGenius running' || echo 'convert: EvilGenius NOT running'"
