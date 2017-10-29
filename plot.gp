#!/usr/bin/gnuplot --persist

set view map
splot "cmake-build-debug/results" using 1:2:4 with image
