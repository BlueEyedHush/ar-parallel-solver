
#include <mpi.h>
#include <exception>
#include <iostream>
#include <cmath>
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
	ClusterManager() {
		MPI_Init(nullptr, nullptr);
		MPI_Comm_rank(comm, &nodeId);
		MPI_Comm_size(comm, &nodeCount);

		auto sqr = static_cast<long>(std::sqrt(nodeCount));
		if(sqr*sqr != nodeCount) {
			if(nodeId == 0) {
				err_log() << "Number of nodes must be power of some integer (got " << nodeCount << " )" << std::endl;
			}

			throw std::runtime_error("incorrect node count!");
		} else {
			sideLen = sqr;
		}

		row = nodeId/sideLen;
		column = nodeId%sideLen;

		initNeighbours();

		err_log() << "Cluster initialized successfully. I'm (" << row << "," << column << ")" << std::endl;
	}

	int getNodeCount() {
		return nodeCount;
	}

	int getNodeId() {
		return nodeId;
	}

	std::ostream& err_log() {
		std::cerr << "[" << nodeId << "] ";
		return std::cerr;
	}

	int* getNeighbours() {
		return &neighbours[0];
	}


	MPI_Comm getComm() {
		return comm;
	}

	~ClusterManager() {
		MPI_Finalize();
	}

private:
	const static auto comm = MPI_COMM_WORLD;

	int nodeCount;
	int sideLen;
	int nodeId;

	int row;
	int column;
	int neighbours[4];

	void initNeighbours() {
		if(row == 0) { neighbours[Neighbour::TOP] = N_INVALID; }
		else { neighbours[Neighbour::TOP] = nodeId-sideLen; }

		if(row == sideLen-1) { neighbours[Neighbour::BOTTOM] = N_INVALID; }
		else { neighbours[Neighbour::BOTTOM] = nodeId+sideLen; }

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

/**
 * Work area is indexed from 1 (first element) to size (last element)
 */
class Workspace {
public:
	Workspace(const Coord size, const NumType borderCond, ClusterManager& cm)
			: innerLength(size), actualSize(size*size), cm(cm), borderCond(borderCond)
	{
		neigh = cm.getNeighbours();
		allocateBuffers();
	}

	~Workspace() {
		freeBuffers();
	}

	void set_elf(const Coord x, const Coord y, const NumType value) {
		// front[outerLength*x+y];
	}

	NumType get_elb(const Coord x, const Coord y) {
		return 0.0;
	}

	Coord getLength() {return innerLength;}

	void swap() {
		NumType* tmp = front;
		front = back;
		back = tmp;
	}

private:
	ClusterManager& cm;
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

	void allocateBuffers() {
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

};


int main() {
	ClusterManager clusterManager;

	std::cout << "parallel algorithm" << std::endl;

	return 0;
}
