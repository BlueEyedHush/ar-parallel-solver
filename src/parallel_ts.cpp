
#include <mpi.h>
#include <exception>
#include <iostream>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <vector>
#include "shared.h"

const int N_INVALID = -1;

enum Neighbour {
	LEFT = 0,
	TOP = 1,
	RIGHT = 2,
	BOTTOM = 3,
	TL = 4,
	TR = 5,
	BL = 6,
	BR = 7,
	NEIGHBOUR_VAL_COUNT = 8,
};

class ClusterMatrix {
public:
	ClusterMatrix(const int sideLen) : outSideLen(sideLen+2) {
		backingStore = new int[outSideLen*outSideLen];
		fillMatrix(sideLen);
	}

	~ClusterMatrix() {
		delete[] backingStore;
	}

	int idAt(const int i, const int j) {
		int idx = outSideLen*(i+1) + j+1;
		return backingStore[idx];
	}

	std::string toStr() {
		std::ostringstream ostr;

		for(int i = 0; i < outSideLen; i++) {
			for(int j = 0; j < outSideLen; j++) {
				ostr << "| " << backingStore[i*outSideLen+j] << " ";
			}

			ostr << "|" << std::endl;
		}

		return ostr.str();
	}

private:
	const int outSideLen;
	int *backingStore;

	void fillMatrix(const int sideLen) {
		const int totalLen = outSideLen*outSideLen;

		/* borders first - top, bottom, left, right */
		for(int i = 0; i < outSideLen; i++) { backingStore[i] = N_INVALID; }
		for(int i = outSideLen*(outSideLen-1); i < totalLen; i++) { backingStore[i] = N_INVALID; }
		for(int i = 0; i < totalLen; i += outSideLen) { backingStore[i] = N_INVALID; }
		for(int i = outSideLen-1; i < totalLen; i += outSideLen) { backingStore[i] = N_INVALID; }

		/* rest */
		int nodeId = 0;
		for(int i = 1; i < outSideLen-1; i++) {
			for(int j = 1; j < outSideLen-1; j++) {
				backingStore[i*outSideLen+j] = nodeId;
				nodeId += 1;
			}
		}
	}
};

class ClusterManager : private NonCopyable {
public:
	ClusterManager(const Coord N) : bitBucket(0) {
		MPI_Init(nullptr, nullptr);
		MPI_Comm_rank(comm, &nodeId);
		MPI_Comm_size(comm, &nodeCount);

		partitioner = new Partitioner(nodeCount, 0.0, 1.0, N);
		const int sideLen = partitioner->get_nodes_grid_dimm();
		std::tie(row, column) = partitioner->node_id_to_grid_pos(nodeId);

		clusterMatrix = new ClusterMatrix(sideLen);
		precalculateNeighbours();

		err_log() << clusterMatrix->toStr() << std::endl << "And I'm " << nodeId;
	}

