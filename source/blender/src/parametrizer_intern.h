
#ifndef __PARAMETRIZER_INTERN_H__
#define __PARAMETRIZER_INTERN_H__

/* Hash:
   -----
   - insert only
   - elements are all stored in a flat linked list
*/

typedef long PHashKey;

typedef struct PHashLink {
	struct PHashLink *next;
	PHashKey key;
} PHashLink;

typedef struct PHash {
	PHashLink *first;
	PHashLink **buckets;
	int size, cursize, cursize_id;
} PHash;

PHash *phash_new(int sizehint);
void phash_delete_with_links(PHash *ph);
void phash_delete(PHash *ph);

int phash_size(PHash *ph);

void phash_insert(PHash *ph, PHashLink *link);
PHashLink *phash_lookup(PHash *ph, PHashKey key);
PHashLink *phash_next(PHash *ph, PHashKey key, PHashLink *link);

#if 0
	#define param_assert(condition)
	#define param_warning(message);
#else
	#define param_assert(condition) \
		if (!(condition)) \
			{ printf("Assertion %s:%d\n", __FILE__, __LINE__); abort(); }
	#define param_warning(message) \
		{ printf("Warning %s:%d: %s\n", __FILE__, __LINE__, message);
#endif

typedef enum PBool {
	P_TRUE = 1,
	P_FALSE = 0
} PBool;

struct PVert;
struct PEdge;
struct PFace;
struct PChart;
struct PHandle;

/* Heap */

typedef struct PHeapLink {
	void *ptr;
	float value;
	int index;
} PHeapLink;

typedef struct PHeap {
	unsigned int size;
	unsigned int bufsize;
	PHeapLink *links;
	PHeapLink **tree;
} PHeap;

/* Simplices */

typedef struct PVert {
	struct PVertLink {
		struct PVert *next;
		PHashKey key;
	} link;

	struct PEdge *edge;
	float *co;
	float uv[2];
	int flag;

	union PVertUnion {
		int index; /* lscm matrix index */
		float distortion; /* area smoothing */
	} u;
} PVert; 

typedef struct PEdge {
	struct PEdgeLink {
		struct PEdge *next;
		PHashKey key;
	} link;

	struct PVert *vert;
	struct PEdge *pair;
	struct PEdge *next;
	struct PFace *face;
	float *orig_uv, old_uv[2];
	int flag;

	union PEdgeUnion {
		PHeapLink *heaplink;
	} u;
} PEdge;

typedef struct PFace {
	struct PFaceLink {
		struct PFace *next;
		PHashKey key;
	} link;

	struct PEdge *edge;
	int flag;

	union PFaceUnion {
		int chart; /* chart construction */
		float area3d; /* stretch */
	} u;
} PFace;

enum PVertFlag {
	PVERT_PIN = 1,
	PVERT_SELECT = 2
};

enum PEdgeFlag {
	PEDGE_SEAM = 1,
	PEDGE_VERTEX_SPLIT = 2,
	PEDGE_PIN = 4,
	PEDGE_SELECT = 8,
	PEDGE_DONE = 16,
	PEDGE_FILLED = 32
};

/* for flipping faces */
#define PEDGE_VERTEX_FLAGS (PEDGE_PIN)

enum PFaceFlag {
	PFACE_CONNECTED = 1,
	PFACE_FILLED = 2
};

/* Chart */

typedef struct PChart {
	PHash *verts;
	PHash *edges;
	PHash *faces;

	union PChartUnion {
		struct PChartLscm {
			NLContext context;
			float *abf_alpha;
			PVert *singlepin;
			PVert *pin1, *pin2;
		} lscm;
		struct PChartPack {
			float rescale;
			float size[2], trans[2];
		} pack;
	} u;

	struct PHandle *handle;
} PChart;

enum PHandleState {
	PHANDLE_STATE_ALLOCATED,
	PHANDLE_STATE_CONSTRUCTED,
	PHANDLE_STATE_LSCM,
	PHANDLE_STATE_STRETCH,
};

typedef struct PHandle {
	PChart *construction_chart;
	PChart **charts;
	int ncharts;
	enum PHandleState state;
	MemArena *arena;
	PBool implicit;
	RNG *rng;
} PHandle;

#endif /*__PARAMETRIZER_INTERN_H__*/

