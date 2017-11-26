
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

class ClusterManager : private NonCopyable {
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
		return std::cerr;
	}

	std::ostream& master_err_log() {
		if(nodeId == 0) {
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

/*
 * Buffer exposal during async
 *  I - start of innies calculation
 *  O - start of outies calulations
 *  s - swap, calculations finished for given iteration
 *  out_r - recv, period of outer buffers exposal to the network
 *  out_s - send, period of inner buffers exposal to the network
 *
 *  I        O    s  I       O  s
 *  - out_r -|    |-- out_r -|
 *  - out_s -|    |-- out_s -|
 *
 * receive (outer) - needed when calculating border values
 *	* must be present when i-1 outies calculated
 *	* can lie idle during subsequent outies calculation (assuming no memcpy)
 * send (inner)
 *	* can be sent only when values calculated (happens right after outer become available)
 *	* can be exposed only until outies from next iteration need to be calculated
 *
 * Memcpy impact?
 * Separate inner buffer: we don't have to wait with i+1 outies calculation until buffers are free (otherwise
 * we could overwrite data being sent)
 * Separate outer buffer: data required to carry out computations, but we can have a couple of spares with
 * outstanding receive request attached
 *
 * Single memcpied send buffer:
 * Allow to extend buffer exposure into outies calculation phase
 *
 *  I        O    s  I       O   s
 *  - out_r -|    |-- out_r -|
 *  --out_m--|xxxx| memcpy-> out_s1
 *  - out_s1 -----| |------------|
 *
 */

class Comms : private NonCopyable {
public:
	Comms(const Coord innerLength) : innerLength(innerLength) {
		reset_rqb(send_rqb, false);
		reset_rqb(recv_rqb, false);
	}

	~Comms() {
		// cancel outstanding receives
		reset_rqb(recv_rqb, true);
	}

	void wait_for_send() {
		wait_for_rqb(send_rqb);
	}

	void wait_for_receives() {
		wait_for_rqb(recv_rqb);
	}

	#define SCHEDULE_OP(OP, RQB) \
		auto idx = RQB.second; \
		auto* rq = RQB.first + idx; \
		OP(buffer, innerLength, NUM_MPI_DT, nodeId, 1, MPI_COMM_WORLD, rq); \
		RQB.second++;
	
	void schedule_send(int nodeId, NumType* buffer) {
		//DL( "schedule send to " << nodeId )
		SCHEDULE_OP(MPI_Isend, send_rqb)
		//DL( "rqb afterwards" << send_rqb.second )
	}

	void schedule_recv(int nodeId, NumType* buffer) {
		//DL( "schedule receive from " << nodeId )
		SCHEDULE_OP(MPI_Irecv, recv_rqb)
		//DL( "rqb afterwards" << recv_rqb.second )
	}

	#undef SCHEDULE_OP

private:
	const static int RQ_COUNT = 4;
	using RqBuffer = std::pair<MPI_Request[RQ_COUNT], int>; 
	
	const Coord innerLength;
	
	RqBuffer send_rqb;
	RqBuffer recv_rqb;

	void reset_rqb(RqBuffer& b, bool pendingWarn) {
		for(int i = 0; i < RQ_COUNT; i++) {
			if(b.first[i] != MPI_REQUEST_NULL) {
				/* commenting out because caused error:
				 * Fatal error in PMPI_Cancel: Invalid MPI_Request, error stack:
				 * PMPI_Cancel(201): MPI_Cancel(request=0x7ffc407347c8) failed
				 * PMPI_Cancel(177): Null Request pointer
				 */
				// MPI_Cancel(b.first + i);
				b.first[i] = MPI_REQUEST_NULL;

				if(pendingWarn) {
					std::cerr << "WARN: pending request left in the queue, cancelling it!" << std::endl;
				}
			}
		}
		b.second = 0;
	}
	
	void wait_for_rqb(RqBuffer& b) {
		//DL( "waiting for rqb" )
		for(int i = 0; i < b.second;  i++) {
			//DL( "iteration: " << i )
			int finished_idx;
			MPI_Waitany(b.second, b.first, &finished_idx, MPI_STATUSES_IGNORE);
		}

		//DL( "finished waiting for rqb!" )
		reset_rqb(b, true);
		//DL( "finished resettng rqb" );
	}
};


struct CSet  {
	CSet(const Coord x = 0, const Coord y = 0) : x(x), y(y) {}

	Coord x;
	Coord y;

	bool operator==(const CSet &o) const {
		return x == o.x && y == o.y;
	}

	bool operator!=(const CSet &o) const {
		return !operator==(o);
	}

	const std::string toStr() {
		std::ostringstream oss;
		oss << "(" << x << "," << y << ")";
		return oss.str();
	}
};

struct AreaCoords {
	AreaCoords() {}
	AreaCoords(const CSet bottomLeft, const CSet upperRight) : bottomLeft(bottomLeft), upperRight(upperRight) {}

	CSet bottomLeft;
	CSet upperRight;

	const std::string toStr() {
		std::ostringstream oss;
		oss << "[ " << bottomLeft.toStr() << " | " << upperRight.toStr() << "]";
		return oss.str();
	}
};

/**
 * Return inclusive ranges !!!
 */
class WorkspaceMetainfo : private NonCopyable {
public:
	WorkspaceMetainfo(const Coord innerSize, const Coord boundaryWidth) {
		precalculate(innerSize, boundaryWidth);
	}

	const AreaCoords& working_workspace_area() const { return wwa; }

	const AreaCoords& innies_space_area() const { return isa; };

	/**
	 * That's how shared areas are divided:
	 *  ___________
	 * | |_______| |
	 * | |       | |
	 * | |       | |
	 * | |_______| |
	 * |_|_______|_|
	 */
	const std::array<AreaCoords, 4>& shared_areas() const { return sha; }
	
private:
	AreaCoords wwa;
	AreaCoords isa;
	std::array<AreaCoords, 4> sha;
	
	void precalculate(const Coord innerSize, const Coord boundaryWidth) {
		const auto lid = innerSize-1;
		
		wwa.bottomLeft.x = 0;
		wwa.bottomLeft.y = 0;
		wwa.upperRight.x = lid;
		wwa.upperRight.y = lid;

		isa.bottomLeft.x = boundaryWidth;
		isa.bottomLeft.y = boundaryWidth;
		isa.upperRight.x = lid - boundaryWidth;
		isa.upperRight.y = lid - boundaryWidth;
		
		sha = {
			AreaCoords(CSet(0, 0), CSet(boundaryWidth-1, lid)), // left
			AreaCoords(CSet(innerSize - boundaryWidth, 0), CSet(lid, lid)), // right
			AreaCoords(CSet(boundaryWidth, innerSize-boundaryWidth), CSet(lid-boundaryWidth, lid)), // top
			AreaCoords(CSet(boundaryWidth, 0), CSet(lid-boundaryWidth, boundaryWidth-1)), // bottom
		};
	}
};

void test_wmi() {
	WorkspaceMetainfo wmi(9, 2);

	auto work_area = wmi.working_workspace_area();
	auto innie = wmi.innies_space_area();
	auto in_bound = wmi.shared_areas();

	#define STR(X) std::cerr << X.toStr() << std::endl;

	assert(work_area.bottomLeft == CSet(0,0));
	assert(work_area.upperRight == CSet(8,8));


	assert(innie.bottomLeft == CSet(2,2));
	assert(innie.upperRight == CSet(6,6));

	// left
	assert(in_bound[0].bottomLeft == CSet(0,0));
	assert(in_bound[0].upperRight == CSet(1,8));
	// right
	assert(in_bound[1].bottomLeft == CSet(7,0));
	assert(in_bound[1].upperRight == CSet(8,8));
	// top
	assert(in_bound[2].bottomLeft == CSet(2,7));
	assert(in_bound[2].upperRight == CSet(6,8));
	// bottom
	assert(in_bound[3].bottomLeft == CSet(2,0));
	assert(in_bound[3].upperRight == CSet(6,1));

	#undef STR
}

void iterate_over_area(AreaCoords area, std::function<void(const Coord, const Coord)> f) {
	for(Coord x_idx = area.bottomLeft.x; x_idx <= area.upperRight.x; x_idx++) {
		for(Coord y_idx = area.bottomLeft.y; y_idx <= area.upperRight.y; y_idx++) {
			f(x_idx, y_idx);
		}
	}
}

class Workspace : private NonCopyable {
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

	/*
	 * All 4 functions are called before swap() is invoked!
	 * 2 first before outie calculations, last two after them
	 */

	void ensure_out_boundary_arrived() {
		comm.wait_for_receives();
		copy_outer_buffer_to(back);
	}

	void ensure_in_boundary_sent() {
		comm.wait_for_send();
	}

	void send_in_boundary() {
		copy_from_x_to_inner_buffer(front);

		for(int i = 0; i < 4; i++) {
			if(neigh[i] != N_INVALID) {
				comm.schedule_send(neigh[i], innerEdge[i]);
			}
		}
	}

	void start_wait_for_new_out_border() {
		for(int i = 0; i < 4; i++) {
			if(neigh[i] != N_INVALID) {
				comm.schedule_recv(neigh[i], outerEdge[i]);
			}
		}
	}

	void swap() {
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
		front = new NumType[memorySize];
		back = new NumType[memorySize];

		for(Coord i = 0; i < memorySize; i++) {
			front[i] = 0.0;
			back[i] = 0.0;
		}

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
		return base + outerSize*(borderWidth + x) + (borderWidth + y);
	}

	void swapBuffers() {
		NumType* tmp = front;
		front = back;
		back = tmp;
	}

	void copy_from_x_to_inner_buffer(NumType *x) {
		#define LOOP(EDGE, X, Y) \
		if(neigh[EDGE] != N_INVALID) { \
			for(Coord i = 0; i < innerSize; i++) { \
				innerEdge[EDGE][i] = *elAddress(X,Y,x); \
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

const Coord BOUNDARY_WIDTH = 1;

int main(int argc, char **argv) {
	std::cerr << __FILE__ << std::endl;

	auto conf = parse_cli(argc, argv);

	ClusterManager cm(conf.N);
	auto n_slice = cm.getPartitioner().get_n_slice();
	NumType x_offset, y_offset;
	std::tie(x_offset, y_offset) = cm.getOffsets();
	auto h = cm.getPartitioner().get_h();

	Comms comm(n_slice);
	Workspace w(n_slice, BOUNDARY_WIDTH, cm, comm);
	WorkspaceMetainfo wi(n_slice, BOUNDARY_WIDTH);

	FileDumper<Workspace> d(filenameGenerator(cm.getNodeId()),
	                        n_slice,
	                        x_offset,
	                        y_offset,
	                        h,
	                        get_freq_sel(conf.timeSteps));

	Timer timer;

	MPI_Barrier(cm.getComm());
	timer.start();

	auto ww_area = wi.working_workspace_area();
	auto wi_area = wi.innies_space_area();
	auto ws_area = wi.shared_areas();

	DL( "filling boundary condition" )

	iterate_over_area(ww_area, [&w, x_offset, y_offset, h](const Coord x_idx, const Coord y_idx) {
		auto x = x_offset + x_idx*h;
		auto y = y_offset + y_idx*h;
		auto val = f(x,y);
		w.set_elf(x_idx,y_idx, val);

		/*
		std::cerr << "[" << x_idx << "," << y_idx <<"] "
			          << "(" << x << "," << y << ") -> "
			          << val << std::endl;
        */
	});

	DL( "calculated boundary condition, initial communication" )

	/* send our part of initial condition to neighbours */
	w.send_in_boundary();
	w.start_wait_for_new_out_border();
	w.swap();

	DL( "initial communication done" )

	auto eq_f = [&w](const Coord x_idx, const Coord y_idx) {
		// std::cerr << "Entering Y loop, x y " << y_idx << std::endl;

		auto eq_val = equation(
				w.elb(x_idx - 1, y_idx),
				w.elb(x_idx, y_idx - 1),
				w.elb(x_idx + 1, y_idx),
				w.elb(x_idx, y_idx + 1)
		);

		w.set_elf(x_idx, y_idx, eq_val);
	};

	for(TimeStepCount ts = 0; ts < conf.timeSteps; ts++) {
		DL( "Entering timestep loop, ts = " << ts )

		iterate_over_area(wi_area, eq_f);
		DL( "Innies iterated, ts = " << ts )

		w.ensure_out_boundary_arrived();
		DL( "Out boundary arrived, ts = " << ts )
		w.ensure_in_boundary_sent();
		DL( "In boundary sent, ts = " << ts )

		for(auto a: ws_area) {
			iterate_over_area(a, eq_f);
		}

		DL( "Outies iterated, ts = " << ts )

		w.send_in_boundary();
		DL( "In boundary send scheduled, ts = " << ts )
		w.start_wait_for_new_out_border();

		DL( "Before swap, ts = " << ts )
		w.swap();

		DL( "Entering file dump" )
		if (unlikely(conf.outputEnabled)) {
			d.dumpBackbuffer(w, ts);
		}
		DL( "After dump, ts = " << ts )
	}

	MPI_Barrier(cm.getComm());
	auto duration = timer.stop();

	if(cm.getNodeId() == 0) {
		print_result("parallel_async", cm.getNodeCount(), duration, conf);
		std::cerr << ((double)duration)/1000000000 << " s" << std::endl;
	}

	DL( "Terminating" )

	return 0;
}
