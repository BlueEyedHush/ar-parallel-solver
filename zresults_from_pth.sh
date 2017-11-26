#!/usr/bin/env bash

REL_DIR="$(dirname "${BASH_SOURCE[0]}")"
DIR="$(readlink -e $REL_DIR)"

MACHINE="prometheus.cyfronet.pl"

SYNC_BASE="$DIR/results/"
TARGET_DIR="$SYNC_BASE"/$1/
mkdir -p "$SYNC_BASE"

if [ -z $2 ]; then
    rm -f "$TARGET_DIR"*
    PASS=`cat $DIR/pass`
    sshpass -p "$PASS" rsync -avzr plgblueeyedhush@$MACHINE:ar-lab1/results/$1 "$SYNC_BASE"
fi

RAW_FILE="$TARGET_DIR"../$1.raw
FINAL_FILE="$TARGET_DIR"../$1.final
cat "$TARGET_DIR"* > "$RAW_FILE"
cat "$RAW_FILE" | sed '/^=/ d' | sed '/^ *Intel/ d' | sed '/^ *https/ d' | sed '/^ *$/ d' > "$FINAL_FILE"