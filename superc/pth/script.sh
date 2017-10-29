#!/usr/bin/env bash


pushd "$HOME"/ar-lab1/cmake-build-release  > /dev/null
# ./seq
mpiexec -ordered-output ./parallel
popd  > /dev/null