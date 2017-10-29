#!/usr/bin/env bash

SCRIPT_DIR="$(readlink -e $(dirname ${BASH_SOURCE[0]}))"
BASE_DIR="$(readlink -e $(dirname ${BASH_SOURCE[0]})/../../)"

CMD="qsub -q plgrid-testing -l walltime=1:00:00 -l mem=2gb -l nodes=1:ppn=12 -N sao -A plgblueeyedhush2017a
    -m e -M knawara112@gmail.com $SCRIPT_DIR/script.sh"
echo "$CMD"
$CMD