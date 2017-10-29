#!/usr/bin/env bash

REL_DIR="$(dirname "${BASH_SOURCE[0]}")"
DIR="$(readlink -e $REL_DIR)"

if [ $1 == "zeus" ]; then
    MACHINE=zeus.cyfronet.pl
elif [ $1 == "icm" ]; then
    MACHINE=login.icm.edu.pl
elif [ $1 == "wcss" ]; then
    MACHINE="bem.wcss.pl"
elif [ $1 == "pth" ]; then
    MACHINE="prometheus.cyfronet.pl"
else
    echo "usage: script.sh zeus|icm|wcss|pth"
    exit 1
fi

PASS=`cat $DIR/pass`
sshpass -p "$PASS" rsync -avzr \
    --exclude *.ipynb \
    --exclude .ipynb_checkpoints/ \
    --exclude pass \
    --exclude results/ \
    --exclude .git/ \
    --exclude .idea/ \
    --exclude animate.gif \
    --exclude cmake-build-debug/ \
    --exclude cmake-build-release/ \
    --exclude .gitignore \
    "$DIR"/. plgblueeyedhush@$MACHINE:ar-lab1

