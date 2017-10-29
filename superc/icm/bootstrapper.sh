#!/bin/bash -l

BASE_DIR="/icm/hydra/home/grid/plgblueeyedhush/sao"
SCRIPT_DIR="$BASE_DIR/superc/icm/"

CMD="sbatch -J sao -N 1 --ntasks-per-node 12 --mem 4gb --time=00:10:00 -A plgblueeyedhush2017a -p plgrid-testing
    --output icm.so --error icm.se --mail-type=END,FAIL --mail-user=knawara112@gmail.com $SCRIPT_DIR/script.sh"
echo "$CMD"
$CMD
