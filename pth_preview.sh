#!/usr/bin/env bash

set -e

SCRIPT_DIR="$(readlink -e $(dirname ${BASH_SOURCE[0]}))"
PARTS="$SCRIPT_DIR"/cmake-build-release/results-parts/
FINAL="$SCRIPT_DIR"/cmake-build-release/results/
FRAMES="$SCRIPT_DIR"/cmake-build-release/frames/

mkdir -p "$PARTS"
mkdir -p "$FINAL"

if [ -z "$2" ]; then
    rm -f "$PARTS"/*
    rm -f "$FINAL"/*

    # download from PTH
    "$SCRIPT_DIR"/xresults_from_superc.sh pth
fi

# find highest node
N=`find cmake-build-release/results-parts/ -printf "%f\n" | sed -E 's/([0-9])+_t_[0-9]+/\1/' | sort -g | tail -n1`
Nn=$(( N+1 ))

# find highest t
T=`find cmake-build-release/results-parts/ -printf "%f\n" | sed -E 's/[0-9]+_t_([0-9]+)/\1/' | sort -g | tail -n1`
Tn=$(( T+1 ))

echo "Tn: $Tn; Nn: $Nn"

# python script
python "$SCRIPT_DIR"/merge_results.py $Nn $Tn

# plot
if [ "$1" == "a" ]; then
    gnuplot -e "n=$Tn" "$SCRIPT_DIR"/plot_pth.gp
elif [ "$1" == "f" ]; then
    mkdir -p "$FRAMES"
    rm -f "$FRAMES"/*
    gnuplot -e "n=$Tn" "$SCRIPT_DIR"/plot_pth_frames.gp
else
    echo "frame or animation mode?"
    exit 1
fi
