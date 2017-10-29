
#include <iostream>
#include <cstddef>
#include <cmath>
#include <limits>
#include <string>
#include <fstream>

using Coord = size_t;
using NumType = double;
const auto NumPrecision = std::numeric_limits<NumType>::max_digits10;

/* dimension of inner work area (without border) */
const Coord N = 1000;

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

	Coord getEdgeLength() {return innerLength+2;}

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
	FileDumper(const std::string targetFile) {
		file.open(targetFile);
		file.precision(NumPrecision);
	}

	~FileDumper() {
		file.close();
	}

	void dumpBackbuffer(Workspace& w, const NumType t) {
		for(size_t i = 0; i < w.getEdgeLength(); i++) {
			for(size_t j = 0; j < w.getEdgeLength(); j++) {
				file << i << " " << j << " " << t << " " << w.elb(i,j) << std::endl;
			}
		}
	}

private:
	std::ofstream file;
};

/*
 * Must be defined on (0.0, 1.0)x(0.0, 1.0) surface
 */
NumType f(NumType x, NumType y) {
	return sin(M_PI*x)*sin(M_PI*y);
}

#define I(IDX) IDX+1

int main() {
	std::cout << "Sequential variant" << std::endl;

	Workspace w(N);
	FileDumper d("./results");

	/* fill in boundary condition */
	w.zeroBuffers(0.0);
	for(size_t x = 0; x < N; x++) {
		for(size_t y = 0; y < N; y++) {
			w.elf(I(x),I(y)) = f(x,y);
		}
	}

	w.swap();

	d.dumpBackbuffer(w, 0.0);

	return 0;
}