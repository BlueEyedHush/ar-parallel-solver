#!/usr/bin/env bash

SCRIPT_DIR="$(readlink -e $(dirname ${BASH_SOURCE[0]}))"
BASE_DIR="$(readlink -e $(dirname ${BASH_SOURCE[0]})/../../)"

USAGE="bootstrapper.sh <variant> <node_num> single|multiple <time_steps> <grid_size>"

if [ -z "$1" ]; then
    echo "$USAGE"
    exit 1
fi

if [ -z "$2" ]; then
    echo "$USAGE"
    exit 1
else
    PROCESS_COUNT="$2"
fi

if [ -z "$3" ]; then
    echo "$USAGE"
else
    MODE="$3"
fi

# "$SCRIPT_DIR"/build.sh "$1"

rm -f "$BASE_DIR"/ar.se
rm -f "$BASE_DIR"/ar.so

MEM="16gb"

if [ "$MODE" == 'single' ]; then
    TPN=$PROCESS_COUNT
    NC=1
    OPT="-o"
else
    NC=$PROCESS_COUNT
    TPN=1
    OPT=""
fi

CMD="sbatch
    -J ar-1
    -N $NC
    --ntasks-per-node $TPN
    --mem $MEM
    --time=00:10:00
    -A ccbmc6
    -p plgrid-testing
    --output ar.so
    --error ar.se
    --mail-type=END,FAIL
    --mail-user=knawara112@gmail.com
    $SCRIPT_DIR/run.sh $1 $4 $5 $OPT"

echo "$CMD"
$CMD