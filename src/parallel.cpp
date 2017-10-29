
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


int main() {
	ClusterManager clusterManager;

	std::cout << "parallel algorithm" << std::endl;

	return 0;
}
