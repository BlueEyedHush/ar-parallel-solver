
#include <time.h>
#include <cstddef>
#include <string>
#include "shared.h"

/**
 * Work area is indexed from 0 to size-1
 * Around that we have border area, which can be accessed using -1 and size indices
 *
 * When plotting results, border counts as 0, but plotter doesn't plot border values
 * (easier to implement that way, same plotter can be used for seq and parallel)
 */
class Workspace {
public:
	Workspace(const Coord size) :
			innerLength(size),
			outerLength(size+2),
			actualSize(outerLength*outerLength),
			zeroOffset(1)
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
		return front[coords(x,y)];
	}

	/**
	 * @return reference to (i,j)th back buffer element
	 */
	inline NumType& elb(const Coord x, const Coord y) {
		return back[coords(x,y)];
	}

	void zeroBuffers(const NumType b) {
		for(Coord i = 0; i < actualSize; i++) {
			front[i] = b;
			back[i] = b;
		}
	}

	Coord getInnerLength() {return innerLength;}

	void swap() {
		NumType* tmp = front;
		front = back;
		back = tmp;
	}

private:
	const Coord zeroOffset;
	const Coord innerLength;
	const Coord outerLength;
	const Coord actualSize;
	NumType *front;
	NumType *back;

	inline Coord coords(const Coord x, const Coord y) {
		return (zeroOffset+outerLength)*x+(y+zeroOffset);
	}
};


int main(int argc, char **argv) {
	auto conf = parse_cli(argc, argv);

	Partitioner p(1, 0.0, 1.0, conf.N);

	/* calculate helper values */
	const TimeStepCount dumpEvery = conf.timeSteps/DUMP_TEMPORAL_FREQUENCY;
	const NumType h = p.get_h();
	const Coord n = p.partition_inner_size();

	Timer timer;
	Workspace w(conf.N);
	NumType x_off, y_off;
	std::tie(x_off, y_off) = p.get_math_offset(0,0);
	FileDumper<Workspace> d("./results/t", n, x_off, y_off, h);

	timer.start();
	/* fill in boundary condition */
	for(Coord x_idx = 0; x_idx < n; x_idx++) {
		for(Coord y_idx = 0; y_idx < n; y_idx++) {
			auto x = x_idx*h;
			auto y = y_idx*h;
			auto val = f(x,y);
			w.elf(x_idx, y_idx) = val;

			#ifdef DEBUG
			std::cerr << "[" << x_idx << "," << y_idx <<"] "
			          << "(" << x << "," << y << ") -> "
			          << val << std::endl;
			#endif
		}
	}

	w.swap();

	for(TimeStepCount step = 0; step < conf.timeSteps; step++) {
		for(Coord x_idx = 0; x_idx < n; x_idx++) {
			for(Coord y_idx = 0; y_idx < n; y_idx++) {
				w.elf(x_idx, y_idx) = equation(
					 w.elb(x_idx-1 ,y_idx),
					 w.elb(x_idx ,y_idx-1),
					 w.elb(x_idx+1 ,y_idx),
					 w.elb(x_idx ,y_idx+1)
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