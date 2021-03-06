#!/usr/bin/gnuplot --persist

# n=<temporal frequency> parameter must be passed!

reset
set term gif animate
set output "animate.gif"
set view map
set yrange [0:1.15]
set xrange [0:1]
set cbrange [0:1]
do for [i=0:n-1]{
    splot sprintf("cmake-build-release/results/t_%i", i) using 1:2:4 with image title sprintf("%i", i)
}
set output