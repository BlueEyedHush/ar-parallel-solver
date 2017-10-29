//
// Created by blueeyedhush on 29.10.17.
//

#ifndef LAB1_SHARED_H
#define LAB1_SHARED_H

#include <mpi.h>
#include <getopt.h>
#include <cassert>
#include <limits>
#include <cmath>
#include <iostream>
#include <sstream>
#include <functional>
#include <fstream>

// #define DEBUG

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
	Coord N = 100;
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

template <typename W>
class FileDumper {
public:
	FileDumper(const std::string prefix, const Coord N) : prefix(prefix), N(N) {}

	void dumpBackbuffer(W& w, const Coord t, const Coord linearDensity = DUMP_SPATIAL_FREQUENCY) {
		auto edgeLen  = w.getEdgeLength();
		auto step = edgeLen/linearDensity;

		assert(step > 0);

		std::cerr << edgeLen << " " << step << std::endl;

		filename.str("");
		filename << prefix << "_" << t;
		auto fname = filename.str();

		std::ofstream file;
		file.open(fname);
		file.precision(NumPrecision);

		#ifdef DEBUG
		std::cerr << "dumping" << std::endl;
		#endif

		loop(edgeLen, step, [=, &w, &file](const Coord i) {
			loop(edgeLen, step, [=, &w, &file](const Coord j) {
				auto x = vr(i);
				auto y = vr(j);
				file << x << " " << y << " " << t << " " << w.elb(i,j) << std::endl;
			});

			file << std::endl;
		});


		#ifdef DEBUG
		std::cerr << "dump finished" << std::endl;
		#endif

		file.close();
	}

private:
	const std::string prefix;
	const Coord N;
	std::ostringstream filename;

	void loop(const Coord limit, const Coord step, std::function<void(const Coord)> f) {
		bool iShouldContinue = true;
		size_t i = 0;

		while(iShouldContinue) {
			if(i >= limit) {
				i = limit-1;
				iShouldContinue = false;
			}

			f(i);

			i += step;
		}
	}

	NumType vr(const Coord idx) {
		return idx*1.0/N;
	}
};


/*
 * Must be defined on (0.0, 1.0)x(0.0, 1.0) surface
 */
NumType f(NumType x, NumType y) {
	return sin(M_PI*x)*sin(M_PI*y);
}

NumType equation(const NumType v_i_j, const NumType vi_j, const NumType v_ij, const NumType vij) {
	return 0.25*(v_i_j + v_ij + vi_j + vij);
}


#endif //LAB1_SHARED_H
