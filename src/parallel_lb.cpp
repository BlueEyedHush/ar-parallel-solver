
#include <mpi.h>
#include <exception>
#include <iostream>
#include <cmath>
#include <cstring>
#include "shared.h"

const int N_INVALID = -1;

enum Neighbour {
	LEFT = 0,
	TOP = 1,
	RIGHT = 2,
	BOTTOM = 3,
};

class ClusterManager {
public:
	ClusterManager(const Coord N) : bitBucket(0) {
		MPI_Init(nullptr, nullptr);
		MPI_Comm_rank(comm, &nodeId);
		MPI_Comm_size(comm, &nodeCount);

		partitioner = new Partitioner(nodeCount, 0.0, 1.0, N);
		sideLen = partitioner->get_nodes_grid_dimm();
		std::tie(row, column) = partitioner->node_id_to_grid_pos(nodeId);

		initNeighbours();

		err_log() << "Cluster initialized successfully. I'm (" << row << "," << column << ")" << std::endl;
	}

	~ClusterManager() {
		delete partitioner;
		MPI_Finalize();
	}

	Partitioner& getPartitioner() {return *partitioner;}

	int getNodeCount() { return nodeCount; }
	int getNodeId() { return nodeId; }
	std::pair<NumType, NumType> getOffsets() { return partitioner->get_math_offset_node(row, column); };
	MPI_Comm getComm() { return comm; }

	std::ostream& err_log() {
		std::cerr << "[" << nodeId << "] ";
		return std::cerr;
	}

	std::ostream& master_err_log() {
		if(nodeId == 0) {
			std::cerr << "[" << nodeId << "] ";
			return std::cerr;
		} else {
			return bitBucket;
		}
	}

	int* getNeighbours() {
		return &neighbours[0];
	}


private:
	const static auto comm = MPI_COMM_WORLD;

	int nodeId;
	int nodeCount;
	int row;
	int column;

	Partitioner *partitioner;

	int sideLen;
	int neighbours[4];

	std::ostream bitBucket;

	void initNeighbours() {
		if(row == 0) { neighbours[Neighbour::BOTTOM] = N_INVALID; }
		else { neighbours[Neighbour::BOTTOM] = nodeId-sideLen; }

		if(row == sideLen-1) { neighbours[Neighbour::TOP] = N_INVALID; }
		else { neighbours[Neighbour::TOP] = nodeId+sideLen; }

		if(column == 0) { neighbours[Neighbour::LEFT] = N_INVALID; }
		else { neighbours[Neighbour::LEFT] = nodeId-1; }

		if(column == sideLen-1) { neighbours[Neighbour::RIGHT] = N_INVALID; }
		else { neighbours[Neighbour::RIGHT] = nodeId+1; }

		err_log() << "Neighbours: "
		          << " LEFT: " << neighbours[LEFT]
		          << " TOP: " << neighbours[TOP]
		          << " RIGHT: " << neighbours[RIGHT]
		          << " BOTTOM: " << neighbours[BOTTOM] << std::endl;
	}
};

class Comms {
public:
	Comms(const Coord innerLength) : innerLength(innerLength) {
		reset();
	}

	void exchange(int targetId, NumType* sendBuffer, NumType* receiveBuffer) {
		MPI_Isend(sendBuffer, innerLength, NUM_MPI_DT, targetId, 1, MPI_COMM_WORLD, rq + nextId);
		MPI_Irecv(receiveBuffer, innerLength, NUM_MPI_DT, targetId, MPI_ANY_TAG, MPI_COMM_WORLD, rq + nextId + 1);

		nextId += 2;
	}

	void wait() {
		#ifdef DEBUG
		std::cerr << "NextId: " << nextId << std::endl;
		#endif
		for(int i = 0; i < nextId; i++) {
			int finished;
			MPI_Waitany(nextId, rq, &finished, MPI_STATUSES_IGNORE);
			#ifdef DEBUG
			std::cerr << "Finished " << finished << ". Already done " << i+1 << std::endl;
			#endif
		}
		#ifdef DEBUG
		std::cerr << "Wait finished" << std::endl;
		#endif
	}

	void reset() {
		for(int i = 0; i < RQ_COUNT; i++) {
			rq[i] = MPI_REQUEST_NULL;
		}
		nextId = 0;
	}

private:
	const static int RQ_COUNT = 8;
	const Coord innerLength;
	MPI_Request rq[RQ_COUNT];
	int nextId;
};


class Workspace {
public:
	Workspace(const Coord innerSize, const Coord borderWidth, ClusterManager& cm, Comms& comm)
			: innerSize(innerSize), cm(cm), comm(comm), borderWidth(borderWidth)
	{
		outerSize = innerSize+2*borderWidth;
		memorySize = outerSize*outerSize;

		neigh = cm.getNeighbours();
		initialize_buffers();
	}

	~Workspace() {
		freeBuffers();
	}

	void set_elf(const Coord x, const Coord y, const NumType value) {
		*elAddress(x, y, front) = value;
	}

	NumType elb(const Coord x, const Coord y) {
		return *elAddress(x,y,back);
	}

	Coord getInnerLength() {return innerSize;}

	void swap(bool comms = true) {
		if(comms) {
			copyInnerEdgesToBuffers();

			comm.reset();
			for(int i = 0; i < 4; i++) {
				auto iThNeigh = neigh[i];
				if(iThNeigh != N_INVALID) {
					comm.exchange(iThNeigh, innerEdge[i], outerEdge[i]);
				}
			}
			comm.wait();

			copy_outer_buffer_to(front);
		}

		swapBuffers();
	}

private:
	ClusterManager& cm;
	Comms& comm;
	int* neigh;

	const Coord innerSize;
	Coord outerSize;
	Coord memorySize;

