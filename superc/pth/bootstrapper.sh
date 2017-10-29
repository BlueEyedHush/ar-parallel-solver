#!/usr/bin/env bash

SCRIPT_DIR="$(readlink -e $(dirname ${BASH_SOURCE[0]}))"
BASE_DIR="$(readlink -e $(dirname ${BASH_SOURCE[0]})/../../)"

CMD="sbatch
    -J ar-1
    -N 1
    --ntasks-per-node 1
    --mem 1gb
    --time=00:10:00
    -A ccbmc6
    -p plgrid-testing
    --output ar.so
    --error ar.se
    --mail-type=END,FAIL
    --mail-user=knawara112@gmail.com
    $SCRIPT_DIR/script.sh"

echo "$CMD"
$CMD