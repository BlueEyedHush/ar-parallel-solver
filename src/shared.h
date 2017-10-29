//
// Created by blueeyedhush on 29.10.17.
//

#ifndef LAB1_SHARED_H
#define LAB1_SHARED_H

#include <limits>

using Coord = size_t;
using TimeStepCount = size_t;
using NumType = double;
const auto NumPrecision = std::numeric_limits<NumType>::max_digits10;
using Duration = long long;

/* dimension of inner work area (without border) */
const Coord N = 50;
const TimeStepCount STEPS_TO_SIMULATE = 400;
const Coord DUMP_SPATIAL_FREQUENCY = 25;
const TimeStepCount DUMP_TEMPORAL_FREQUENCY = 100;

#endif //LAB1_SHARED_H
