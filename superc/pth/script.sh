#!/usr/bin/env bash

module load tools/impi/2018
module load plgrid/tools/cmake/3.7.2

pushd "$HOME"/ar-lab1/cmake-build-release  > /dev/null
# ./seq
mpiexec -ordered-output -prepend-rank ./parallel -o -t 10000
#mpiexec ./parallel
popd  > /dev/null