#!/usr/bin/env bash

SCRIPT_DIR="$(readlink -e $(dirname ${BASH_SOURCE[0]}))"
BASE_DIR="$(readlink -e $(dirname ${BASH_SOURCE[0]})/../../)"

# load required modules
module load plgrid/tools/python/2.7.9

pushd ~/sao

pip install --user -r "$BASE_DIR"/requirements/computation.txt
python "$BASE_DIR"/evaluate.py

popd