
#ifndef __PARAMETRIZER_INTERN_H__
#define __PARAMETRIZER_INTERN_H__

/* Utils */

#if 0
	#define param_assert(condition);
	#define param_warning(message);
	#define param_test_equals_ptr(condition);
	#define param_test_equals_int(condition);
#else
	#define param_assert(condition) \
		if (!(condition)) \
			{ /*printf("Assertion %s:%d\n", __FILE__, __LINE__); abort();*/ }
	#define param_warning(message) \
		{ /*printf("Warning %s:%d: %s\n", __FILE__, __LINE__, message);*/ }
	#define param_test_equals_ptr(str, a, b) \
		if (a != b) \
			{ /*printf("Equals %s => %p != %p\n", str, a, b);*/ };
	#define param_test_equals_int(str, a, b) \
		if (a != b) \
			{ /*printf("Equals %s => %d != %d\n", str, a, b);*/ };
#endif

typedef enum PBool {
	P_TRUE = 1,
	P_FALSE = 0
} PBool;

/* Special Purpose Hash */

typedef long PHashKey;

typedef struct PHashLink {
	struct PHashLink *next;
	PHashKey key;
} PHashLink;

typedef struct PHash {
	PHashLink **list;
	PHashLink **buckets;
	int size, cursize, cursize_id;
} PHash;



struct PVert;
struct PEdge;
struct PFace;
struct PChart;
struct PHandle;

/* Simplices */

typedef struct PVert {
	struct PVert *nextlink;

	union PVertUnion {
		PHashKey key;			/* construct */
		int id;					/* abf/lscm matrix index */
		float distortion;		/* area smoothing */
		HeapNode *heaplink;		/* edge collapsing */
	} u;

	struct PEdge *edge;
	float *co;
	float uv[2];
	unsigned char flag;

} PVert; 

typedef struct PEdge {
	struct PEdge *nextlink;

	union PEdgeUnion {
		PHashKey key;					/* construct */
		int id;							/* abf matrix index */
		HeapNode *heaplink;				/* fill holes */
		struct PEdge *nextcollapse;		/* simplification */
	} u;

	struct PVert *vert;
	struct PEdge *pair;
	struct PEdge *next;
	struct PFace *face;
	float *orig_uv, old_uv[2];
	unsigned short flag;

} PEdge;

typedef struct PFace {
	struct PFace *nextlink;

	union PFaceUnion {
		PHashKey key;			/* construct */
		int chart;				/* construct splitting*/
		float area3d;			/* stretch */
		int id;					/* abf matrix index */
	} u;

	struct PEdge *edge;
	unsigned char flag;

} PFace;

enum PVertFlag {
	PVERT_PIN = 1,
	PVERT_SELECT = 2,
	PVERT_INTERIOR = 4,
	PVERT_COLLAPSE = 8,
	PVERT_SPLIT = 16
};

enum PEdgeFlag {
	PEDGE_SEAM = 1,
	PEDGE_VERTEX_SPLIT = 2,
	PEDGE_PIN = 4,
	PEDGE_SELECT = 8,
	PEDGE_DONE = 16,
	PEDGE_FILLED = 32,
	PEDGE_COLLAPSE = 64,
	PEDGE_COLLAPSE_EDGE = 128,
	PEDGE_COLLAPSE_PAIR = 256
};

/* for flipping faces */
#define PEDGE_VERTEX_FLAGS (PEDGE_PIN)

enum PFaceFlag {
	PFACE_CONNECTED = 1,
	PFACE_FILLED = 2,
	PFACE_COLLAPSE = 4
};

/* Chart */

typedef struct PChart {
	PVert *verts;
	PEdge *edges;
	PFace *faces;
	int nverts, nedges, nfaces;

	PVert *collapsed_verts;
	PEdge *collapsed_edges;
	PFace *collapsed_faces;

	union PChartUnion {
		struct PChartLscm {
			NLContext context;
			float *abf_alpha;
			PVert *pin1, *pin2;
		} lscm;
		struct PChartPack {
			float rescale, area;
			float size[2], trans[2];
		} pack;
	} u;

	unsigned char flag;
	struct PHandle *handle;
} PChart;

enum PChartFlag {
	PCHART_NOPACK = 1
};

enum PHandleState {
	PHANDLE_STATE_ALLOCATED,
	PHANDLE_STATE_CONSTRUCTED,
	PHANDLE_STATE_LSCM,
	PHANDLE_STATE_STRETCH
};

typedef struct PHandle {
	enum PHandleState state;
	MemArena *arena;

	PChart *construction_chart;
	PHash *hash_verts;
	PHash *hash_edges;
	PHash *hash_faces;

	PChart **charts;
	int ncharts;

	float aspx, aspy;

	RNG *rng;
	float blend;
} PHandle;

#endif /*__PARAMETRIZER_INTERN_H__*/

