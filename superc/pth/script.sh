#!/usr/bin/env bash

SCRIPT_DIR="$(readlink -e $(dirname ${BASH_SOURCE[0]}))"
BASE_DIR="$(readlink -e $(dirname ${BASH_SOURCE[0]})/../../)"

# load required modules
module load tools/impi/2018
module load plgrid/tools/cmake/3.7.2

pushd "$HOME"/ar-lab1/

mkdir -p cmake-build-release
pushd cmake-build-release

cmake -DCMAKE_C_COMPILER=icc -DCMAKE_CXX_COMPILER=icpc -DCMAKE_BUILD_TYPE=Release ../
make
./seq

popd
popd