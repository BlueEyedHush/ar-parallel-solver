
#include <time.h>
#include <iostream>
#include <cstddef>
#include <string>
#include <fstream>
#include <functional>
#include <sstream>
#include "shared.h"

/**
 * Work area is indexed from 1 (first element) to size (last element)
 */
class Workspace {
public:
	Workspace(const Coord size)
			: innerLength(size), outerLength(size+2), actualSize( outerLength*outerLength )
	{
		front = new NumType[actualSize];
		back = new NumType[actualSize];
	}

	~Workspace() {
		delete[] front;
		delete[] back;
	}

	/**
	 * @return reference to (i,j)th front buffer element
	 */
	inline NumType& elf(const Coord x, const Coord y) {
		return front[outerLength*x+y];
	}

	/**
	 * @return reference to (i,j)th back buffer element
	 */
	inline NumType& elb(const Coord x, const Coord y) {
		return back[outerLength*x+y];
	}

	void zeroBuffers(const NumType b) {
		for(Coord i = 0; i < actualSize; i++) {
			front[i] = b;
			back[i] = b;
		}
	}

	Coord getEdgeLength() {return outerLength;}

	void swap() {
		NumType* tmp = front;
		front = back;
		back = tmp;
	}

private:
	const Coord innerLength;
	const Coord outerLength;
	const Coord actualSize;
	NumType *front;
	NumType *back;

};


class FileDumper {
public:
	FileDumper(const std::string prefix, const Coord N) : prefix(prefix), N(N) {}

	void dumpBackbuffer(Workspace& w, const Coord t, const Coord linearDensity = DUMP_SPATIAL_FREQUENCY) {
		auto edgeLen  = w.getEdgeLength();
		auto step = edgeLen/linearDensity;

		filename.str("");
		filename << prefix << "_" << t;
		auto fname = filename.str();

		std::ofstream file;
		file.open(fname);
		file.precision(NumPrecision);

		loop(edgeLen, step, [=, &w, &file](const Coord i) {
			loop(edgeLen, step, [=, &w, &file](const Coord j) {
				auto x = vr(i);
				auto y = vr(j);
				file << x << " " << y << " " << t << " " << w.elb(i,j) << std::endl;
			});

			file << std::endl;
		});

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

int main(int argc, char **argv) {
	auto conf = parse_cli(argc, argv);

	Timer timer;
	Workspace w(conf.N);
	FileDumper d("./results/t", conf.N);

	#define I(IDX) IDX+1
	#define V(IDX) (IDX+1)*1.0/conf.N

	/* calculate helper values */
	const NumType h = 1.0/conf.N;
	const NumType k = h*h/4.0;
	const TimeStepCount dumpEvery = conf.timeSteps/DUMP_TEMPORAL_FREQUENCY;

	timer.start();
	/* fill in boundary condition */
	w.zeroBuffers(0.0);
	for(Coord x_idx = 0; x_idx < conf.N; x_idx++) {
		for(Coord y_idx = 0; y_idx < conf.N; y_idx++) {
			w.elf(I(x_idx),I(y_idx)) = f(V(x_idx),V(y_idx));
		}
	}

	w.swap();


	for(TimeStepCount step = 0; step < conf.timeSteps; step++) {
		for(Coord x_idx = 0; x_idx < conf.N; x_idx++) {
			for(Coord y_idx = 0; y_idx < conf.N; y_idx++) {
				w.elf(I(x_idx), I(y_idx)) = equation(
					 w.elb(I(x_idx-1), I(y_idx-1)),
					 w.elb(I(x_idx+1), I(y_idx-1)),
					 w.elb(I(x_idx-1), I(y_idx+1)),
					 w.elb(I(x_idx+1), I(y_idx+1))
				);
			}
		}

		w.swap();
		if (conf.outputEnabled && step % dumpEvery == 0) {
			d.dumpBackbuffer(w, step/dumpEvery);
		}
	}

	auto duration = timer.stop();
	std::cout << duration << std::endl;
	std::cerr << ((double)duration)/1000000000 << " s" << std::endl;

	return 0;
}