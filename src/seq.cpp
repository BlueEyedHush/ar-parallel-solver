
#include <time.h>
#include <cstddef>
#include <string>
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


int main(int argc, char **argv) {
	auto conf = parse_cli(argc, argv);

	Timer timer;
	Workspace w(conf.N);
	FileDumper<Workspace> d("./results/t", conf.N, 0.0, 0.0, 1.0/conf.N, 1);

	#define I(IDX) IDX+1
	#define V(IDX) (IDX+1)*1.0/conf.N

	/* calculate helper values */
	const TimeStepCount dumpEvery = conf.timeSteps/DUMP_TEMPORAL_FREQUENCY;

	timer.start();
	/* fill in boundary condition */
	for(Coord x_idx = 0; x_idx < conf.N; x_idx++) {
		for(Coord y_idx = 0; y_idx < conf.N; y_idx++) {
	 		auto x_i = I(x_idx);
			auto y_i = I(y_idx);
			auto x = V(x_idx);
			auto y = V(y_idx);
			auto val = f(x,y);
			w.elf(x_i,y_i) = val;

			#ifdef DEBUG
			std::cerr << "[" << x_i-1 << "," << y_i-1 <<"] "
			          << "(" << x << "," << y << ") -> "
			          << val << std::endl;
			#endif
		}
	}

	w.swap();


	for(TimeStepCount step = 0; step < conf.timeSteps; step++) {
		for(Coord x_idx = 0; x_idx < conf.N; x_idx++) {
			for(Coord y_idx = 0; y_idx < conf.N; y_idx++) {
				w.elf(I(x_idx), I(y_idx)) = equation(
					 w.elb(I(x_idx-1), I(y_idx)),
					 w.elb(I(x_idx), I(y_idx-1)),
					 w.elb(I(x_idx+1), I(y_idx)),
					 w.elb(I(x_idx), I(y_idx+1))
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