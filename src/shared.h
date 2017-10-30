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

using Coord = long long;
using TimeStepCount = size_t;
using NumType = double;
const MPI_Datatype NUM_MPI_DT = MPI_DOUBLE;
const auto NumPrecision = std::numeric_limits<NumType>::max_digits10;
using Duration = long long;

/* dimension of inner work area (without border) */
const Coord DUMP_SPATIAL_FREQUENCY = 25;
const TimeStepCount DUMP_TEMPORAL_FREQUENCY = 100;

/* 0                       1
 *    _*_*_*_*_ _*_*_*_*_
 * * | * * * * | * * * * | *
 * * | * * * * | * * * * | *
 *
 * That's how we partition vertices across nodes; external vertices are boundary conditions
 * - innerSize - grid length without boundary conditions
 * - outerSize - grid length including boundary conditions
 * Non-edge nodes instead of boundary conditions have values copied over from other nodes.
 *
 * nodes in each partition are indexed from 0 to n_partitioned-1
 * global offset of nodes in partition is (0, n_partitioned)
 * but if we want math offset, we get (0, (n_partitioned+1)*h) /because of boundary being outside/
 *
 * Node-to-carthesian system mapping:
 * | 2 | 3 |
 * | 0 | 1 |
 * node_row no -> y
 * node_column no -> x
 * _don't mix indexing within workspace/any other array into it_ (just remember how you did it and be consistent)
 *
 * WRONG!
 * if points in-between, then we have h/2 from the boundary
 *   it shouldn't be a problem, since boundary is 0 everywhere? (but outside, not inside)
 * additionally, if we divide into 40 intervals, we have 39 (not 38 points) in between
 * if we have 0 and 1 aligned with points, we don't have such problems
 *
 * N = 4
 * outer = 6
 * on [0,1] interval
 * divided by 6 we get 1/6th...
 * if we have 4 points in the middle, we should divide by 5 (6 points == 5 intervals)!!!
 *
 */
class Partitioner {
	/* Responsibilities
	 * - check for partitioning correctness
	 * - get index and numerical offsets
	 */
public:
	Partitioner(const Coord node_count, const NumType lower_b, NumType upper_b, const Coord grid_dimm)
			: nodeCount(node_count), lowerB(lower_b), upperB(upper_b), grid_dimm(grid_dimm)
	{
		h = (upperB - lowerB)/(grid_dimm+1);
		verify_values();
	}

	int get_n_slice() {
		return mh;
	}

	Coord partition_inner_size() {
		return grid_dimm/sideLen;
	}

	NumType get_h() {
		return h;
	}


	/**
	 * Math offsets across point grid
	 */
	std::pair<NumType, NumType> get_math_offset_node(const int node_row, const int node_column) {
		auto x = (node_column*mh + 1)*h;
		auto y = (node_row*mh + 1)*h;
		return std::make_pair(x,y);
	};

	std::pair<int, int> node_id_to_grid_pos(int nodeId) {
		const auto row = nodeId/sideLen;
		const auto column = nodeId%sideLen;
		return std::make_pair(row, column);
	};

	int get_nodes_grid_dimm() {
		return sideLen;
	}

private:
	/* characteristics of node grid */
	const int nodeCount;
	int sideLen;
	Coord row;
	Coord column;

	/* qualities linking both worlds */
	int mh;

	/* characteristics of stored values and point grids */
	const NumType lowerB;
	const NumType upperB;
	const Coord grid_dimm;
	NumType h;

	void verify_values() {
		auto sqr = static_cast<long>(std::sqrt(nodeCount));
		if(sqr*sqr != nodeCount) {
			throw std::runtime_error("numer of nodes must be square");
		} else {
			sideLen = sqr;
		}

		if(grid_dimm % sideLen != 0) {
			throw std::runtime_error("point grid len must be evenly divisible by machine grid len");
		} else {
			mh = grid_dimm/sideLen;
		}
	}
};

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

/**
 * It doesn't plot borders, so it always queries workspace from 0 to size-1
 */
template <typename W>
class FileDumper {
public:
	FileDumper(const std::string prefix,
	           const Coord n_partition,
	           const NumType offset_x,
	           const NumType offset_y,
	           const NumType step)
			: prefix(prefix), N(n_partition), offset_x(offset_x), offset_y(offset_y), step(step) {}

	void dumpBackbuffer(W& w, const Coord t, const Coord keep_snapshots = DUMP_SPATIAL_FREQUENCY) {

		auto edgeLen  = w.getInnerLength();
		auto step = edgeLen/keep_snapshots;

		#ifdef DEBUG
		std::cerr << "edgeLen: " << edgeLen
				  << "keep_snapshots" << keep_snapshots
		          << " step: " << step
		          << " offset_x: " << offset_x
		          << " offset_y: " << offset_y
		          << std::endl;
		#endif

		if(step < 1) {
			throw std::runtime_error("FileDumper: step == 0 -> infinite iteration");
		}

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
				auto x = vr_x(i);
				auto y = vr_y(j);
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

	const NumType offset_x;
	const NumType offset_y;
	const NumType step;

	void loop(const Coord limit, const Coord step, std::function<void(const Coord)> f) {
		bool iShouldContinue = true;
		size_t i = 0;

		while(iShouldContinue) {
			if(i >= limit) {
				iShouldContinue = false;

				/* should we do one more iteration with variable exact to limit-1? */
				if(i - limit > step/4) {
					i = limit-1;
				} else {
					break;
				}
			}

			f(i);

			i += step;
		}
	}

	NumType vr_x(const Coord idx) {
		return offset_x + idx*step;
	}

	NumType vr_y(const Coord idx) {
		return offset_y + idx*step;
	}
};


class Timer {
public:
	Timer() {
		resetTm(tm);
		clock_getres(CLOCK, &tm);
		std::cerr << "Clock resolution: " << tm.tv_sec << " s " << tm.tv_nsec << " ns" << std::endl;
		resetTm(tm);
	}

	void start() {
		clock_gettime(CLOCK, &tm);
	}

	Duration stop() {
		timespec endTm;
		resetTm(endTm);

		clock_gettime(CLOCK, &endTm);

		Duration start = conv(tm);
		Duration end = conv(endTm);

		return end - start;
	}

private:
	const static auto CLOCK = CLOCK_MONOTONIC;
	timespec tm;

	void resetTm(timespec& t) {
		t.tv_sec = 0;
		t.tv_nsec = 0;
	}

	/**
	 * @return value in us
	 */
	Duration conv(timespec& t) {
		return t.tv_sec*1000000000 + t.tv_nsec;
	}
};



/*
 * Must be defined on (0.0, 1.0)x(0.0, 1.0) surface
 */
NumType f(NumType x, NumType y) {
	return sin(M_PI*x)*sin(M_PI*y);
}

NumType equation(const NumType v_i_j, const NumType vi_j, const NumType v_ij, const NumType vij) {
	auto val = 0.25*(v_i_j + v_ij + vi_j + vij);
	#ifdef DEBUG
	std::cerr << "(" << v_i_j << "," << vi_j << "," << v_ij << "," << vij  << "," << val << ")" << std::endl;
	#endif
	return val;
}


#endif //LAB1_SHARED_H
