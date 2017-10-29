#!/usr/bin/env bash


pushd "$HOME"/ar-lab1/cmake-build-release  > /dev/null
# ./seq
mpiexec -ordered-output -prepend-rank ./parallel
popd  > /dev/null