#!/usr/bin/gnuplot --persist

reset
set term gif animate
set output "animate.gif"
set view map
n=100 # temporal frequency
do for [i=0:n-1]{
    splot sprintf("cmake-build-debug/results/t_%i", i) using 1:2:4 with image
}
set output