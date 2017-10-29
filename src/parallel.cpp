
#include <mpi.h>
#include <exception>
#include <iostream>
#include <cmath>
#include "shared.h"

class ClusterManager {
public:
	ClusterManager() {
		MPI_Init(nullptr, nullptr);
		MPI_Comm_rank(comm, &nodeId);
		MPI_Comm_size(comm, &nodeCount);

		auto sqr = static_cast<long>(std::sqrt(nodeCount));
		if(sqr*sqr != nodeCount) {
			if(nodeId == 0) {
				std::cerr << "Number of nodes must be power of some integer (got " << nodeCount << " )" << std::endl;
			}

			throw std::runtime_error("incorrect node count!");
		} else {
			sideLen = sqr;
		}

		std::cerr << "Cluster initialized successfully" << std::endl;
	}

	int getNodeCount() {
		return nodeCount;
	}

	int getNodeId() {
		return nodeId;
	}

	MPI_Comm getComm() {
		return comm;
	}

	~ClusterManager() {
		MPI_Finalize();
	}

private:
	int nodeCount;
	int sideLen;
	int nodeId;
	const static auto comm = MPI_COMM_WORLD;
};


int main() {
	ClusterManager clusterManager;

	std::cout << "parallel algorithm" << std::endl;

	return 0;
}