	~ClusterManager() {
		delete partitioner;
		delete clusterMatrix;
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
	const static int directionMap[][2] = {
		{-1, 0}, // LEFT = 0,
		{0, 1}, // TOP = 1,
		{1, 0}, // RIGHT = 2,
		{0, -1}, // BOTTOM = 3,
		{-1, 1}, // TL = 4,
		{1, 1}, // TR = 5,
		{-1, -1}, // BL = 6,
		{1, -1} // BR = 7,
	};

	int row;
	int column;
	int nodeId;
	int nodeCount;
	int neighbours[NEIGHBOUR_VAL_COUNT];

	ClusterMatrix *clusterMatrix;
	Partitioner *partitioner;

	std::ostream bitBucket;

	void precalculateNeighbours() {
		for(int i = 0; i < NEIGHBOUR_VAL_COUNT; i++) {
			auto ni = row + directionMap[i][0];
			auto nj = column + directionMap[i][1];
			neighbours[i] = clusterMatrix->idAt(ni, nj);
		}
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
	const static int RQ_COUNT = NEIGHBOUR_VAL_COUNT;
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
	OUT = NEIGHBOUR_VAL_COUNT,
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

		MPI_Type_vector(gap_width, inner_size, outer_size, NUM_MPI_DT, &horiz_dt);
		MPI_Type_commit(&horiz_dt);
		MPI_Type_vector(inner_size, gap_width, outer_size, NUM_MPI_DT, &vert_dt);
		MPI_Type_commit(&vert_dt);

		/* put here coordinates of the beginning; since storage is flipped horizontally, (0,0) /x,y/
		 * is stored at the beginning, then (1,0), (2,0), ... (0,1) and so on
		 */
		info[IN + LEFT] = comms_info(nm[LEFT], cm(0,0), vert_dt, 1);
		info[IN + RIGHT] = comms_info(nm[RIGHT], cm(inner_size-gap_width, 0), vert_dt, 1);
		info[IN + TOP] = comms_info(nm[TOP], cm(0,inner_size-1), horiz_dt, 1);
		info[IN + BOTTOM] = comms_info(nm[BOTTOM], cm(0,0), horiz_dt, 1);

		info[OUT + LEFT] = comms_info(nm[LEFT], cm(-1*(gap_width),0), vert_dt, 1);
		info[OUT + RIGHT] = comms_info(nm[RIGHT], cm(inner_size, 0), vert_dt, 1);
		info[OUT + TOP] = comms_info(nm[TOP], cm(0,inner_size), horiz_dt, 1);
		info[OUT + BOTTOM] = comms_info(nm[BOTTOM], cm(0,-1*(gap_width)), horiz_dt, 1);

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
		MPI_Type_free(&horiz_dt);
	}

	void schedule_send(Comms& c, Neighbour n, NumType* buffer) {
		auto& inf = info[IN + n];
		DL( "proxy_send, neighbour: " << n << ", info_target: " << inf.node_id << ", offset: "
		                              << inf.offset << ", type = " << ((inf.type == vert_dt) ? "vert_dt" : "num_type") )
		c.schedule_send(inf.node_id, buffer + inf.offset, inf.size, inf.type);
	}

	void schedule_recv(Comms& c, Neighbour n, NumType* buffer) {
		auto& inf = info[OUT + n];
		DL( "proxy_recv, neighbour: " << n << ", info_target: " << inf.node_id << ", offset: "
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
	MPI_Datatype horiz_dt;
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
	WorkspaceMetainfo(const Coord innerSize, TimeStepCount intervalLen) {
		precalculate(innerSize, intervalLen);
	}

	const std::vector<AreaCoords>& working_workspace_area() const { return wwas; }

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
	std::vector<AreaCoords> wwas;
	AreaCoords isa;
	std::array<AreaCoords, 4> sha;
	
	void precalculate(const Coord innerSize, const TimeStepCount intervalLen) {
		const auto lid = innerSize-1;
		
		for(int i = 0; i < intervalLen; i++) {
			AreaCoords wwa;
			wwa.bottomLeft.x = 0 - i;
			wwa.bottomLeft.y = 0 - i;
			wwa.upperRight.x = lid + i;
			wwa.upperRight.y = lid + i;
			wwas.push_back(wwa);
		}

		isa.bottomLeft.x = intervalLen;
		isa.bottomLeft.y = intervalLen;
		isa.upperRight.x = lid - intervalLen;
		isa.upperRight.y = lid - intervalLen;
		
		sha = {
			AreaCoords(CSet(0, 0), CSet(intervalLen-1, lid)), // left
			AreaCoords(CSet(innerSize - intervalLen, 0), CSet(lid, lid)), // right
			AreaCoords(CSet(intervalLen, innerSize-intervalLen), CSet(lid-intervalLen, lid)), // top
			AreaCoords(CSet(intervalLen, 0), CSet(lid-intervalLen, intervalLen-1)), // bottom
		};
	}
};

void test_wmi() {
	WorkspaceMetainfo wmi(9, 2);

	auto work_area = wmi.working_workspace_area();
	auto innie = wmi.innies_space_area();
	auto in_bound = wmi.shared_areas();

	#define STR(X) std::cerr << X.toStr() << std::endl;

	assert(work_area[0].bottomLeft == CSet(0,0));
	assert(work_area[0].upperRight == CSet(8,8));


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
				comm_proxy->schedule_send(comm, static_cast<Neighbour>(i), back);
			}
		}
	}

