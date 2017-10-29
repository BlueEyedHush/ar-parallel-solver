
#include <iostream>
#include <cstddef>
#include <cmath>
#include <limits>
#include <string>
#include <fstream>
#include <functional>

using Coord = size_t;
using TimeStepCount = size_t;
using NumType = double;
const auto NumPrecision = std::numeric_limits<NumType>::max_digits10;

/* dimension of inner work area (without border) */
const Coord N = 1000;
const TimeStepCount STEPS_TO_SIMULATE = 100;
const Coord DUMP_SPATIAL_DENSITY_DEF = 25;
const TimeStepCount DUMP_TEMPORAL_DENSITY = 10;

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

#define I(IDX) IDX+1
#define V(IDX) (IDX+1)*1.0/N
#define VR(IDX) IDX*1.0/N

class FileDumper {
public:
	FileDumper(const std::string targetFile) {
		file.open(targetFile);
		file.precision(NumPrecision);
	}

	~FileDumper() {
		file.close();
	}

	void dumpBackbuffer(Workspace& w, const NumType t, const Coord linearDensity = DUMP_SPATIAL_DENSITY_DEF) {
		auto edgeLen  = w.getEdgeLength();
		auto step = edgeLen/linearDensity;

		loop(edgeLen, step, [=, &w](const Coord i) {
			loop(edgeLen, step, [=, &w](const Coord j) {
				auto x = VR(i);
				auto y = VR(j);
				this->file << x << " " << y << " " << t << " " << w.elb(i,j) << std::endl;
			});

			file << std::endl;
		});
	}

private:
	std::ofstream file;

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
};

/*
 * Must be defined on (0.0, 1.0)x(0.0, 1.0) surface
 */
NumType f(NumType x, NumType y) {
	return sin(M_PI*x)*sin(M_PI*y);
}

NumType equation(const NumType v_i_j, const NumType vi_j, const NumType v_ij, const NumType vij) {
	return 0.25*(v_i_j + v_ij + v_ij + vij);
}


int main() {
	std::cout << "Sequential variant" << std::endl;

	Workspace w(N);
	FileDumper d("./results");

	/* fill in boundary condition */
	w.zeroBuffers(0.0);
	for(Coord x_idx = 0; x_idx < N; x_idx++) {
		for(Coord y_idx = 0; y_idx < N; y_idx++) {
			w.elf(I(x_idx),I(y_idx)) = f(V(x_idx),V(y_idx));
		}
	}

	w.swap();

	d.dumpBackbuffer(w, 0.0);

	/* calculate helper values */
	const NumType h = 1.0/N;
	const NumType k = h*h/4.0;

	for(TimeStepCount step = 0; step < STEPS_TO_SIMULATE; step++) {
		for(Coord x_idx = 0; x_idx < N; x_idx++) {
			for(Coord y_idx = 0; y_idx < N; y_idx++) {
				w.elf(I(x_idx), I(y_idx)) = equation(
					 w.elb(I(x_idx-1), I(y_idx-1)),
					 w.elb(I(x_idx+1), I(y_idx-1)),
					 w.elb(I(x_idx-1), I(y_idx+1)),
					 w.elb(I(x_idx-1), I(y_idx+1))
				);
			}
		}

		w.swap();
		if (step % DUMP_TEMPORAL_DENSITY) {
			d.dumpBackbuffer(w, k*step);
		}
	}

	return 0;
}