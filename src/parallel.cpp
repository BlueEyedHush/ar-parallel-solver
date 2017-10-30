
#include <mpi.h>
#include <exception>
#include <iostream>
#include <cmath>
#include <cstring>
#include "shared.h"

/**
 * ToDo
 * - log prefixing not needed!
 */

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
	Workspace(const Coord innerSize, const NumType borderCond, ClusterManager& cm, Comms& comm)
			: innerLength(innerSize), actualSize(innerSize*innerSize), cm(cm), borderCond(borderCond), comm(comm)
	{
		neigh = cm.getNeighbours();
		fillBuffers();
	}

	~Workspace() {
		freeBuffers();
	}

	void set_elf(const Coord x, const Coord y, const NumType value) {
		// copying to send buffers occurs during comms phase
		front[x*innerLength+y] = value;
	}

	NumType elb(const Coord x, const Coord y) {
		if(x == -1) {
			if(y == -1) {
				// conrner - invalid query, we never ask about it
				throw std::runtime_error("corner access!");
			} else if (y == innerLength) {
				// corner - invalid query
				throw std::runtime_error("corner access!");
			} else {
				// left outer border
				if(neigh[LEFT] != N_INVALID) {
					return outerEdge[LEFT][y];
				} else {
					return borderCond;
				}
			}	
		} else if (x == innerLength) {
			if(y == -1) {
				// conrner - invalid query, we never ask about it
				throw std::runtime_error("corner access!");
			} else if (y == innerLength) {
				// corner - invalid query
				throw std::runtime_error("corner access!");
			} else {
				// right outer border
				if(neigh[RIGHT] != N_INVALID) {
					return outerEdge[RIGHT][y];
				} else {
					return borderCond;
				}
			}
		} else {
			if(y == -1) {
				if(neigh[BOTTOM] != N_INVALID) {
					return outerEdge[BOTTOM][x];
				} else {
					return borderCond;
				}
			} else if (y == innerLength) {
				if(neigh[TOP] != N_INVALID) {
					return outerEdge[TOP][x];
				} else {
					return borderCond;
				}
			} else {
				// coords within main area
				return back[x*innerLength+y];
			}
		}
	}

	Coord getInnerLength() {return innerLength;}

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
		}

		swapBuffers();
	}

private:
	ClusterManager& cm;
	Comms& comm;
	int* neigh;

	const Coord innerLength;
	const Coord actualSize;

	const NumType borderCond;

	/* horizontal could be stored with main buffer, but for convenience both horizontals and
	 * verticals are allocated separatelly (and writes mirrored) */
	NumType* innerEdge[4];
	/* all outer edges are allocated separatelly; their length is innerLength, not innerLength + 2 */
	NumType* outerEdge[4];
	NumType *front;
	NumType *back;

	void fillBuffers() {
		front = new NumType[actualSize];
		back = new NumType[actualSize];

		for(int i = 0; i < 4; i++) {
			if(neigh[i] != N_INVALID) {
				innerEdge[i] = new NumType[innerLength];
				outerEdge[i] = new NumType[innerLength];
			} else {
				innerEdge[i] = nullptr;
				outerEdge[i] = nullptr;
			}
		}

		// flipping buffers requires to flip poitners to inner/outer ones (if part is shared -> don't share?)
		// outer buffers are only associated with back buffer, but inner are mainly associated with both
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
		return base + innerLength*x + y;
	}

	void swapBuffers() {
		NumType* tmp = front;
		front = back;
		back = tmp;
	}

	void copyInnerEdgesToBuffers() {
		#define LOOP(EDGE, X, Y, BUFF) \
		if(neigh[EDGE] != N_INVALID) { \
			for(Coord i = 0; i < innerLength; i++) { \
				innerEdge[EDGE][i] = *elAddress(X,Y,BUFF); \
			} \
		}

		LOOP(TOP, i, innerLength-1, front)
		LOOP(BOTTOM, i, 0, front)
		LOOP(LEFT, 0, i, front)
		LOOP(RIGHT, innerLength-1, i, front)

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
	Workspace w(n_slice, 0.0, cm, comm);

	FileDumper<Workspace> d(filenameGenerator(cm.getNodeId()),
	                        n_slice,
	                        x_offset,
	                        y_offset,
	                        h,
	                        get_freq_sel(conf.timeSteps));

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

		if (unlikely(conf.outputEnabled)) {
			d.dumpBackbuffer(w, ts);
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
