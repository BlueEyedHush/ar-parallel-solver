//
// Created by blueeyedhush on 29.10.17.
//

#ifndef LAB1_SHARED_H
#define LAB1_SHARED_H

#include <mpi.h>
#include <getopt.h>
#include <limits>

// @todo modified while working on parallel, possible source of bugs
using Coord = long long;
using TimeStepCount = size_t;
using NumType = double;
const MPI_Datatype NUM_MPI_DT = MPI_DOUBLE;
const auto NumPrecision = std::numeric_limits<NumType>::max_digits10;
using Duration = long long;

/* dimension of inner work area (without border) */
const Coord DUMP_SPATIAL_FREQUENCY = 25;
const TimeStepCount DUMP_TEMPORAL_FREQUENCY = 100;

/* for nice plot: N = 40, timeSteps = 400 */
struct Config {
	Coord N = 40;
	TimeStepCount timeSteps = 400;
	bool outputEnabled = false;
};

Config parse_cli(int argc, char **argv) {
	Config conf;

	int c;
	while (1) {
		c = getopt(argc, argv, "n:t:o");
		if (c == -1)
			break;

		switch (c) {
			case 'n':
				conf.N = std::stoull(optarg);
				break;
			case 't':
				conf.timeSteps = std::stoull(optarg);
				break;
			case 'o':
				conf.outputEnabled = true;
				break;
		}
	}

	std::cerr << "N = " << conf.N << ", timeSteps = " << conf.timeSteps << ", output = " << conf.outputEnabled
	          << std::endl;

	return conf;
}


#endif //LAB1_SHARED_H
