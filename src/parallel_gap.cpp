
#include <mpi.h>
#include <exception>
#include <iostream>
#include <cmath>
#include <cstring>
#include <iomanip>
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
	Comms() {
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
		OP(buffer, size, type, nodeId, 1, MPI_COMM_WORLD, rq); \
		RQB.second++;

	void schedule_send(int nodeId, NumType *buffer, Coord size, MPI_Datatype type) {
		DL( "schedule send to " << nodeId )
		SCHEDULE_OP(MPI_Isend, send_rqb)
		DL( "rqb afterwards" << send_rqb.second )
	}

	void schedule_recv(int nodeId, NumType *buffer, Coord size, MPI_Datatype type) {
		DL( "schedule receive from " << nodeId )
		SCHEDULE_OP(MPI_Irecv, recv_rqb)
		DL( "rqb afterwards" << recv_rqb.second )
	}

	#undef SCHEDULE_OP

private:
	const static int RQ_COUNT = 4;
	using RqBuffer = std::pair<MPI_Request[RQ_COUNT], int>; 
	
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


/*
 * Vertical borders (y - external, x - internal)
 *  ___________
 * |___________|
 * |y|x|___|x|y|
 * |y|x|___|x|y|
 * |y|x|___|x|y|
 * |___________|
 *
 * Horizontal borders
 *  ___________
 * |__yyyyyyy__|
 * | |xxxxxxx| |
 * | | |___| | |
 * | |xxxxxxx| |
 * |__yyyyyyy__|
 *
 * In case of internal borders we have overlap, with external we don't
 */

enum border_side {
	IN = 0,
	OUT = 4,
};

class NeighboursCommProxy {
public:
	NeighboursCommProxy(int* neigh_mapping, 
	                    const Coord innerLength, 
	                    const Coord gap_width, 
	                    std::function<Coord(const Coord, const Coord)> cm) : inner_size(innerLength)
			
	{
		const auto outer_size = inner_size + 2*gap_width;
		const auto nm = neigh_mapping;

		MPI_Type_vector(inner_size, gap_width, outer_size, NUM_MPI_DT, &vert_dt);
		MPI_Type_commit(&vert_dt);

		/* put here coordinates of the beginning; since storage is flipped horizontally, (0,0) /x,y/
		 * is stored at the beginning, then (1,0), (2,0), ... (0,1) and so on
		 */
		info[IN + LEFT] = comms_info(nm[LEFT], cm(0,0), vert_dt, 1);
		info[IN + RIGHT] = comms_info(nm[RIGHT], cm(inner_size-gap_width, 0), vert_dt, 1);
		info[IN + TOP] = comms_info(nm[TOP], cm(0,inner_size-1), NUM_MPI_DT, inner_size);
		info[IN + BOTTOM] = comms_info(nm[BOTTOM], cm(0,0), NUM_MPI_DT, inner_size);

		info[OUT + LEFT] = comms_info(nm[LEFT], cm(-1,0), vert_dt, 1);
		info[OUT + RIGHT] = comms_info(nm[RIGHT], cm(inner_size, 0), vert_dt, 1);
		info[OUT + TOP] = comms_info(nm[TOP], cm(0,inner_size), NUM_MPI_DT, inner_size);
		info[OUT + BOTTOM] = comms_info(nm[BOTTOM], cm(0,-1), NUM_MPI_DT, inner_size);

		DL( "inner_size = " << inner_size << ", gap_width = " << gap_width << ", outer_size = " << outer_size )

		#ifdef DEBUG
		for(int i = 0; i < 8; i++) {
			std::cerr << "CommsInfo: node_id = " << info[i].node_id << ", offset = " << info[i].offset << ", type = "
			                            << ((info[i].type == vert_dt) ? "vert_dt" : "num_type") << std::endl;
		}
			#endif
	}

	~NeighboursCommProxy() {
		MPI_Type_free(&vert_dt);
	}

	void schedule_send(Comms& c, Neighbour n, NumType* buffer) {
		auto& inf = info[IN + n];
		DL( "proxy_send, neighbour: " << n << ", bs: " << bs << ", info_target: " << inf.node_id << ", offset: "
		                              << inf.offset << ", type = " << ((inf.type == vert_dt) ? "vert_dt" : "num_type") )
		c.schedule_send(inf.node_id, buffer + inf.offset, inf.size, inf.type);
	}

	void schedule_recv(Comms& c, Neighbour n, NumType* buffer) {
		auto& inf = info[OUT + n];
		DL( "proxy_recv, neighbour: " << n << ", bs: " << bs << ", info_target: " << inf.node_id << ", offset: "
		                              << inf.offset << ", type = " << ((inf.type == vert_dt) ? "vert_dt" : "num_type") )
		c.schedule_recv(inf.node_id, buffer + inf.offset, inf.size, inf.type);
	}

private:
	struct comms_info {
		comms_info() {}
		comms_info(int nid, Coord offset, MPI_Datatype dt, Coord size)
				: offset(offset), type(dt), node_id(nid), size(size) {}

		Coord offset;
		MPI_Datatype type;
		int node_id;
		Coord size;
	};

	const Coord inner_size;
	comms_info info[8];

	MPI_Datatype vert_dt;
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

		comm_proxy = new NeighboursCommProxy(neigh, innerSize, borderWidth, [this](auto x, auto y) {
			return this->get_offset(x,y);
		});
	}

	~Workspace() {
		delete comm_proxy;
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
	}

	void ensure_in_boundary_sent() {
		comm.wait_for_send();
	}

	void send_in_boundary() {
		for(int i = 0; i < 4; i++) {
			if(neigh[i] != N_INVALID) {
				comm_proxy->schedule_send(comm, static_cast<Neighbour>(i), front);
			}
		}
	}

	void start_wait_for_new_out_border() {
		for(int i = 0; i < 4; i++) {
			if(neigh[i] != N_INVALID) {
				comm_proxy->schedule_recv(comm, static_cast<Neighbour>(i), front);
			}
		}
	}

	void swap() {
		swapBuffers();
	}

	void memory_dump(bool dump_front) {
		auto* buffer = dump_front ? front : back;

		for(Coord i = 0; i < outerSize; i++) {

			for(Coord j = 0; j < outerSize; j++) {
				std::cerr << std::fixed << std::setprecision(2) << buffer[i*outerSize+j] << " ";
			}

			std::cerr << std::endl;
		}
	}

private:
	ClusterManager& cm;
	Comms& comm;
	int* neigh;
	NeighboursCommProxy* comm_proxy;

	const Coord innerSize;
	Coord outerSize;
	Coord memorySize;

	const Coord borderWidth;

	NumType *front;
	NumType *back;

	void initialize_buffers() {
		front = new NumType[memorySize];
		back = new NumType[memorySize];

		for(Coord i = 0; i < memorySize; i++) {
			front[i] = 0.0;
			back[i] = 0.0;
		}
	}

	void freeBuffers() {
		delete[] front;
		delete[] back;
	}

	NumType* elAddress(const Coord x, const Coord y, NumType* base) {
		return base + get_offset(x,y);
	}

	/*
	 * Because MPI reads (and writes) directy from front/back, memory layout is no longer arbitrary
	 * I decided to store coordinate system in horizontally mirrored manner:
	 *             x
	 *  (0,0) -------------->
	 *    |
	 *    |
	 *  y |
	 *    |
	 *    |
	 *    |
	 *
	 *  x corresponds to j, y corresponds to i
	 *  stored in row major manner ( adr = i*width + j = y*width + x )
	 *
	 */
	Coord get_offset(const Coord x, const Coord y) {
		return outerSize*(borderWidth + y) + (borderWidth + x);
	}

	void swapBuffers() {
		NumType* tmp = front;
		front = back;
		back = tmp;
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

	Comms comm;
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

	DBG_ONLY( w.memory_dump(true) )

	DL( "calculated boundary condition, initial communication" )

	/* send our part of initial condition to neighbours */
	w.send_in_boundary();
	w.start_wait_for_new_out_border();

	DL( "initial swap" )
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

		DL( "front dump - before innies calculated" )
		DBG_ONLY( w.memory_dump(true) )
		DL ("back dump - before innies calculated")
		DBG_ONLY( w.memory_dump(false) )

		iterate_over_area(wi_area, eq_f);
		DL( "Innies iterated, ts = " << ts )

		w.ensure_out_boundary_arrived();
		DL( "Out boundary arrived, ts = " << ts )
		w.ensure_in_boundary_sent();
		DL( "In boundary sent, ts = " << ts )

		DL( "front dump - innies calculated" )
		DBG_ONLY( w.memory_dump(true) )
		DL ("back dump - innies calculated")
		DBG_ONLY( w.memory_dump(false) )

		for(auto a: ws_area) {
			iterate_over_area(a, eq_f);
		}

		DL( "Outies iterated, ts = " << ts )

		DL( "front dump - outies calculated" )
		DBG_ONLY( w.memory_dump(true) )
		DL ("back dump - outies calculated")
		DBG_ONLY( w.memory_dump(false) )

		w.send_in_boundary();
		DL( "In boundary send scheduled, ts = " << ts )
		w.start_wait_for_new_out_border();

		DL( "Entering file dump" )
		if (unlikely(conf.outputEnabled)) {
			d.dumpBackbuffer(w, ts);
		}

		DL( "Before swap, ts = " << ts )
		w.swap();
		DL( "After swap, ts = " << ts )
	}

	MPI_Barrier(cm.getComm());
	auto duration = timer.stop();

	if(cm.getNodeId() == 0) {
		print_result("parallel_gap", cm.getNodeCount(), duration, conf);
		std::cerr << ((double)duration)/1000000000 << " s" << std::endl;
	}

	DL( "Terminating" )

	return 0;
}
