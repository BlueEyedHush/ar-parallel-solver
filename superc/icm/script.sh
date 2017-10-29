#!/bin/bash -l

BASE_DIR="/icm/hydra/home/grid/plgblueeyedhush/sao"
SCRIPT_DIR="$BASE_DIR/superc/icm/"

# load required modules
module load plgrid/tools/python/2.7.5

pushd "$BASE_DIR"

pip install --user -r "$BASE_DIR"/requirements/computation.txt
python "$BASE_DIR"/evaluate.py

popd