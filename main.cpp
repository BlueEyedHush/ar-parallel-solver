
#include <iostream>
#include <cstddef>
#include <cmath>

using Coord = size_t;
using NumType = double;

/* dimension of inner work area (without border) */
const Coord N = 1000;

/**
 * Work area is indexed from 1 (first element) to size (last element)
 */
class Workspace {
public:
	Workspace(const Coord size) : innerSize(size), actualSize( (innerSize+2)*(innerSize+2) ) {
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
		return front[actualSize*x+y];
	}

	/**
	 * @return reference to (i,j)th back buffer element
	 */
	inline NumType& elb(const Coord x, const Coord y) {
		return back[actualSize*x+y];
	}

	void zeroBuffers(const NumType b) {
		for(Coord i = 0; i < actualSize; i++) {
			front[i] = b;
			back[i] = b;
		}
	}

	void swap() {
		NumType* tmp = front;
		front = back;
		back = tmp;
	}

private:
	const Coord innerSize;
	const Coord actualSize;
	NumType *front;
	NumType *back;

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

	/* fill in boundary condition */
	w.zeroBuffers(0.0);
	for(size_t x = 0; x < N; x++) {
		for(size_t y = 0; y < N; y++) {
			w.elf(x,y) = f(x,y);
		}
	}

	return 0;
}