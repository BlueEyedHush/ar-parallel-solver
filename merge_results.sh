#!/usr/bin/env bash

pushd cmake-build-release

rm -rf temp

mkdir results
mkdir temp

for tid in `seq 0 99`; do
    final_fname="t_$tid"
    paste -d '\n' results-parts/*_t_$tid > temp/${final_fname}
    sed -r 's/^\s+$//' temp/${final_fname} | cat -s > results/${final_fname}
done

popd
