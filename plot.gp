#!/usr/bin/gnuplot --persist

reset
set term gif animate
set output "animate.gif"
set view map
set yrange [0:1]
set xrange [0:1]
set cbrange [0:1]
n=100 # temporal frequency
do for [i=0:n-1]{
    splot sprintf("cmake-build-debug/results/t_%i", i) using 1:2:4 with image title sprintf("%i", i)
}
set output