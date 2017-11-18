#!/usr/bin/env bash

module load tools/impi/2018
module load plgrid/tools/cmake/3.7.2

pushd "$HOME"/ar-lab1/cmake-build-release  > /dev/null

rm -f ./results/*

# ./seq
mpiexec -ordered-output -prepend-rank ./$1 -o -t 10 -n 20
#mpiexec ./parallel
popd  > /dev/null