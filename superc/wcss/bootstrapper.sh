#!/usr/bin/env bash

BASE_DIR="/home/grid/users/plgblueeyedhush/sao/"
SCRIPT_DIR="$BASE_DIR/superc/wcss/"

CMD="qsub -q plgrid-testing -l walltime=1:00:00 -l mem=2gb -l nodes=1:ppn=12 -N sao -A plgblueeyedhush2017a
    -m e -M knawara112@gmail.com $SCRIPT_DIR/script.sh"
echo "$CMD"
$CMD