#!/usr/bin/env bash

module load tools/impi/2018
module load plgrid/tools/cmake/3.7.2

pushd "$HOME"/ar-lab1/cmake-build-release  > /dev/null

rm -f ./results/*

if [ "$1" == "seq" ]; then
    ./seq -t $2 -n $3 $4
else
    mpiexec -ordered-output -prepend-rank ./$1 -o -t $2 -n $3 $4
fi
popd  > /dev/null