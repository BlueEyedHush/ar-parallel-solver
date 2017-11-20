#!/usr/bin/env bash

SCRIPT_DIR="$(readlink -e $(dirname ${BASH_SOURCE[0]}))"

export PYTHONPATH="$PYTHONPATH":"$SCRIPT_DIR"
python "$SCRIPT_DIR"/"$1"_scheduler.py