	void start_wait_for_new_out_border() {
		for(int i = 0; i < 4; i++) {
			if(neigh[i] != N_INVALID) {
				comm_proxy->schedule_recv(comm, static_cast<Neighbour>(i), back);
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

const Coord TIME_INTERVAL = 2;

int main(int argc, char **argv) {
	std::cerr << __FILE__ << std::endl;

	auto conf = parse_cli(argc, argv);

	ClusterManager cm(conf.N);
	auto n_slice = cm.getPartitioner().get_n_slice();
	NumType x_offset, y_offset;
	std::tie(x_offset, y_offset) = cm.getOffsets();
	auto h = cm.getPartitioner().get_h();

	Comms comm;
	Workspace w(n_slice, TIME_INTERVAL, cm, comm);
	WorkspaceMetainfo wi(n_slice, TIME_INTERVAL);

	FileDumper<Workspace> d(filenameGenerator(cm.getNodeId()),
	                        n_slice,
	                        x_offset,
	                        y_offset,
	                        h,
	                        sel_first_k_policy(10));

	Timer timer;

	MPI_Barrier(cm.getComm());
	timer.start();

	auto ww_areas = wi.working_workspace_area();
	auto wi_area = wi.innies_space_area();
	auto ws_area = wi.shared_areas();

	for(auto a: ww_areas) {
		std::cerr << "Workspace area:" << a.toStr() << std::endl;
	}

	DL( "filling boundary condition" )

	iterate_over_area(ww_areas[0], [&w, x_offset, y_offset, h](const Coord x_idx, const Coord y_idx) {
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

	DL( "initial swap" )
	w.swap();

	DL( "calculated boundary condition, initial communication" )
	/* send our part of initial condition to neighbours */
	w.send_in_boundary();
	w.start_wait_for_new_out_border();
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

		DL( "Entering file dump" )
		if (unlikely(conf.outputEnabled)) {
			d.dumpBackbuffer(w, ts*TIME_INTERVAL);
		}

		/* after finished iteration, calultions you just made must end up in back-buffer -> you need to swap */
		DL( "Before swap, ts = " << ts << " t = 0")
		w.swap();
		DL( "After swap, ts = " << ts << " t = 0" )

		/* no we start calculation using cached data */
		for(int i = 1; i < TIME_INTERVAL; i++) {
			iterate_over_area(ww_areas[i], eq_f);

			DL( "front dump - timeshift calculations for t = " << i )
			DBG_ONLY( w.memory_dump(true) )
			DL ("back dump - timsehift calculations for t = " << i )
			DBG_ONLY( w.memory_dump(false) )

			DL( "Entering file dump" )
			if (unlikely(conf.outputEnabled)) {
				d.dumpBackbuffer(w, ts*TIME_INTERVAL + i);
			}

			DL( "Before swap, ts = " << ts << " t = " << i )
			w.swap();
			DL( "After swap, ts = " << ts << " t = " << i )
		}


		w.send_in_boundary();
		DL( "In boundary send scheduled, ts = " << ts )
		w.start_wait_for_new_out_border();
		DL( "Initiated receive requests for new boundary, ts = " << ts )
	}

	MPI_Barrier(cm.getComm());
	auto duration = timer.stop();

	if(cm.getNodeId() == 0) {
		std::cout << duration << std::endl;
		std::cerr << ((double)duration)/1000000000 << " s" << std::endl;
	}

	DL( "Terminating" )

	return 0;
}

/*
 * Changes
 * - another loop in main, within time range
 * - communication structure stays identicall, but we perform it at the beginning and end of the whole range
 * - also we have wider border that must be communicates
 * - area size is going to depent on the time within range (basically we must add t_r) -> it must be configured
 * - what about border nodes? we probably reserve more memory and fill it with 0s
 *
 * ToDo
 * - +modify workspacemetainfo - it returns vector of areas over which we iterate
 *   - data inside already precalculated - no need to cache it in main
 * - +additional loop inside main (in the middle of that loop we only cclaculate innies!)
 *   - +it seems it requires to use backbuffer for communication, not front buffer
 *   - dump front, not back buffer
 * - +ennsure Workspace exposes whole area (no need to do it otherwise, is it?)
 * - +review mannger in which number of steps is configured
 *   - interval size configurable from cli
 *   - +frames reneding frequency now calculated incorrectly
 *   - +fix metric - frequency seems unintuitive
 * - communication with corner nodes
 *   - rewrite ClusterInfo
 *     - do we still need partitioner? maybe merge them/deduplicate responsibilities?
 *   - rewrite NeighboursCommProxy
 *   - send/receive parts of Workspace - we have to initiate communication to more nodes
 */