	const Coord borderWidth;

	/* horizontal could be stored with main buffer, but for convenience both horizontals and
	 * verticals are allocated separatelly (and writes mirrored) */
	NumType* innerEdge[4];
	/* all outer edges are allocated separatelly; their length is innerLength, not innerLength + 2 */
	NumType* outerEdge[4];
	NumType *front;
	NumType *back;

	void initialize_buffers() {
		// @todo: need to zero-out front & back buffers
		front = new NumType[memorySize]();
		back = new NumType[memorySize]();

		/* create inner buffer (as comm buffers) for  */
		for(int i = 0; i < 4; i++) {
			if(neigh[i] != N_INVALID) {
				innerEdge[i] = new NumType[innerSize];
				outerEdge[i] = new NumType[innerSize];
			} else {
				innerEdge[i] = nullptr;
				outerEdge[i] = nullptr;
			}
		}
	}

	void freeBuffers() {
		delete[] front;
		delete[] back;

		for(int i = 0; i < 4; i++) {
			if(innerEdge != nullptr) {
				delete[] innerEdge[i];
				delete[] outerEdge[i];
			}
		}
	}

	NumType* elAddress(const Coord x, const Coord y, NumType* base) {
		return base + innerSize*(borderWidth + x) + (borderWidth + y);
	}

	void swapBuffers() {
		NumType* tmp = front;
		front = back;
		back = tmp;
	}

	void copyInnerEdgesToBuffers() {
		#define LOOP(EDGE, X, Y) \
		if(neigh[EDGE] != N_INVALID) { \
			for(Coord i = 0; i < innerSize; i++) { \
				innerEdge[EDGE][i] = *elAddress(X,Y,front); \
			} \
		}

		LOOP(TOP, i, innerSize-1)
		LOOP(BOTTOM, i, 0)
		LOOP(LEFT, 0, i)
		LOOP(RIGHT, innerSize-1, i)

		#undef LOOP
	}

	void copy_outer_buffer_to(NumType *target) {
		#define LOOP(EDGE, X, Y) \
		if(neigh[EDGE] != N_INVALID) { \
			for(Coord i = 0; i < innerSize; i++) { \
				*elAddress(X,Y,target) = outerEdge[EDGE][i]; \
			} \
		}

		LOOP(TOP, i, innerSize)
		LOOP(BOTTOM, i, -1)
		LOOP(LEFT, -1, i)
		LOOP(RIGHT, innerSize, i)

		#undef LOOP
	}
};

std::string filenameGenerator(int nodeId) {
	std::ostringstream oss;
	oss << "./results/" << nodeId << "_t";
	return oss.str();
}

int main(int argc, char **argv) {
	std::cerr << __FILE__ << std::endl;

	auto conf = parse_cli(argc, argv);

	ClusterManager cm(conf.N);
	auto n_slice = cm.getPartitioner().get_n_slice();
	NumType x_offset, y_offset;
	std::tie(x_offset, y_offset) = cm.getOffsets();
	auto h = cm.getPartitioner().get_h();

	Comms comm(n_slice);
	Workspace w(n_slice, 1, cm, comm);

	FileDumper<Workspace> d(filenameGenerator(cm.getNodeId()), n_slice, x_offset, y_offset, h);
	const TimeStepCount dumpEvery = conf.timeSteps/DUMP_TEMPORAL_FREQUENCY;

	Timer timer;

	MPI_Barrier(cm.getComm());
	timer.start();

	for(Coord x_idx = 0; x_idx < n_slice; x_idx++) {
		for(Coord y_idx = 0; y_idx < n_slice; y_idx++) {
			auto x = x_offset + x_idx*h;
			auto y = y_offset + y_idx*h;
			auto val = f(x,y);
			w.set_elf(x_idx,y_idx, val);

			#ifdef DEBUG
			std::cerr << "[" << x_idx << "," << y_idx <<"] "
			          << "(" << x << "," << y << ") -> "
			          << val << std::endl;
			#endif
		}
	}

	w.swap();

	for(TimeStepCount ts = 0; ts < conf.timeSteps; ts++) {
		#ifdef DEBUG
		std::cerr << "Entering timestep loop, ts = " << ts << std::endl;
		#endif

		for(Coord x_idx = 0; x_idx < n_slice; x_idx++) {
			#ifdef DEBUG
			std::cerr << "Entering X loop, x = " << x_idx << std::endl;
			#endif

			for(Coord y_idx = 0; y_idx < n_slice; y_idx++) {
				#ifdef DEBUG
				std::cerr << "Entering Y loop, x y " << y_idx << std::endl;
				#endif

				auto eq_val = equation(
						w.elb(x_idx - 1, y_idx),
						w.elb(x_idx, y_idx - 1),
						w.elb(x_idx + 1, y_idx),
						w.elb(x_idx, y_idx + 1)
				);

				w.set_elf(x_idx, y_idx, eq_val);
			}
		}

		#ifdef DEBUG
		std::cerr << "Before swap, ts = " << ts << std::endl;
		#endif

		w.swap();

		#ifdef DEBUG
		std::cerr << "Entering file dump" << std::endl;
		#endif

		if (conf.outputEnabled && ts % dumpEvery == 0) {
			d.dumpBackbuffer(w, ts/dumpEvery);
		}

		#ifdef DEBUG
		std::cerr << "After dump, ts = " << ts << std::endl;
		#endif
	}

	MPI_Barrier(cm.getComm());
	auto duration = timer.stop();

	if(cm.getNodeId() == 0) {
		std::cout << duration << std::endl;
		std::cerr << ((double)duration)/1000000000 << " s" << std::endl;
	}

	#ifdef DEBUG
	std::cerr << "Terminating" << std::endl;
	#endif

	return 0;
}
