#!/usr/bin/env bash

SCRIPT_DIR="$(readlink -e $(dirname ${BASH_SOURCE[0]}))"
BASE_DIR="$(readlink -e $(dirname ${BASH_SOURCE[0]})/../../)"

USAGE="bootstrapper.sh <variant> <node num> test|bench [nobuild]"

if [ -z "$1" ]; then
    echo "$USAGE"
    exit 1
fi

if [ -z "$2" ]; then
    echo "$USAGE"
    exit 1
else
    TPN="$2"
fi

if [ -z "$3" ]; then
    echo "$USAGE"
else
    MODE="$3"
fi

if [ -z "$4" ]; then
    "$SCRIPT_DIR"/build.sh "$1"
fi

rm -f "$BASE_DIR"/ar.se
rm -f "$BASE_DIR"/ar.so

if [ "$MODE" == 'test' ]; then
    CMD="sbatch
        -J ar-1
        -N 1
        --ntasks-per-node $TPN
        --mem 1gb
        --time=00:10:00
        -A ccbmc6
        -p plgrid-testing
        --output ar.so
        --error ar.se
        --mail-type=END,FAIL
        --mail-user=knawara112@gmail.com
        $SCRIPT_DIR/script.sh $1"
else
    CMD="sbatch
        -J ar-1
        -N $TPN
        --ntasks-per-node 1
        --mem 1gb
        --time=00:10:00
        -A ccbmc6
        -p plgrid-testing
        --output ar.so
        --error ar.se
        --mail-type=END,FAIL
        --mail-user=knawara112@gmail.com
        $SCRIPT_DIR/script.sh $1"
fi

echo "$CMD"
$CMD