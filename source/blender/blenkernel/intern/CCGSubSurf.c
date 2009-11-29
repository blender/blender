/* $Id$ */

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "CCGSubSurf.h"

#include "BLO_sys_types.h" // for intptr_t support

/* used for normalize_v3 in BLI_math_vector
 * float.h's FLT_EPSILON causes trouble with subsurf normals - campbell */
#define EPSILON (1.0e-35f)

/***/

typedef unsigned char	byte;

/***/

static int kHashSizes[] = {
	1, 3, 5, 11, 17, 37, 67, 131, 257, 521, 1031, 2053, 4099, 8209, 
	16411, 32771, 65537, 131101, 262147, 524309, 1048583, 2097169, 
	4194319, 8388617, 16777259, 33554467, 67108879, 134217757, 268435459
};

typedef struct _EHEntry EHEntry;
struct _EHEntry {
	EHEntry *next;
	void *key;
};
typedef struct _EHash {
	EHEntry **buckets;
	int numEntries, curSize, curSizeIdx;

	CCGAllocatorIFC allocatorIFC;
	CCAllocHDL allocator;
} EHash;

#define EHASH_alloc(eh, nb)			((eh)->allocatorIFC.alloc((eh)->allocator, nb))
#define EHASH_free(eh, ptr)			((eh)->allocatorIFC.free((eh)->allocator, ptr))

#define EHASH_hash(eh, item)	(((uintptr_t) (item))%((unsigned int) (eh)->curSize))

static EHash *_ehash_new(int estimatedNumEntries, CCGAllocatorIFC *allocatorIFC, CCAllocHDL allocator) {
	EHash *eh = allocatorIFC->alloc(allocator, sizeof(*eh));
	eh->allocatorIFC = *allocatorIFC;
	eh->allocator = allocator;
	eh->numEntries = 0;
	eh->curSizeIdx = 0;
	while (kHashSizes[eh->curSizeIdx]<estimatedNumEntries)
		eh->curSizeIdx++;
	eh->curSize = kHashSizes[eh->curSizeIdx];
	eh->buckets = EHASH_alloc(eh, eh->curSize*sizeof(*eh->buckets));
	memset(eh->buckets, 0, eh->curSize*sizeof(*eh->buckets));

	return eh;
}
typedef void (*EHEntryFreeFP)(EHEntry *, void *);
static void _ehash_free(EHash *eh, EHEntryFreeFP freeEntry, void *userData) {
	int numBuckets = eh->curSize;

	while (numBuckets--) {
		EHEntry *entry = eh->buckets[numBuckets];

		while (entry) {
			EHEntry *next = entry->next;

			freeEntry(entry, userData);

			entry = next;
		}
	}

	EHASH_free(eh, eh->buckets);
	EHASH_free(eh, eh);
}

static void _ehash_insert(EHash *eh, EHEntry *entry) {
	int numBuckets = eh->curSize;
	int hash = EHASH_hash(eh, entry->key);
	entry->next = eh->buckets[hash];
	eh->buckets[hash] = entry;
	eh->numEntries++;

	if (eh->numEntries > (numBuckets*3)) {
		EHEntry **oldBuckets = eh->buckets;
		eh->curSize = kHashSizes[++eh->curSizeIdx];
		
		eh->buckets = EHASH_alloc(eh, eh->curSize*sizeof(*eh->buckets));
		memset(eh->buckets, 0, eh->curSize*sizeof(*eh->buckets));

		while (numBuckets--) {
			for (entry = oldBuckets[numBuckets]; entry;) {
				EHEntry *next = entry->next;
				
				hash = EHASH_hash(eh, entry->key);
				entry->next = eh->buckets[hash];
				eh->buckets[hash] = entry;
				
				entry = next;
			}
		}

		EHASH_free(eh, oldBuckets);
	}
}

static void *_ehash_lookupWithPrev(EHash *eh, void *key, void ***prevp_r) {
	int hash = EHASH_hash(eh, key);
	void **prevp = (void**) &eh->buckets[hash];
	EHEntry *entry;
	
	for (; (entry = *prevp); prevp = (void**) &entry->next) {
		if (entry->key==key) {
			*prevp_r = (void**) prevp;
			return entry;
		}
	}
	
	return NULL;
}

static void *_ehash_lookup(EHash *eh, void *key) {
	int hash = EHASH_hash(eh, key);
	EHEntry *entry;
	
	for (entry = eh->buckets[hash]; entry; entry = entry->next)
		if (entry->key==key)
			break;
	
	return entry;
}

/**/

typedef struct _EHashIterator {
	EHash *eh;
	int curBucket;
	EHEntry *curEntry;
} EHashIterator;

static EHashIterator *_ehashIterator_new(EHash *eh) {
	EHashIterator *ehi = EHASH_alloc(eh, sizeof(*ehi));
	ehi->eh = eh;
	ehi->curEntry = NULL;
	ehi->curBucket = -1;
	while (!ehi->curEntry) {
		ehi->curBucket++;
		if (ehi->curBucket==ehi->eh->curSize)
			break;
		ehi->curEntry = ehi->eh->buckets[ehi->curBucket];
	}
	return ehi;
}
static void _ehashIterator_free(EHashIterator *ehi) {
	EHASH_free(ehi->eh, ehi);
}

static void *_ehashIterator_getCurrent(EHashIterator *ehi) {
	return ehi->curEntry;
}

static void _ehashIterator_next(EHashIterator *ehi) {
	if (ehi->curEntry) {
        ehi->curEntry = ehi->curEntry->next;
		while (!ehi->curEntry) {
			ehi->curBucket++;
			if (ehi->curBucket==ehi->eh->curSize)
				break;
			ehi->curEntry = ehi->eh->buckets[ehi->curBucket];
		}
	}
}
static int _ehashIterator_isStopped(EHashIterator *ehi) {
	return !ehi->curEntry;
}

/***/

static void *_stdAllocator_alloc(CCAllocHDL a, int numBytes) {
	return malloc(numBytes);
}
static void *_stdAllocator_realloc(CCAllocHDL a, void *ptr, int newSize, int oldSize) {
	return realloc(ptr, newSize);
}
static void _stdAllocator_free(CCAllocHDL a, void *ptr) {
	free(ptr);
}

static CCGAllocatorIFC *_getStandardAllocatorIFC(void) {
	static CCGAllocatorIFC ifc;

	ifc.alloc = _stdAllocator_alloc;
	ifc.realloc = _stdAllocator_realloc;
	ifc.free = _stdAllocator_free;
	ifc.release = NULL;

	return &ifc;
}

/***/

static int VertDataEqual(float *a, float *b) {
	return a[0]==b[0] && a[1]==b[1] && a[2]==b[2];
}
#define VertDataZero(av)				{ float *_a = (float*) av; _a[0] = _a[1] = _a[2] = 0.0f; }
#define VertDataCopy(av, bv)			{ float *_a = (float*) av, *_b = (float*) bv; _a[0] =_b[0]; _a[1] =_b[1]; _a[2] =_b[2]; }
#define VertDataAdd(av, bv)				{ float *_a = (float*) av, *_b = (float*) bv; _a[0]+=_b[0]; _a[1]+=_b[1]; _a[2]+=_b[2]; }
#define VertDataSub(av, bv)				{ float *_a = (float*) av, *_b = (float*) bv; _a[0]-=_b[0]; _a[1]-=_b[1]; _a[2]-=_b[2]; }
#define VertDataMulN(av, n)				{ float *_a = (float*) av; _a[0]*=n; _a[1]*=n; _a[2]*=n; }
#define VertDataAvg4(tv, av, bv, cv, dv) \
	{ \
		float *_t = (float*) tv, *_a = (float*) av, *_b = (float*) bv, *_c = (float*) cv, *_d = (float*) dv; \
		_t[0] = (_a[0]+_b[0]+_c[0]+_d[0])*.25; \
		_t[1] = (_a[1]+_b[1]+_c[1]+_d[1])*.25; \
		_t[2] = (_a[2]+_b[2]+_c[2]+_d[2])*.25; \
	}
#define NormZero(av)					{ float *_a = (float*) av; _a[0] = _a[1] = _a[2] = 0.0f; }
#define NormCopy(av, bv)				{ float *_a = (float*) av, *_b = (float*) bv; _a[0] =_b[0]; _a[1] =_b[1]; _a[2] =_b[2]; }
#define NormAdd(av, bv)					{ float *_a = (float*) av, *_b = (float*) bv; _a[0]+=_b[0]; _a[1]+=_b[1]; _a[2]+=_b[2]; }


static int _edge_isBoundary(CCEdge *e);

/***/

enum {
	Vert_eEffected=		(1<<0),
	Vert_eChanged=		(1<<1),
	Vert_eSeam=			(1<<2),
} VertFlags;
enum {
	Edge_eEffected=		(1<<0),
} CCEdgeFlags;
enum {
	Face_eEffected=		(1<<0),
} FaceFlags;

struct _CCVert {
	CCVert		*next;	/* EHData.next */
	CCVertHDL	vHDL;	/* EHData.key */

	short numEdges, numFaces, flags, pad;

	CCEdge **edges;
	CCFace **faces;
//	byte *levelData;
//	byte *userData;
};
#define VERT_getLevelData(v)		((byte*) &(v)[1])

struct _CCEdge {
	CCEdge		*next;	/* EHData.next */
	CCEdgeHDL	eHDL;	/* EHData.key */

	short numFaces, flags;
	float crease;

	CCVert *v0,*v1;
	CCFace **faces;

//	byte *levelData;
//	byte *userData;
};
#define EDGE_getLevelData(e)		((byte*) &(e)[1])

struct _CCFace {
	CCFace		*next;	/* EHData.next */
	CCFaceHDL	fHDL;	/* EHData.key */

	short numVerts, flags, pad1, pad2;

//	CCVert **verts;
//	CCEdge **edges;
//	byte *centerData;
//	byte **gridData;
//	byte *userData;
};
#define FACE_getVerts(f)		((CCVert**) &(f)[1])
#define FACE_getEdges(f)		((CCEdge**) &(FACE_getVerts(f)[(f)->numVerts]))
#define FACE_getCenterData(f)	((byte*) &(FACE_getEdges(f)[(f)->numVerts]))

typedef enum {
	eSyncState_None = 0,
	eSyncState_Vert,
	eSyncState_Edge,
	eSyncState_Face,
	eSyncState_Partial,
} SyncState;

struct _CSubSurf {
	EHash *vMap;	/* map of CCVertHDL -> Vert */
	EHash *eMap;	/* map of CCEdgeHDL -> Edge */
	EHash *fMap;	/* map of CCFaceHDL -> Face */

	CCGMeshIFC meshIFC;
	
	CCGAllocatorIFC allocatorIFC;
	CCAllocHDL allocator;

	int subdivLevels;
	int numGrids;
	int allowEdgeCreation;
	float defaultCreaseValue;
	void *defaultEdgeUserData;

	void *q, *r;
		
		// data for calc vert normals
	int calcVertNormals;
	int normalDataOffset;

		// data for age'ing (to debug sync)
	int currentAge;
	int useAgeCounts;
	int vertUserAgeOffset;
	int edgeUserAgeOffset;
	int faceUserAgeOffset;

		// data used during syncing
	SyncState syncState;

	EHash *oldVMap, *oldEMap, *oldFMap;
	int lenTempArrays;
	CCVert **tempVerts;
	CCEdge **tempEdges;
};

#define CCGSUBSURF_alloc(ss, nb)			((ss)->allocatorIFC.alloc((ss)->allocator, nb))
#define CCGSUBSURF_realloc(ss, ptr, nb, ob)	((ss)->allocatorIFC.realloc((ss)->allocator, ptr, nb, ob))
#define CCGSUBSURF_free(ss, ptr)			((ss)->allocatorIFC.free((ss)->allocator, ptr))

/***/

static CCVert *_vert_new(CCVertHDL vHDL, CSubSurf *ss) {
	CCVert *v = CCGSUBSURF_alloc(ss, sizeof(CCVert) + ss->meshIFC.vertDataSize * (ss->subdivLevels+1) + ss->meshIFC.vertUserSize);
	byte *userData;

	v->vHDL = vHDL;
	v->edges = NULL;
	v->faces = NULL;
	v->numEdges = v->numFaces = 0;
	v->flags = 0;

	userData = CCS_getVertUserData(ss, v);
	memset(userData, 0, ss->meshIFC.vertUserSize);
	if (ss->useAgeCounts) *((int*) &userData[ss->vertUserAgeOffset]) = ss->currentAge;

	return v;
}
static void _vert_remEdge(CCVert *v, CCEdge *e) {
	int i;
	for (i=0; i<v->numEdges; i++) {
		if (v->edges[i]==e) {
			v->edges[i] = v->edges[--v->numEdges];
			break;
		}
	}
}
static void _vert_remFace(CCVert *v, CCFace *f) {
	int i;
	for (i=0; i<v->numFaces; i++) {
		if (v->faces[i]==f) {
			v->faces[i] = v->faces[--v->numFaces];
			break;
		}
	}
}
static void _vert_addEdge(CCVert *v, CCEdge *e, CSubSurf *ss) {
	v->edges = CCGSUBSURF_realloc(ss, v->edges, (v->numEdges+1)*sizeof(*v->edges), v->numEdges*sizeof(*v->edges));
	v->edges[v->numEdges++] = e;
}
static void _vert_addFace(CCVert *v, CCFace *f, CSubSurf *ss) {
	v->faces = CCGSUBSURF_realloc(ss, v->faces, (v->numFaces+1)*sizeof(*v->faces), v->numFaces*sizeof(*v->faces));
	v->faces[v->numFaces++] = f;
}
static CCEdge *_vert_findEdgeTo(CCVert *v, CCVert *vQ) {
	int i;
	for (i=0; i<v->numEdges; i++) {
		CCEdge *e = v->edges[v->numEdges-1-i]; // XXX, note reverse
		if (	(e->v0==v && e->v1==vQ) ||
				(e->v1==v && e->v0==vQ))
			return e;
	}
	return 0;
}
static int _vert_isBoundary(CCVert *v) {
	int i;
	for (i=0; i<v->numEdges; i++)
		if (_edge_isBoundary(v->edges[i]))
			return 1;
	return 0;
}

static void *_vert_getCo(CCVert *v, int lvl, int dataSize) {
	return &VERT_getLevelData(v)[lvl*dataSize];
}
static float *_vert_getNo(CCVert *v, int lvl, int dataSize, int normalDataOffset) {
	return (float*) &VERT_getLevelData(v)[lvl*dataSize + normalDataOffset];
}

static void _vert_free(CCVert *v, CSubSurf *ss) {
	CCGSUBSURF_free(ss, v->edges);
	CCGSUBSURF_free(ss, v->faces);
	CCGSUBSURF_free(ss, v);
}

static int VERT_seam(CCVert *v) {
	return ((v->flags & Vert_eSeam) != 0);
}

/***/

static CCEdge *_edge_new(CCEdgeHDL eHDL, CCVert *v0, CCVert *v1, float crease, CSubSurf *ss) {
	CCEdge *e = CCGSUBSURF_alloc(ss, sizeof(CCEdge) + ss->meshIFC.vertDataSize *((ss->subdivLevels+1) + (1<<(ss->subdivLevels+1))-1) + ss->meshIFC.edgeUserSize);
	byte *userData;

	e->eHDL = eHDL;
	e->v0 = v0;
	e->v1 = v1;
	e->crease = crease;
	e->faces = NULL;
	e->numFaces = 0;
	e->flags = 0;
	_vert_addEdge(v0, e, ss);
	_vert_addEdge(v1, e, ss);

	userData = CCS_getEdgeUserData(ss, e);
	memset(userData, 0, ss->meshIFC.edgeUserSize);
	if (ss->useAgeCounts) *((int*) &userData[ss->edgeUserAgeOffset]) = ss->currentAge;

	return e;
}
static void _edge_remFace(CCEdge *e, CCFace *f) {
	int i;
	for (i=0; i<e->numFaces; i++) {
		if (e->faces[i]==f) {
			e->faces[i] = e->faces[--e->numFaces];
			break;
		}
	}
}
static void _edge_addFace(CCEdge *e, CCFace *f, CSubSurf *ss) {
	e->faces = CCGSUBSURF_realloc(ss, e->faces, (e->numFaces+1)*sizeof(*e->faces), e->numFaces*sizeof(*e->faces));
	e->faces[e->numFaces++] = f;
}
static int _edge_isBoundary(CCEdge *e) {
	return e->numFaces<2;
}

static CCVert *_edge_getOtherVert(CCEdge *e, CCVert *vQ) {
	if (vQ==e->v0) {
		return e->v1;
	} else {
		return e->v0;
	}
}

static void *_edge_getCo(CCEdge *e, int lvl, int x, int dataSize) {
	int levelBase = lvl + (1<<lvl) - 1;
	return &EDGE_getLevelData(e)[dataSize*(levelBase + x)];
}
#if 0
static float *_edge_getNo(CCEdge *e, int lvl, int x, int dataSize, int normalDataOffset) {
	int levelBase = lvl + (1<<lvl) - 1;
	return (float*) &EDGE_getLevelData(e)[dataSize*(levelBase + x) + normalDataOffset];
}
#endif
static void *_edge_getCoVert(CCEdge *e, CCVert *v, int lvl, int x, int dataSize) {
	int levelBase = lvl + (1<<lvl) - 1;
	if (v==e->v0) {
		return &EDGE_getLevelData(e)[dataSize*(levelBase + x)];
	} else {
		return &EDGE_getLevelData(e)[dataSize*(levelBase + (1<<lvl) - x)];		
	}
}

static void _edge_free(CCEdge *e, CSubSurf *ss) {
	CCGSUBSURF_free(ss, e->faces);
	CCGSUBSURF_free(ss, e);
}
static void _edge_unlinkMarkAndFree(CCEdge *e, CSubSurf *ss) {
	_vert_remEdge(e->v0, e);
	_vert_remEdge(e->v1, e);
	e->v0->flags |= Vert_eEffected;
	e->v1->flags |= Vert_eEffected;
	_edge_free(e, ss);
}

static float EDGE_getSharpness(CCEdge *e, int lvl) {
	if (!lvl)
		return e->crease;
	else if (!e->crease)
		return 0.0;
	else if (e->crease - lvl < 0.0)
		return 0.0;
	else
		return e->crease - lvl;
}

static CCFace *_face_new(CCFaceHDL fHDL, CCVert **verts, CCEdge **edges, int numVerts, CSubSurf *ss) {
	int maxGridSize = 1 + (1<<(ss->subdivLevels-1));
	CCFace *f = CCGSUBSURF_alloc(ss, sizeof(CCFace) + sizeof(CCVert*)*numVerts + sizeof(CCEdge*)*numVerts + ss->meshIFC.vertDataSize *(1 + numVerts*maxGridSize + numVerts*maxGridSize*maxGridSize) + ss->meshIFC.faceUserSize);
	byte *userData;
	int i;

	f->numVerts = numVerts;
	f->fHDL = fHDL;
	f->flags = 0;

	for (i=0; i<numVerts; i++) {
		FACE_getVerts(f)[i] = verts[i];
		FACE_getEdges(f)[i] = edges[i];
		_vert_addFace(verts[i], f, ss);
		_edge_addFace(edges[i], f, ss);
	}

	userData = CCS_getFaceUserData(ss, f);
	memset(userData, 0, ss->meshIFC.faceUserSize);
	if (ss->useAgeCounts) *((int*) &userData[ss->faceUserAgeOffset]) = ss->currentAge;

	return f;
}

static void *_face_getIECo(CCFace *f, int lvl, int S, int x, int levels, int dataSize) {
	int maxGridSize = 1 + (1<<(levels-1));
	int spacing = 1<<(levels-lvl);
	byte *gridBase = FACE_getCenterData(f) + dataSize*(1 + S*(maxGridSize + maxGridSize*maxGridSize));
	return &gridBase[dataSize*x*spacing];
}
static void *_face_getIFCo(CCFace *f, int lvl, int S, int x, int y, int levels, int dataSize) {
	int maxGridSize = 1 + (1<<(levels-1));
	int spacing = 1<<(levels-lvl);
	byte *gridBase = FACE_getCenterData(f) + dataSize*(1 + S*(maxGridSize + maxGridSize*maxGridSize));
	return &gridBase[dataSize*(maxGridSize + (y*maxGridSize + x)*spacing)];
}
static float *_face_getIFNo(CCFace *f, int lvl, int S, int x, int y, int levels, int dataSize, int normalDataOffset) {
	int maxGridSize = 1 + (1<<(levels-1));
	int spacing = 1<<(levels-lvl);
	byte *gridBase = FACE_getCenterData(f) + dataSize*(1 + S*(maxGridSize + maxGridSize*maxGridSize));
	return (float*) &gridBase[dataSize*(maxGridSize + (y*maxGridSize + x)*spacing) + normalDataOffset];
}
static int _face_getVertIndex(CCFace *f, CCVert *v) {
	int i;
	for (i=0; i<f->numVerts; i++)
		if (FACE_getVerts(f)[i]==v)
			return i;
	return -1;
}
static void *_face_getIFCoEdge(CCFace *f, CCEdge *e, int lvl, int eX, int eY, int levels, int dataSize) {
	int maxGridSize = 1 + (1<<(levels-1));
	int spacing = 1<<(levels-lvl);
	int S, x, y, cx, cy;

	for (S=0; S<f->numVerts; S++)
		if (FACE_getEdges(f)[S]==e)
			break;

	eX = eX*spacing;
	eY = eY*spacing;
	if (e->v0!=FACE_getVerts(f)[S]) {
		eX = (maxGridSize*2 - 1)-1 - eX;
	}
	y = maxGridSize - 1 - eX;
	x = maxGridSize - 1 - eY;
	if (x<0) {
		S = (S+f->numVerts-1)%f->numVerts;
		cx = y;
		cy = -x;
	} else if (y<0) {
		S = (S+1)%f->numVerts;
		cx = -y;
		cy = x;
	} else {
		cx = x;
		cy = y;
	}
	return _face_getIFCo(f, levels, S, cx, cy, levels, dataSize);
}
static float *_face_getIFNoEdge(CCFace *f, CCEdge *e, int lvl, int eX, int eY, int levels, int dataSize, int normalDataOffset) {
	return (float*) ((byte*) _face_getIFCoEdge(f, e, lvl, eX, eY, levels, dataSize) + normalDataOffset);
}
void _face_calcIFNo(CCFace *f, int lvl, int S, int x, int y, float *no, int levels, int dataSize) {
	float *a = _face_getIFCo(f, lvl, S, x+0, y+0, levels, dataSize);
	float *b = _face_getIFCo(f, lvl, S, x+1, y+0, levels, dataSize);
	float *c = _face_getIFCo(f, lvl, S, x+1, y+1, levels, dataSize);
	float *d = _face_getIFCo(f, lvl, S, x+0, y+1, levels, dataSize);
	float a_cX = c[0]-a[0], a_cY = c[1]-a[1], a_cZ = c[2]-a[2];
	float b_dX = d[0]-b[0], b_dY = d[1]-b[1], b_dZ = d[2]-b[2];
	float length;

	no[0] = b_dY*a_cZ - b_dZ*a_cY;
	no[1] = b_dZ*a_cX - b_dX*a_cZ;
	no[2] = b_dX*a_cY - b_dY*a_cX;

	length = sqrt(no[0]*no[0] + no[1]*no[1] + no[2]*no[2]);

	if (length>EPSILON) {
		float invLength = 1.f/length;

		no[0] *= invLength;
		no[1] *= invLength;
		no[2] *= invLength;
	} else {
		NormZero(no);
	}
}

static void _face_free(CCFace *f, CSubSurf *ss) {
	CCGSUBSURF_free(ss, f);
}
static void _face_unlinkMarkAndFree(CCFace *f, CSubSurf *ss) {
	int j;
	for (j=0; j<f->numVerts; j++) {
		_vert_remFace(FACE_getVerts(f)[j], f);
		_edge_remFace(FACE_getEdges(f)[j], f);
		FACE_getVerts(f)[j]->flags |= Vert_eEffected;
	}
	_face_free(f, ss);
}

/***/

CSubSurf *CCS_new(CCGMeshIFC *ifc, int subdivLevels, CCGAllocatorIFC *allocatorIFC, CCAllocHDL allocator) {
	if (!allocatorIFC) {
		allocatorIFC = _getStandardAllocatorIFC();
		allocator = NULL;
	}

	if (subdivLevels<1) {
		return NULL;
	} else {
		CSubSurf *ss = allocatorIFC->alloc(allocator, sizeof(*ss));

		ss->allocatorIFC = *allocatorIFC;
		ss->allocator = allocator;

		ss->vMap = _ehash_new(0, &ss->allocatorIFC, ss->allocator);
		ss->eMap = _ehash_new(0, &ss->allocatorIFC, ss->allocator);
		ss->fMap = _ehash_new(0, &ss->allocatorIFC, ss->allocator);

		ss->meshIFC = *ifc;
		
		ss->subdivLevels = subdivLevels;
		ss->numGrids = 0;
		ss->allowEdgeCreation = 0;
		ss->defaultCreaseValue = 0;
		ss->defaultEdgeUserData = NULL;

		ss->useAgeCounts = 0;
		ss->vertUserAgeOffset = ss->edgeUserAgeOffset = ss->faceUserAgeOffset = 0;

		ss->calcVertNormals = 0;
		ss->normalDataOffset = 0;

		ss->q = CCGSUBSURF_alloc(ss, ss->meshIFC.vertDataSize);
		ss->r = CCGSUBSURF_alloc(ss, ss->meshIFC.vertDataSize);

		ss->currentAge = 0;

		ss->syncState = eSyncState_None;

		ss->oldVMap = ss->oldEMap = ss->oldFMap = NULL;
		ss->lenTempArrays = 0;
		ss->tempVerts = NULL;
		ss->tempEdges = NULL;	

		return ss;
	}
}

void CCS_free(CSubSurf *ss) {
	CCGAllocatorIFC allocatorIFC = ss->allocatorIFC;
	CCAllocHDL allocator = ss->allocator;

	if (ss->syncState) {
		_ehash_free(ss->oldFMap, (EHEntryFreeFP) _face_free, ss);
		_ehash_free(ss->oldEMap, (EHEntryFreeFP) _edge_free, ss);
		_ehash_free(ss->oldVMap, (EHEntryFreeFP) _vert_free, ss);

		CCGSUBSURF_free(ss, ss->tempVerts);
		CCGSUBSURF_free(ss, ss->tempEdges);
	}

	CCGSUBSURF_free(ss, ss->r);
	CCGSUBSURF_free(ss, ss->q);
	if (ss->defaultEdgeUserData) CCGSUBSURF_free(ss, ss->defaultEdgeUserData);

	_ehash_free(ss->fMap, (EHEntryFreeFP) _face_free, ss);
	_ehash_free(ss->eMap, (EHEntryFreeFP) _edge_free, ss);
	_ehash_free(ss->vMap, (EHEntryFreeFP) _vert_free, ss);

	CCGSUBSURF_free(ss, ss);

	if (allocatorIFC.release) {
		allocatorIFC.release(allocator);
	}
}

CCGError CCS_setAllowEdgeCreation(CSubSurf *ss, int allowEdgeCreation, float defaultCreaseValue, void *defaultUserData) {
	if (ss->defaultEdgeUserData) {
		CCGSUBSURF_free(ss, ss->defaultEdgeUserData);
	}

	ss->allowEdgeCreation = !!allowEdgeCreation;
	ss->defaultCreaseValue = defaultCreaseValue;
	ss->defaultEdgeUserData = CCGSUBSURF_alloc(ss, ss->meshIFC.edgeUserSize);

	if (defaultUserData) {
		memcpy(ss->defaultEdgeUserData, defaultUserData, ss->meshIFC.edgeUserSize);
	} else {
		memset(ss->defaultEdgeUserData, 0, ss->meshIFC.edgeUserSize);
	}

	return eCCGError_None;
}
void CCS_getAllowEdgeCreation(CSubSurf *ss, int *allowEdgeCreation_r, float *defaultCreaseValue_r, void *defaultUserData_r) {
	if (allowEdgeCreation_r) *allowEdgeCreation_r = ss->allowEdgeCreation;
	if (ss->allowEdgeCreation) {
		if (defaultCreaseValue_r) *defaultCreaseValue_r = ss->defaultCreaseValue;
		if (defaultUserData_r) memcpy(defaultUserData_r, ss->defaultEdgeUserData, ss->meshIFC.edgeUserSize);
	}
}

CCGError CCS_setSubdivisionLevels(CSubSurf *ss, int subdivisionLevels) {
	if (subdivisionLevels<=0) {
		return eCCGError_InvalidValue;
	} else if (subdivisionLevels!=ss->subdivLevels) {
		ss->numGrids = 0;
		ss->subdivLevels = subdivisionLevels;
		_ehash_free(ss->vMap, (EHEntryFreeFP) _vert_free, ss);
		_ehash_free(ss->eMap, (EHEntryFreeFP) _edge_free, ss);
		_ehash_free(ss->fMap, (EHEntryFreeFP) _face_free, ss);
		ss->vMap = _ehash_new(0, &ss->allocatorIFC, ss->allocator);
		ss->eMap = _ehash_new(0, &ss->allocatorIFC, ss->allocator);
		ss->fMap = _ehash_new(0, &ss->allocatorIFC, ss->allocator);
	}

	return eCCGError_None;
}

void CCS_getUseAgeCounts(CSubSurf *ss, int *useAgeCounts_r, int *vertUserOffset_r, int *edgeUserOffset_r, int *faceUserOffset_r)
{
	*useAgeCounts_r = ss->useAgeCounts;

	if (vertUserOffset_r) *vertUserOffset_r = ss->vertUserAgeOffset;
	if (edgeUserOffset_r) *edgeUserOffset_r = ss->edgeUserAgeOffset;
	if (faceUserOffset_r) *faceUserOffset_r = ss->faceUserAgeOffset;
}

CCGError CCS_setUseAgeCounts(CSubSurf *ss, int useAgeCounts, int vertUserOffset, int edgeUserOffset, int faceUserOffset) {
	if (useAgeCounts) {
		if (	(vertUserOffset+4>ss->meshIFC.vertUserSize) ||
				(edgeUserOffset+4>ss->meshIFC.edgeUserSize) ||
				(faceUserOffset+4>ss->meshIFC.faceUserSize)) {
			return eCCGError_InvalidValue;
		}  else {
			ss->useAgeCounts = 1;
			ss->vertUserAgeOffset = vertUserOffset;
			ss->edgeUserAgeOffset = edgeUserOffset;
			ss->faceUserAgeOffset = faceUserOffset;
		}
	} else {
		ss->useAgeCounts = 0;
		ss->vertUserAgeOffset = ss->edgeUserAgeOffset = ss->faceUserAgeOffset = 0;
	}

	return eCCGError_None;
}

CCGError CCS_setCalcVertexNormals(CSubSurf *ss, int useVertNormals, int normalDataOffset) {
	if (useVertNormals) {
		if (normalDataOffset<0 || normalDataOffset+12>ss->meshIFC.vertDataSize) {
			return eCCGError_InvalidValue;
		} else {
			ss->calcVertNormals = 1;
			ss->normalDataOffset = normalDataOffset;
		}
	} else {
		ss->calcVertNormals = 0;
		ss->normalDataOffset = 0;
	}

	return eCCGError_None;
}

/***/

CCGError CCS_initFullSync(CSubSurf *ss) {
	if (ss->syncState!=eSyncState_None) {
		return eCCGError_InvalidSyncState;
	}

	ss->currentAge++;

	ss->oldVMap = ss->vMap; 
	ss->oldEMap = ss->eMap; 
	ss->oldFMap = ss->fMap;

	ss->vMap = _ehash_new(0, &ss->allocatorIFC, ss->allocator);
	ss->eMap = _ehash_new(0, &ss->allocatorIFC, ss->allocator);
	ss->fMap = _ehash_new(0, &ss->allocatorIFC, ss->allocator);

	ss->numGrids = 0;

	ss->lenTempArrays = 12;
	ss->tempVerts = CCGSUBSURF_alloc(ss, sizeof(*ss->tempVerts)*ss->lenTempArrays);
	ss->tempEdges = CCGSUBSURF_alloc(ss, sizeof(*ss->tempEdges)*ss->lenTempArrays);

	ss->syncState = eSyncState_Vert;

	return eCCGError_None;
}

CCGError CCS_initPartialSync(CSubSurf *ss) {
	if (ss->syncState!=eSyncState_None) {
		return eCCGError_InvalidSyncState;
	}

	ss->currentAge++;

	ss->syncState = eSyncState_Partial;

	return eCCGError_None;
}

CCGError CCS_syncVertDel(CSubSurf *ss, CCVertHDL vHDL) {
	if (ss->syncState!=eSyncState_Partial) {
		return eCCGError_InvalidSyncState;
	} else {
		void **prevp;
		CCVert *v = _ehash_lookupWithPrev(ss->vMap, vHDL, &prevp);

		if (!v || v->numFaces || v->numEdges) {
			return eCCGError_InvalidValue;
		} else {
			*prevp = v->next;
			_vert_free(v, ss);
		}
	}

	return eCCGError_None;
}

CCGError CCS_syncEdgeDel(CSubSurf *ss, CCEdgeHDL eHDL) {
	if (ss->syncState!=eSyncState_Partial) {
		return eCCGError_InvalidSyncState;
	} else {
		void **prevp;
		CCEdge *e = _ehash_lookupWithPrev(ss->eMap, eHDL, &prevp);

		if (!e || e->numFaces) {
			return eCCGError_InvalidValue;
		} else {
			*prevp = e->next;
			_edge_unlinkMarkAndFree(e, ss);
		}
	}

	return eCCGError_None;
}

CCGError CCS_syncFaceDel(CSubSurf *ss, CCFaceHDL fHDL) {
	if (ss->syncState!=eSyncState_Partial) {
		return eCCGError_InvalidSyncState;
	} else {
		void **prevp;
		CCFace *f = _ehash_lookupWithPrev(ss->fMap, fHDL, &prevp);

		if (!f) {
			return eCCGError_InvalidValue;
		} else {
			*prevp = f->next;
			_face_unlinkMarkAndFree(f, ss);
		}
	}

	return eCCGError_None;
}

CCGError CCS_syncVert(CSubSurf *ss, CCVertHDL vHDL, void *vertData, int seam, CCVert **v_r) {
	void **prevp;
	CCVert *v = NULL;
	short seamflag = (seam)? Vert_eSeam: 0;
	
	if (ss->syncState==eSyncState_Partial) {
		v = _ehash_lookupWithPrev(ss->vMap, vHDL, &prevp);
		if (!v) {
			v = _vert_new(vHDL, ss);
			VertDataCopy(_vert_getCo(v,0,ss->meshIFC.vertDataSize), vertData);
			_ehash_insert(ss->vMap, (EHEntry*) v);
			v->flags = Vert_eEffected|seamflag;
		} else if (!VertDataEqual(vertData, _vert_getCo(v, 0, ss->meshIFC.vertDataSize)) || ((v->flags & Vert_eSeam) != seamflag)) {
			int i, j;

			VertDataCopy(_vert_getCo(v,0,ss->meshIFC.vertDataSize), vertData);
			v->flags = Vert_eEffected|seamflag;

			for (i=0; i<v->numEdges; i++) {
				CCEdge *e = v->edges[i];
				e->v0->flags |= Vert_eEffected;
				e->v1->flags |= Vert_eEffected;
			}
			for (i=0; i<v->numFaces; i++) {
				CCFace *f = v->faces[i];
				for (j=0; j<f->numVerts; j++) {
					FACE_getVerts(f)[j]->flags |= Vert_eEffected;
				}
			}
		}
	} else {
		if (ss->syncState!=eSyncState_Vert) { 
			return eCCGError_InvalidSyncState;
		}

		v = _ehash_lookupWithPrev(ss->oldVMap, vHDL, &prevp);
		if (!v) {
			v = _vert_new(vHDL, ss);
			VertDataCopy(_vert_getCo(v,0,ss->meshIFC.vertDataSize), vertData);
			_ehash_insert(ss->vMap, (EHEntry*) v);
			v->flags = Vert_eEffected|seamflag;
		} else if (!VertDataEqual(vertData, _vert_getCo(v, 0, ss->meshIFC.vertDataSize)) || ((v->flags & Vert_eSeam) != seamflag)) {
			*prevp = v->next;
			_ehash_insert(ss->vMap, (EHEntry*) v);
			VertDataCopy(_vert_getCo(v,0,ss->meshIFC.vertDataSize), vertData);
			v->flags = Vert_eEffected|Vert_eChanged|seamflag;
		} else {
			*prevp = v->next;
			_ehash_insert(ss->vMap, (EHEntry*) v);
			v->flags = 0;
		}
	}

	if (v_r) *v_r = v;
	return eCCGError_None;
}

CCGError CCS_syncEdge(CSubSurf *ss, CCEdgeHDL eHDL, CCVertHDL e_vHDL0, CCVertHDL e_vHDL1, float crease, CCEdge **e_r) {
	void **prevp;
	CCEdge *e = NULL, *eNew;

	if (ss->syncState==eSyncState_Partial) {
		e = _ehash_lookupWithPrev(ss->eMap, eHDL, &prevp);
		if (!e || e->v0->vHDL!=e_vHDL0 || e->v1->vHDL!=e_vHDL1 || crease!=e->crease) {
			CCVert *v0 = _ehash_lookup(ss->vMap, e_vHDL0);
			CCVert *v1 = _ehash_lookup(ss->vMap, e_vHDL1);

			eNew = _edge_new(eHDL, v0, v1, crease, ss);

			if (e) {
				*prevp = eNew;
				eNew->next = e->next;

				_edge_unlinkMarkAndFree(e, ss);
			} else {
				_ehash_insert(ss->eMap, (EHEntry*) eNew);
			}

			eNew->v0->flags |= Vert_eEffected;
			eNew->v1->flags |= Vert_eEffected;
		}
	} else {
		if (ss->syncState==eSyncState_Vert) {
			ss->syncState = eSyncState_Edge;
		} else if (ss->syncState!=eSyncState_Edge) {
			return eCCGError_InvalidSyncState;
		}

		e = _ehash_lookupWithPrev(ss->oldEMap, eHDL, &prevp);
		if (!e || e->v0->vHDL!=e_vHDL0 || e->v1->vHDL!=e_vHDL1|| e->crease!=crease) {
			CCVert *v0 = _ehash_lookup(ss->vMap, e_vHDL0);
			CCVert *v1 = _ehash_lookup(ss->vMap, e_vHDL1);
			e = _edge_new(eHDL, v0, v1, crease, ss);
			_ehash_insert(ss->eMap, (EHEntry*) e);
			e->v0->flags |= Vert_eEffected;
			e->v1->flags |= Vert_eEffected;
		} else {
			*prevp = e->next;
			_ehash_insert(ss->eMap, (EHEntry*) e);
			e->flags = 0;
			if ((e->v0->flags|e->v1->flags)&Vert_eChanged) {
				e->v0->flags |= Vert_eEffected;
				e->v1->flags |= Vert_eEffected;
			}
		}
	}

	if (e_r) *e_r = e;
	return eCCGError_None;
}

CCGError CCS_syncFace(CSubSurf *ss, CCFaceHDL fHDL, int numVerts, CCVertHDL *vHDLs, CCFace **f_r) {
	void **prevp;
	CCFace *f = NULL, *fNew;
	int j, k, topologyChanged = 0;

	if (numVerts>ss->lenTempArrays) {
		int oldLen = ss->lenTempArrays;
		ss->lenTempArrays = (numVerts<ss->lenTempArrays*2)?ss->lenTempArrays*2:numVerts;
		ss->tempVerts = CCGSUBSURF_realloc(ss, ss->tempVerts, sizeof(*ss->tempVerts)*ss->lenTempArrays, sizeof(*ss->tempVerts)*oldLen);
		ss->tempEdges = CCGSUBSURF_realloc(ss, ss->tempEdges, sizeof(*ss->tempEdges)*ss->lenTempArrays, sizeof(*ss->tempEdges)*oldLen);
	}

	if (ss->syncState==eSyncState_Partial) {
		f = _ehash_lookupWithPrev(ss->fMap, fHDL, &prevp);

		for (k=0; k<numVerts; k++) {
			ss->tempVerts[k] = _ehash_lookup(ss->vMap, vHDLs[k]);
		}
		for (k=0; k<numVerts; k++) {
			ss->tempEdges[k] = _vert_findEdgeTo(ss->tempVerts[k], ss->tempVerts[(k+1)%numVerts]);
		}

		if (f) {
			if (	f->numVerts!=numVerts ||
					memcmp(FACE_getVerts(f), ss->tempVerts, sizeof(*ss->tempVerts)*numVerts) ||
					memcmp(FACE_getEdges(f), ss->tempEdges, sizeof(*ss->tempEdges)*numVerts))
				topologyChanged = 1;
		}

		if (!f || topologyChanged) {
			fNew = _face_new(fHDL, ss->tempVerts, ss->tempEdges, numVerts, ss);

			if (f) {
				ss->numGrids += numVerts - f->numVerts;

				*prevp = fNew;
				fNew->next = f->next;

				_face_unlinkMarkAndFree(f, ss);
			} else {
				ss->numGrids += numVerts;
				_ehash_insert(ss->fMap, (EHEntry*) fNew);
			}

			for (k=0; k<numVerts; k++)
				FACE_getVerts(fNew)[k]->flags |= Vert_eEffected;
		}
	} else {
		if (ss->syncState==eSyncState_Vert || ss->syncState==eSyncState_Edge) {
			ss->syncState = eSyncState_Face;
		} else if (ss->syncState!=eSyncState_Face) {
			return eCCGError_InvalidSyncState;
		}

		f = _ehash_lookupWithPrev(ss->oldFMap, fHDL, &prevp);

		for (k=0; k<numVerts; k++) {
			ss->tempVerts[k] = _ehash_lookup(ss->vMap, vHDLs[k]);

			if (!ss->tempVerts[k])
				return eCCGError_InvalidValue;
		}
		for (k=0; k<numVerts; k++) {
			ss->tempEdges[k] = _vert_findEdgeTo(ss->tempVerts[k], ss->tempVerts[(k+1)%numVerts]);

			if (!ss->tempEdges[k]) {
				if (ss->allowEdgeCreation) {
					CCEdge *e = ss->tempEdges[k] = _edge_new((CCEdgeHDL) -1, ss->tempVerts[k], ss->tempVerts[(k+1)%numVerts], ss->defaultCreaseValue, ss);
					_ehash_insert(ss->eMap, (EHEntry*) e);
					e->v0->flags |= Vert_eEffected;
					e->v1->flags |= Vert_eEffected;
					if (ss->meshIFC.edgeUserSize) {
						memcpy(CCS_getEdgeUserData(ss, e), ss->defaultEdgeUserData, ss->meshIFC.edgeUserSize);
					}
				} else {
					return eCCGError_InvalidValue;
				}
			}
		}

		if (f) {
			if (	f->numVerts!=numVerts ||
					memcmp(FACE_getVerts(f), ss->tempVerts, sizeof(*ss->tempVerts)*numVerts) ||
					memcmp(FACE_getEdges(f), ss->tempEdges, sizeof(*ss->tempEdges)*numVerts))
				topologyChanged = 1;
		}

		if (!f || topologyChanged) {
			f = _face_new(fHDL, ss->tempVerts, ss->tempEdges, numVerts, ss);
			_ehash_insert(ss->fMap, (EHEntry*) f);
			ss->numGrids += numVerts;

			for (k=0; k<numVerts; k++)
				FACE_getVerts(f)[k]->flags |= Vert_eEffected;
		} else {
			*prevp = f->next;
			_ehash_insert(ss->fMap, (EHEntry*) f);
			f->flags = 0;
			ss->numGrids += f->numVerts;

			for (j=0; j<f->numVerts; j++) {
				if (FACE_getVerts(f)[j]->flags&Vert_eChanged) {
					for (k=0; k<f->numVerts; k++)
						FACE_getVerts(f)[k]->flags |= Vert_eEffected;
					break;
				}
			}
		}
	}

	if (f_r) *f_r = f;
	return eCCGError_None;
}

static void CCS__sync(CSubSurf *ss);
CCGError CCS_processSync(CSubSurf *ss) {
	if (ss->syncState==eSyncState_Partial) {
		ss->syncState = eSyncState_None;

		CCS__sync(ss);
	} else if (ss->syncState) {
		_ehash_free(ss->oldFMap, (EHEntryFreeFP) _face_unlinkMarkAndFree, ss);
		_ehash_free(ss->oldEMap, (EHEntryFreeFP) _edge_unlinkMarkAndFree, ss);
		_ehash_free(ss->oldVMap, (EHEntryFreeFP) _vert_free, ss);
		CCGSUBSURF_free(ss, ss->tempEdges);
		CCGSUBSURF_free(ss, ss->tempVerts);

		ss->lenTempArrays = 0;

		ss->oldFMap = ss->oldEMap = ss->oldVMap = NULL;
		ss->tempVerts = NULL;
		ss->tempEdges = NULL;

		ss->syncState = eSyncState_None;

		CCS__sync(ss);
	} else {
		return eCCGError_InvalidSyncState;
	}

	return eCCGError_None;
}

static void CCS__sync(CSubSurf *ss) {
	CCVert **effectedV;
	CCEdge **effectedE;
	CCFace **effectedF;
	int numEffectedV, numEffectedE, numEffectedF;
	int subdivLevels = ss->subdivLevels;
	int vertDataSize = ss->meshIFC.vertDataSize;
	int i,ptrIdx,cornerIdx;
	int S,x,y;
	void *q = ss->q, *r = ss->r;
	int curLvl, nextLvl;
	int j;

	effectedV = CCGSUBSURF_alloc(ss, sizeof(*effectedV)*ss->vMap->numEntries);
	effectedE = CCGSUBSURF_alloc(ss, sizeof(*effectedE)*ss->eMap->numEntries);
	effectedF = CCGSUBSURF_alloc(ss, sizeof(*effectedF)*ss->fMap->numEntries);
	numEffectedV = numEffectedE = numEffectedF = 0;
	for (i=0; i<ss->vMap->curSize; i++) {
		CCVert *v = (CCVert*) ss->vMap->buckets[i];
		for (; v; v = v->next) {
			if (v->flags&Vert_eEffected) {
				effectedV[numEffectedV++] = v;

				for (j=0; j<v->numEdges; j++) {
					CCEdge *e = v->edges[j];
					if (!(e->flags&Edge_eEffected)) {
						effectedE[numEffectedE++] = e;
						e->flags |= Edge_eEffected;
					}
				}

				for (j=0; j<v->numFaces; j++) {
					CCFace *f = v->faces[j];
					if (!(f->flags&Face_eEffected)) {
						effectedF[numEffectedF++] = f;
						f->flags |= Face_eEffected;
					}
				}
			}
		}
	}

#define VERT_getCo(v, lvl)				_vert_getCo(v, lvl, vertDataSize)
#define EDGE_getCo(e, lvl, x)			_edge_getCo(e, lvl, x, vertDataSize)
#define FACE_getIECo(f, lvl, S, x)		_face_getIECo(f, lvl, S, x, subdivLevels, vertDataSize)
#define FACE_getIFCo(f, lvl, S, x, y)	_face_getIFCo(f, lvl, S, x, y, subdivLevels, vertDataSize)
	curLvl = 0;
	nextLvl = curLvl+1;

	for (ptrIdx=0; ptrIdx<numEffectedF; ptrIdx++) {
		CCFace *f = effectedF[ptrIdx];
		void *co = FACE_getCenterData(f);
		VertDataZero(co);
		for (i=0; i<f->numVerts; i++) {
			VertDataAdd(co, VERT_getCo(FACE_getVerts(f)[i], curLvl));
		}
		VertDataMulN(co, 1.0f/f->numVerts);

		f->flags = 0;
	}
	for (ptrIdx=0; ptrIdx<numEffectedE; ptrIdx++) {
		CCEdge *e = effectedE[ptrIdx];
		void *co = EDGE_getCo(e, nextLvl, 1);
		float sharpness = EDGE_getSharpness(e, curLvl);

		if (_edge_isBoundary(e) || sharpness>=1.0) {
			VertDataCopy(co, VERT_getCo(e->v0, curLvl));
			VertDataAdd(co, VERT_getCo(e->v1, curLvl));
			VertDataMulN(co, 0.5f);
		} else {
			int numFaces = 0;
			VertDataCopy(q, VERT_getCo(e->v0, curLvl));
			VertDataAdd(q, VERT_getCo(e->v1, curLvl));
			for (i=0; i<e->numFaces; i++) {
				CCFace *f = e->faces[i];
				VertDataAdd(q, FACE_getCenterData(f));
				numFaces++;
			}
			VertDataMulN(q, 1.0f/(2.0f+numFaces));

			VertDataCopy(r, VERT_getCo(e->v0, curLvl));
			VertDataAdd(r, VERT_getCo(e->v1, curLvl));
			VertDataMulN(r, 0.5f);

			VertDataCopy(co, q);
			VertDataSub(r, q);
			VertDataMulN(r, sharpness);
			VertDataAdd(co, r);
		}

		// edge flags cleared later
	}
	for (ptrIdx=0; ptrIdx<numEffectedV; ptrIdx++) {
		CCVert *v = effectedV[ptrIdx];
		void *co = VERT_getCo(v, curLvl);
		void *nCo = VERT_getCo(v, nextLvl);
		int sharpCount = 0, allSharp = 1;
		float avgSharpness = 0.0;
		int seam = VERT_seam(v), seamEdges = 0;

		for (i=0; i<v->numEdges; i++) {
			CCEdge *e = v->edges[i];
			float sharpness = EDGE_getSharpness(e, curLvl);

			if (seam && _edge_isBoundary(e))
				seamEdges++;

			if (sharpness!=0.0f) {
				sharpCount++;
				avgSharpness += sharpness;
			} else {
				allSharp = 0;
			}
		}

		if(sharpCount) {
			avgSharpness /= sharpCount;
			if (avgSharpness>1.0) {
				avgSharpness = 1.0;
			}
		}

		if (seam && seamEdges < 2)
			seam = 0;

		if (!v->numEdges) {
			VertDataCopy(nCo, co);
		} else if (_vert_isBoundary(v)) {
			int numBoundary = 0;

			VertDataZero(r);
			for (i=0; i<v->numEdges; i++) {
				CCEdge *e = v->edges[i];
				if (_edge_isBoundary(e)) {
					VertDataAdd(r, VERT_getCo(_edge_getOtherVert(e, v), curLvl));
					numBoundary++;
				}
			}
			VertDataCopy(nCo, co);
			VertDataMulN(nCo, 0.75);
			VertDataMulN(r, 0.25f/numBoundary);
			VertDataAdd(nCo, r);
		} else {
			int numEdges = 0, numFaces = 0;

			VertDataZero(q);
			for (i=0; i<v->numFaces; i++) {
				CCFace *f = v->faces[i];
				VertDataAdd(q, FACE_getCenterData(f));
				numFaces++;
			}
			VertDataMulN(q, 1.0f/numFaces);
			VertDataZero(r);
			for (i=0; i<v->numEdges; i++) {
				CCEdge *e = v->edges[i];
				VertDataAdd(r, VERT_getCo(_edge_getOtherVert(e, v), curLvl));
				numEdges++;
			}
			VertDataMulN(r, 1.0f/numEdges);

			VertDataCopy(nCo, co);
			VertDataMulN(nCo, numEdges-2.0f);
			VertDataAdd(nCo, q);
			VertDataAdd(nCo, r);
			VertDataMulN(nCo, 1.0f/numEdges);
		}

		if (sharpCount>1 || seam) {
			VertDataZero(q);

			if (seam) {
				avgSharpness = 1.0f;
				sharpCount = seamEdges;
				allSharp = 1;
			}

			for (i=0; i<v->numEdges; i++) {
				CCEdge *e = v->edges[i];
				float sharpness = EDGE_getSharpness(e, curLvl);

				if (seam) {
					if (_edge_isBoundary(e)) {
						CCVert *oV = _edge_getOtherVert(e, v);
						VertDataAdd(q, VERT_getCo(oV, curLvl));
					}
				} else if (sharpness != 0.0) {
					CCVert *oV = _edge_getOtherVert(e, v);
					VertDataAdd(q, VERT_getCo(oV, curLvl));
				}
			}

			VertDataMulN(q, (float) 1/sharpCount);

			if (sharpCount!=2 || allSharp) {
					// q = q + (co-q)*avgSharpness
				VertDataCopy(r, co);
				VertDataSub(r, q);
				VertDataMulN(r, avgSharpness);
				VertDataAdd(q, r);
			}

				// r = co*.75 + q*.25
			VertDataCopy(r, co);
			VertDataMulN(r, .75);
			VertDataMulN(q, .25);
			VertDataAdd(r, q);

				// nCo = nCo  + (r-nCo)*avgSharpness
			VertDataSub(r, nCo);
			VertDataMulN(r, avgSharpness);
			VertDataAdd(nCo, r);
		}

		// vert flags cleared later
	}

	if (ss->useAgeCounts) {
		for (i=0; i<numEffectedV; i++) {
			CCVert *v = effectedV[i];
			byte *userData = CCS_getVertUserData(ss, v);
			*((int*) &userData[ss->vertUserAgeOffset]) = ss->currentAge;
		}

		for (i=0; i<numEffectedE; i++) {
			CCEdge *e = effectedE[i];
			byte *userData = CCS_getEdgeUserData(ss, e);
			*((int*) &userData[ss->edgeUserAgeOffset]) = ss->currentAge;
		}

		for (i=0; i<numEffectedF; i++) {
			CCFace *f = effectedF[i];
			byte *userData = CCS_getFaceUserData(ss, f);
			*((int*) &userData[ss->faceUserAgeOffset]) = ss->currentAge;
		}
	}

	for (i=0; i<numEffectedE; i++) {
		CCEdge *e = effectedE[i];
		VertDataCopy(EDGE_getCo(e, nextLvl, 0), VERT_getCo(e->v0, nextLvl));
		VertDataCopy(EDGE_getCo(e, nextLvl, 2), VERT_getCo(e->v1, nextLvl));
	}
	for (i=0; i<numEffectedF; i++) {
		CCFace *f = effectedF[i];
		for (S=0; S<f->numVerts; S++) {
			CCEdge *e = FACE_getEdges(f)[S];
			CCEdge *prevE = FACE_getEdges(f)[(S+f->numVerts-1)%f->numVerts];

			VertDataCopy(FACE_getIFCo(f, nextLvl, S, 0, 0), FACE_getCenterData(f));
			VertDataCopy(FACE_getIECo(f, nextLvl, S, 0), FACE_getCenterData(f));
			VertDataCopy(FACE_getIFCo(f, nextLvl, S, 1, 1), VERT_getCo(FACE_getVerts(f)[S], nextLvl));
			VertDataCopy(FACE_getIECo(f, nextLvl, S, 1), EDGE_getCo(FACE_getEdges(f)[S], nextLvl, 1));

			VertDataCopy(FACE_getIFCo(f, nextLvl, S, 1, 0), _edge_getCoVert(e, FACE_getVerts(f)[S], nextLvl, 1, vertDataSize));
			VertDataCopy(FACE_getIFCo(f, nextLvl, S, 0, 1), _edge_getCoVert(prevE, FACE_getVerts(f)[S], nextLvl, 1, vertDataSize));
		}
	}

	for (curLvl=1; curLvl<subdivLevels; curLvl++) {
		int edgeSize = 1 + (1<<curLvl);
		int gridSize = 1 + (1<<(curLvl-1));
		nextLvl = curLvl+1;

		for (ptrIdx=0; ptrIdx<numEffectedF; ptrIdx++) {
			CCFace *f = (CCFace*) effectedF[ptrIdx];

				/* interior face midpoints
				 *  o old interior face points
				 */
			for (S=0; S<f->numVerts; S++) {
				for (y=0; y<gridSize-1; y++) {
					for (x=0; x<gridSize-1; x++) {
						int fx = 1 + 2*x;
						int fy = 1 + 2*y;
						void *co0 = FACE_getIFCo(f, curLvl, S, x+0, y+0);
						void *co1 = FACE_getIFCo(f, curLvl, S, x+1, y+0);
						void *co2 = FACE_getIFCo(f, curLvl, S, x+1, y+1);
						void *co3 = FACE_getIFCo(f, curLvl, S, x+0, y+1);
						void *co = FACE_getIFCo(f, nextLvl, S, fx, fy);

						VertDataAvg4(co, co0, co1, co2, co3);
					}
				}
			}

				/* interior edge midpoints
				 *  o old interior edge points
				 *  o new interior face midpoints
				 */
			for (S=0; S<f->numVerts; S++) {
				for (x=0; x<gridSize-1; x++) {
					int fx = x*2 + 1;
					void *co0 = FACE_getIECo(f, curLvl, S, x+0);
					void *co1 = FACE_getIECo(f, curLvl, S, x+1);
					void *co2 = FACE_getIFCo(f, nextLvl, (S+1)%f->numVerts, 1, fx);
					void *co3 = FACE_getIFCo(f, nextLvl, S, fx, 1);
					void *co = FACE_getIECo(f, nextLvl, S, fx);
					
					VertDataAvg4(co, co0, co1, co2, co3);
				}

						/* interior face interior edge midpoints
						 *  o old interior face points
						 *  o new interior face midpoints
						 */

					/* vertical */
				for (x=1; x<gridSize-1; x++) {
					for (y=0; y<gridSize-1; y++) {
						int fx = x*2;
						int fy = y*2+1;
						void *co0 = FACE_getIFCo(f, curLvl, S, x, y+0);
						void *co1 = FACE_getIFCo(f, curLvl, S, x, y+1);
						void *co2 = FACE_getIFCo(f, nextLvl, S, fx-1, fy);
						void *co3 = FACE_getIFCo(f, nextLvl, S, fx+1, fy);
						void *co = FACE_getIFCo(f, nextLvl, S, fx, fy);

						VertDataAvg4(co, co0, co1, co2, co3);
					}
				}

					/* horizontal */
				for (y=1; y<gridSize-1; y++) {
					for (x=0; x<gridSize-1; x++) {
						int fx = x*2+1;
						int fy = y*2;
						void *co0 = FACE_getIFCo(f, curLvl, S, x+0, y);
						void *co1 = FACE_getIFCo(f, curLvl, S, x+1, y);
						void *co2 = FACE_getIFCo(f, nextLvl, S, fx, fy-1);
						void *co3 = FACE_getIFCo(f, nextLvl, S, fx, fy+1);
						void *co = FACE_getIFCo(f, nextLvl, S, fx, fy);

						VertDataAvg4(co, co0, co1, co2, co3);
					}
				}
			}
		}

			/* exterior edge midpoints
			 *  o old exterior edge points
			 *  o new interior face midpoints
			 */
		for (ptrIdx=0; ptrIdx<numEffectedE; ptrIdx++) {
			CCEdge *e = (CCEdge*) effectedE[ptrIdx];
			float sharpness = EDGE_getSharpness(e, curLvl);

			if (_edge_isBoundary(e) || sharpness>1.0) {
				for (x=0; x<edgeSize-1; x++) {
					int fx = x*2 + 1;
					void *co0 = EDGE_getCo(e, curLvl, x+0);
					void *co1 = EDGE_getCo(e, curLvl, x+1);
					void *co = EDGE_getCo(e, nextLvl, fx);

					VertDataCopy(co, co0);
					VertDataAdd(co, co1);
					VertDataMulN(co, 0.5);
				}
			} else {
				for (x=0; x<edgeSize-1; x++) {
					int fx = x*2 + 1;
					void *co0 = EDGE_getCo(e, curLvl, x+0);
					void *co1 = EDGE_getCo(e, curLvl, x+1);
					void *co = EDGE_getCo(e, nextLvl, fx);
					int numFaces = 0;

					VertDataCopy(q, co0);
					VertDataAdd(q, co1);

					for (i=0; i<e->numFaces; i++) {
						CCFace *f = e->faces[i];
						VertDataAdd(q, _face_getIFCoEdge(f, e, nextLvl, fx, 1, subdivLevels, vertDataSize));
						numFaces++;
					}

					VertDataMulN(q, 1.0f/(2.0f+numFaces));

					VertDataCopy(r, co0);
					VertDataAdd(r, co1);
					VertDataMulN(r, 0.5);

					VertDataCopy(co, q);
					VertDataSub(r, q);
					VertDataMulN(r, sharpness);
					VertDataAdd(co, r);
				}
			}
		}

			/* exterior vertex shift
			 *  o old vertex points (shifting)
			 *  o old exterior edge points
			 *  o new interior face midpoints
			 */
		for (ptrIdx=0; ptrIdx<numEffectedV; ptrIdx++) {
			CCVert *v = (CCVert*) effectedV[ptrIdx];
			void *co = VERT_getCo(v, curLvl);
			void *nCo = VERT_getCo(v, nextLvl);
			int sharpCount = 0, allSharp = 1;
			float avgSharpness = 0.0;
			int seam = VERT_seam(v), seamEdges = 0;

			for (i=0; i<v->numEdges; i++) {
				CCEdge *e = v->edges[i];
				float sharpness = EDGE_getSharpness(e, curLvl);

				if (seam && _edge_isBoundary(e))
					seamEdges++;

				if (sharpness!=0.0f) {
					sharpCount++;
					avgSharpness += sharpness;
				} else {
					allSharp = 0;
				}
			}

			if(sharpCount) {
				avgSharpness /= sharpCount;
				if (avgSharpness>1.0) {
					avgSharpness = 1.0;
				}
			}

			if (seam && seamEdges < 2)
				seam = 0;

			if (!v->numEdges) {
				VertDataCopy(nCo, co);
			} else if (_vert_isBoundary(v)) {
				int numBoundary = 0;

				VertDataZero(r);
				for (i=0; i<v->numEdges; i++) {
					CCEdge *e = v->edges[i];
					if (_edge_isBoundary(e)) {
						VertDataAdd(r, _edge_getCoVert(e, v, curLvl, 1, vertDataSize));
						numBoundary++;
					}
				}

				VertDataCopy(nCo, co);
				VertDataMulN(nCo, 0.75);
				VertDataMulN(r, 0.25f/numBoundary);
				VertDataAdd(nCo, r);
			} else {
				int cornerIdx = (1 + (1<<(curLvl))) - 2;
				int numEdges = 0, numFaces = 0;

				VertDataZero(q);
				for (i=0; i<v->numFaces; i++) {
					CCFace *f = v->faces[i];
					VertDataAdd(q, FACE_getIFCo(f, nextLvl, _face_getVertIndex(f,v), cornerIdx, cornerIdx));
					numFaces++;
				}
				VertDataMulN(q, 1.0f/numFaces);
				VertDataZero(r);
				for (i=0; i<v->numEdges; i++) {
					CCEdge *e = v->edges[i];
					VertDataAdd(r, _edge_getCoVert(e, v, curLvl, 1,vertDataSize));
					numEdges++;
				}
				VertDataMulN(r, 1.0f/numEdges);

				VertDataCopy(nCo, co);
				VertDataMulN(nCo, numEdges-2.0f);
				VertDataAdd(nCo, q);
				VertDataAdd(nCo, r);
				VertDataMulN(nCo, 1.0f/numEdges);
			}

			if ((sharpCount>1 && v->numFaces) || seam) {
				VertDataZero(q);

				if (seam) {
					avgSharpness = 1.0f;
					sharpCount = seamEdges;
					allSharp = 1;
				}

				for (i=0; i<v->numEdges; i++) {
					CCEdge *e = v->edges[i];
					float sharpness = EDGE_getSharpness(e, curLvl);

					if (seam) {
						if (_edge_isBoundary(e))
							VertDataAdd(q, _edge_getCoVert(e, v, curLvl, 1, vertDataSize));
					} else if (sharpness != 0.0) {
						VertDataAdd(q, _edge_getCoVert(e, v, curLvl, 1, vertDataSize));
					}
				}

				VertDataMulN(q, (float) 1/sharpCount);

				if (sharpCount!=2 || allSharp) {
						// q = q + (co-q)*avgSharpness
					VertDataCopy(r, co);
					VertDataSub(r, q);
					VertDataMulN(r, avgSharpness);
					VertDataAdd(q, r);
				}

					// r = co*.75 + q*.25
				VertDataCopy(r, co);
				VertDataMulN(r, .75);
				VertDataMulN(q, .25);
				VertDataAdd(r, q);

					// nCo = nCo  + (r-nCo)*avgSharpness
				VertDataSub(r, nCo);
				VertDataMulN(r, avgSharpness);
				VertDataAdd(nCo, r);
			}
		}

			/* exterior edge interior shift
			 *  o old exterior edge midpoints (shifting)
			 *  o old exterior edge midpoints
			 *  o new interior face midpoints
			 */
		for (ptrIdx=0; ptrIdx<numEffectedE; ptrIdx++) {
			CCEdge *e = (CCEdge*) effectedE[ptrIdx];
			float sharpness = EDGE_getSharpness(e, curLvl);
			int sharpCount = 0;
			float avgSharpness = 0.0;

			if (sharpness!=0.0f) {
				sharpCount = 2;
				avgSharpness += sharpness;

				if (avgSharpness>1.0) {
					avgSharpness = 1.0;
				}
			} else {
				sharpCount = 0;
				avgSharpness = 0;
			}

			if (_edge_isBoundary(e) && (!e->numFaces || sharpCount<2)) {
				for (x=1; x<edgeSize-1; x++) {
					int fx = x*2;
					void *co = EDGE_getCo(e, curLvl, x);
					void *nCo = EDGE_getCo(e, nextLvl, fx);
					VertDataCopy(r, EDGE_getCo(e, curLvl, x-1));
					VertDataAdd(r, EDGE_getCo(e, curLvl, x+1));
					VertDataMulN(r, 0.5);
					VertDataCopy(nCo, co);
					VertDataMulN(nCo, 0.75);
					VertDataMulN(r, 0.25);
					VertDataAdd(nCo, r);
				}
			} else {
				for (x=1; x<edgeSize-1; x++) {
					int fx = x*2;
					void *co = EDGE_getCo(e, curLvl, x);
					void *nCo = EDGE_getCo(e, nextLvl, fx);
					int numFaces = 0;

					VertDataZero(q);
					VertDataZero(r);
					VertDataAdd(r, EDGE_getCo(e, curLvl, x-1));
					VertDataAdd(r, EDGE_getCo(e, curLvl, x+1));
					for (i=0; i<e->numFaces; i++) {
						CCFace *f = e->faces[i];
						VertDataAdd(q, _face_getIFCoEdge(f, e, nextLvl, fx-1, 1, subdivLevels, vertDataSize));
						VertDataAdd(q, _face_getIFCoEdge(f, e, nextLvl, fx+1, 1, subdivLevels, vertDataSize));

						VertDataAdd(r, _face_getIFCoEdge(f, e, curLvl, x, 1, subdivLevels, vertDataSize));
						numFaces++;
					}
					VertDataMulN(q, 1.0/(numFaces*2.0f));
					VertDataMulN(r, 1.0/(2.0f + numFaces));

					VertDataCopy(nCo, co);
					VertDataMulN(nCo, (float) numFaces);
					VertDataAdd(nCo, q);
					VertDataAdd(nCo, r);
					VertDataMulN(nCo, 1.0f/(2+numFaces));

					if (sharpCount==2) {
						VertDataCopy(q, co);
						VertDataMulN(q, 6.0f);
						VertDataAdd(q, EDGE_getCo(e, curLvl, x-1));
						VertDataAdd(q, EDGE_getCo(e, curLvl, x+1));
						VertDataMulN(q, 1/8.0f);

						VertDataSub(q, nCo);
						VertDataMulN(q, avgSharpness);
						VertDataAdd(nCo, q);
					}
				}
			}
		}

		for (ptrIdx=0; ptrIdx<numEffectedF; ptrIdx++) {
			CCFace *f = (CCFace*) effectedF[ptrIdx];

				/* interior center point shift
				 *  o old face center point (shifting)
				 *  o old interior edge points
				 *  o new interior face midpoints
				 */
			VertDataZero(q);
			for (S=0; S<f->numVerts; S++) {
				VertDataAdd(q, FACE_getIFCo(f, nextLvl, S, 1, 1));
			}
			VertDataMulN(q, 1.0f/f->numVerts);
			VertDataZero(r);
			for (S=0; S<f->numVerts; S++) {
				VertDataAdd(r, FACE_getIECo(f, curLvl, S, 1));
			}
			VertDataMulN(r, 1.0f/f->numVerts);

			VertDataMulN(FACE_getCenterData(f), f->numVerts-2.0f);
			VertDataAdd(FACE_getCenterData(f), q);
			VertDataAdd(FACE_getCenterData(f), r);
			VertDataMulN(FACE_getCenterData(f), 1.0f/f->numVerts);

			for (S=0; S<f->numVerts; S++) {
					/* interior face shift
					 *  o old interior face point (shifting)
					 *  o new interior edge midpoints
					 *  o new interior face midpoints
					 */
				for (x=1; x<gridSize-1; x++) {
					for (y=1; y<gridSize-1; y++) {
						int fx = x*2;
						int fy = y*2;
						void *co = FACE_getIFCo(f, curLvl, S, x, y);
						void *nCo = FACE_getIFCo(f, nextLvl, S, fx, fy);
						
						VertDataAvg4(q, FACE_getIFCo(f, nextLvl, S, fx-1, fy-1),
							FACE_getIFCo(f, nextLvl, S, fx+1, fy-1),
							FACE_getIFCo(f, nextLvl, S, fx+1, fy+1),
							FACE_getIFCo(f, nextLvl, S, fx-1, fy+1));

						VertDataAvg4(r, FACE_getIFCo(f, nextLvl, S, fx-1, fy+0),
							FACE_getIFCo(f, nextLvl, S, fx+1, fy+0),
							FACE_getIFCo(f, nextLvl, S, fx+0, fy-1),
							FACE_getIFCo(f, nextLvl, S, fx+0, fy+1));

						VertDataCopy(nCo, co);
						VertDataSub(nCo, q);
						VertDataMulN(nCo, 0.25f);
						VertDataAdd(nCo, r);
					}
				}

					/* interior edge interior shift
					 *  o old interior edge point (shifting)
					 *  o new interior edge midpoints
					 *  o new interior face midpoints
					 */
				for (x=1; x<gridSize-1; x++) {
					int fx = x*2;
					void *co = FACE_getIECo(f, curLvl, S, x);
					void *nCo = FACE_getIECo(f, nextLvl, S, fx);
					
					VertDataAvg4(q, FACE_getIFCo(f, nextLvl, (S+1)%f->numVerts, 1, fx-1),
						FACE_getIFCo(f, nextLvl, (S+1)%f->numVerts, 1, fx+1),
						FACE_getIFCo(f, nextLvl, S, fx+1, +1),
						FACE_getIFCo(f, nextLvl, S, fx-1, +1));

					VertDataAvg4(r, FACE_getIECo(f, nextLvl, S, fx-1),
						FACE_getIECo(f, nextLvl, S, fx+1),
						FACE_getIFCo(f, nextLvl, (S+1)%f->numVerts, 1, fx),
						FACE_getIFCo(f, nextLvl, S, fx, 1));

					VertDataCopy(nCo, co);
					VertDataSub(nCo, q);
					VertDataMulN(nCo, 0.25f);
					VertDataAdd(nCo, r);
				}
			}
		}

			/* copy down */
		edgeSize = 1 + (1<<(nextLvl));
		gridSize = 1 + (1<<((nextLvl)-1));
		cornerIdx = gridSize-1;
		for (i=0; i<numEffectedE; i++) {
			CCEdge *e = effectedE[i];
			VertDataCopy(EDGE_getCo(e, nextLvl, 0), VERT_getCo(e->v0, nextLvl));
			VertDataCopy(EDGE_getCo(e, nextLvl, edgeSize-1), VERT_getCo(e->v1, nextLvl));
		}
		for (i=0; i<numEffectedF; i++) {
			CCFace *f = effectedF[i];
			for (S=0; S<f->numVerts; S++) {
				CCEdge *e = FACE_getEdges(f)[S];
				CCEdge *prevE = FACE_getEdges(f)[(S+f->numVerts-1)%f->numVerts];

				VertDataCopy(FACE_getIFCo(f, nextLvl, S, 0, 0), FACE_getCenterData(f));
				VertDataCopy(FACE_getIECo(f, nextLvl, S, 0), FACE_getCenterData(f));
				VertDataCopy(FACE_getIFCo(f, nextLvl, S, cornerIdx, cornerIdx), VERT_getCo(FACE_getVerts(f)[S], nextLvl));
				VertDataCopy(FACE_getIECo(f, nextLvl, S, cornerIdx), EDGE_getCo(FACE_getEdges(f)[S], nextLvl, cornerIdx));
				for (x=1; x<gridSize-1; x++) {
					void *co = FACE_getIECo(f, nextLvl, S, x);
					VertDataCopy(FACE_getIFCo(f, nextLvl, S, x, 0), co);
					VertDataCopy(FACE_getIFCo(f, nextLvl, (S+1)%f->numVerts, 0, x), co);
				}
				for (x=0; x<gridSize-1; x++) {
					int eI = gridSize-1-x;
					VertDataCopy(FACE_getIFCo(f, nextLvl, S, cornerIdx, x), _edge_getCoVert(e, FACE_getVerts(f)[S], nextLvl, eI,vertDataSize));
					VertDataCopy(FACE_getIFCo(f, nextLvl, S, x, cornerIdx), _edge_getCoVert(prevE, FACE_getVerts(f)[S], nextLvl, eI,vertDataSize));
				}
			}
		}
	}

#define FACE_getIFNo(f, lvl, S, x, y)		_face_getIFNo(f, lvl, S, x, y, subdivLevels, vertDataSize, normalDataOffset)
#define FACE_calcIFNo(f, lvl, S, x, y, no)	_face_calcIFNo(f, lvl, S, x, y, no, subdivLevels, vertDataSize)
	if (ss->calcVertNormals) {
		int lvl = ss->subdivLevels;
		int edgeSize = 1 + (1<<lvl);
		int gridSize = 1 + (1<<(lvl-1));
		int normalDataOffset = ss->normalDataOffset;

		for (ptrIdx=0; ptrIdx<numEffectedF; ptrIdx++) {
			CCFace *f = (CCFace*) effectedF[ptrIdx];
			int S, x, y;

			for (S=0; S<f->numVerts; S++) {
				for (y=0; y<gridSize-1; y++)
					for (x=0; x<gridSize-1; x++)
						NormZero(FACE_getIFNo(f, lvl, S, x, y));

				if (FACE_getEdges(f)[(S-1+f->numVerts)%f->numVerts]->flags&Edge_eEffected)
					for (x=0; x<gridSize-1; x++)
						NormZero(FACE_getIFNo(f, lvl, S, x, gridSize-1));
				if (FACE_getEdges(f)[S]->flags&Edge_eEffected)
					for (y=0; y<gridSize-1; y++)
						NormZero(FACE_getIFNo(f, lvl, S, gridSize-1, y));
				if (FACE_getVerts(f)[S]->flags&Vert_eEffected)
					NormZero(FACE_getIFNo(f, lvl, S, gridSize-1, gridSize-1));
			}
		}

		for (ptrIdx=0; ptrIdx<numEffectedF; ptrIdx++) {
			CCFace *f = (CCFace*) effectedF[ptrIdx];
			int S, x, y;
			float no[3];

			for (S=0; S<f->numVerts; S++) {
				int yLimit = !(FACE_getEdges(f)[(S-1+f->numVerts)%f->numVerts]->flags&Edge_eEffected);
				int xLimit = !(FACE_getEdges(f)[S]->flags&Edge_eEffected);
				int yLimitNext = xLimit;
				int xLimitPrev = yLimit;
				
				for (y=0; y<gridSize - 1; y++) {
					for (x=0; x<gridSize - 1; x++) {
						int xPlusOk = (!xLimit || x<gridSize-2);
						int yPlusOk = (!yLimit || y<gridSize-2);

						FACE_calcIFNo(f, lvl, S, x, y, no);

						NormAdd(FACE_getIFNo(f, lvl, S, x+0, y+0), no);
						if (xPlusOk)
							NormAdd(FACE_getIFNo(f, lvl, S, x+1, y+0), no);
						if (yPlusOk)
							NormAdd(FACE_getIFNo(f, lvl, S, x+0, y+1), no);
						if (xPlusOk && yPlusOk) {
							if (x<gridSize-2 || y<gridSize-2 || FACE_getVerts(f)[S]->flags&Vert_eEffected) {
								NormAdd(FACE_getIFNo(f, lvl, S, x+1, y+1), no);
							}
						}

						if (x==0 && y==0) {
							int K;

							if (!yLimitNext || 1<gridSize-1)
								NormAdd(FACE_getIFNo(f, lvl, (S+1)%f->numVerts, 0, 1), no);
							if (!xLimitPrev || 1<gridSize-1)
								NormAdd(FACE_getIFNo(f, lvl, (S-1+f->numVerts)%f->numVerts, 1, 0), no);

							for (K=0; K<f->numVerts; K++) {
								if (K!=S) {
									NormAdd(FACE_getIFNo(f, lvl, K, 0, 0), no);
								}
							}
						} else if (y==0) {
							NormAdd(FACE_getIFNo(f, lvl, (S+1)%f->numVerts, 0, x), no);
							if (!yLimitNext || x<gridSize-2)
								NormAdd(FACE_getIFNo(f, lvl, (S+1)%f->numVerts, 0, x+1), no);
						} else if (x==0) {
							NormAdd(FACE_getIFNo(f, lvl, (S-1+f->numVerts)%f->numVerts, y, 0), no);
							if (!xLimitPrev || y<gridSize-2)
								NormAdd(FACE_getIFNo(f, lvl, (S-1+f->numVerts)%f->numVerts, y+1, 0), no);
						}
					}
				}
			}
		}
			// XXX can I reduce the number of normalisations here?
		for (ptrIdx=0; ptrIdx<numEffectedV; ptrIdx++) {
			CCVert *v = (CCVert*) effectedV[ptrIdx];
			float length, *no = _vert_getNo(v, lvl, vertDataSize, normalDataOffset);

			NormZero(no);

			for (i=0; i<v->numFaces; i++) {
				CCFace *f = v->faces[i];
				NormAdd(no, FACE_getIFNo(f, lvl, _face_getVertIndex(f,v), gridSize-1, gridSize-1));
			}

			length = sqrt(no[0]*no[0] + no[1]*no[1] + no[2]*no[2]);

			if (length>EPSILON) {
				float invLength = 1.0f/length;
				no[0] *= invLength;
				no[1] *= invLength;
				no[2] *= invLength;
			} else {
				NormZero(no);
			}

			for (i=0; i<v->numFaces; i++) {
				CCFace *f = v->faces[i];
				NormCopy(FACE_getIFNo(f, lvl, _face_getVertIndex(f,v), gridSize-1, gridSize-1), no);
			}
		}
		for (ptrIdx=0; ptrIdx<numEffectedE; ptrIdx++) {
			CCEdge *e = (CCEdge*) effectedE[ptrIdx];

			if (e->numFaces) {
				CCFace *fLast = e->faces[e->numFaces-1];
				int x;

				for (i=0; i<e->numFaces-1; i++) {
					CCFace *f = e->faces[i];

					for (x=1; x<edgeSize-1; x++) {
						NormAdd(_face_getIFNoEdge(fLast, e, lvl, x, 0, subdivLevels, vertDataSize, normalDataOffset),
								_face_getIFNoEdge(f, e, lvl, x, 0, subdivLevels, vertDataSize, normalDataOffset));
					}
				}

				for (i=0; i<e->numFaces-1; i++) {
					CCFace *f = e->faces[i];

					for (x=1; x<edgeSize-1; x++) {
						NormCopy(_face_getIFNoEdge(f, e, lvl, x, 0, subdivLevels, vertDataSize, normalDataOffset),
								_face_getIFNoEdge(fLast, e, lvl, x, 0, subdivLevels, vertDataSize, normalDataOffset));
					}
				}
			}
		}
		for (ptrIdx=0; ptrIdx<numEffectedF; ptrIdx++) {
			CCFace *f = (CCFace*) effectedF[ptrIdx];
			int S;

			for (S=0; S<f->numVerts; S++) {
				NormCopy(FACE_getIFNo(f, lvl, (S+1)%f->numVerts, 0, gridSize-1),
						 FACE_getIFNo(f, lvl, S, gridSize-1, 0));
			}
		}
		for (ptrIdx=0; ptrIdx<numEffectedF; ptrIdx++) {
			CCFace *f = (CCFace*) effectedF[ptrIdx];
			int S, x, y;

			for (S=0; S<f->numVerts; S++) {
				for (y=0; y<gridSize; y++) {
					for (x=0; x<gridSize; x++) {
						float *no = FACE_getIFNo(f, lvl, S, x, y);
						float length = sqrt(no[0]*no[0] + no[1]*no[1] + no[2]*no[2]);

						if (length>EPSILON) {
							float invLength = 1.0f/length;
							no[0] *= invLength;
							no[1] *= invLength;
							no[2] *= invLength;
						} else {
							NormZero(no);
						}
					}
				}
			}
		}
	}
#undef FACE_getIFNo

	for (ptrIdx=0; ptrIdx<numEffectedV; ptrIdx++) {
		CCVert *v = effectedV[ptrIdx];
		v->flags = 0;
	}
	for (ptrIdx=0; ptrIdx<numEffectedE; ptrIdx++) {
		CCEdge *e = effectedE[ptrIdx];
		e->flags = 0;
	}

#undef VERT_getCo
#undef EDGE_getCo
#undef FACE_getIECo
#undef FACE_getIFCo

	CCGSUBSURF_free(ss, effectedF);
	CCGSUBSURF_free(ss, effectedE);
	CCGSUBSURF_free(ss, effectedV);
}

/*** External API accessor functions ***/

int CCS_getNumVerts(CSubSurf *ss) {
	return ss->vMap->numEntries;
}
int CCS_getNumEdges(CSubSurf *ss) {
	return ss->eMap->numEntries;
}
int CCS_getNumFaces(CSubSurf *ss) {
	return ss->fMap->numEntries;
}

CCVert *CCS_getVert(CSubSurf *ss, CCVertHDL v) {
	return (CCVert*) _ehash_lookup(ss->vMap, v);
}
CCEdge *CCS_getEdge(CSubSurf *ss, CCEdgeHDL e) {
	return (CCEdge*) _ehash_lookup(ss->eMap, e);
}
CCFace *CCS_getFace(CSubSurf *ss, CCFaceHDL f) {
	return (CCFace*) _ehash_lookup(ss->fMap, f);
}

int CCS_getSubdivisionLevels(CSubSurf *ss) {
	return ss->subdivLevels;
}
int CCS_getEdgeSize(CSubSurf *ss) {
	return CCS_getEdgeLevelSize(ss, ss->subdivLevels);
}
int CCS_getEdgeLevelSize(CSubSurf *ss, int level) {
	if (level<1 || level>ss->subdivLevels) {
		return -1;
	} else {
		return 1 + (1<<level);
	}
}
int CCS_getGridSize(CSubSurf *ss) {
	return CCS_getGridLevelSize(ss, ss->subdivLevels);
}
int CCS_getGridLevelSize(CSubSurf *ss, int level) {
	if (level<1 || level>ss->subdivLevels) {
		return -1;
	} else {
		return 1 + (1<<(level-1));
	}
}

/* Vert accessors */

CCVertHDL CCS_getVertVertHandle(CCVert *v) {
	return v->vHDL;
}
int CCS_getVertAge(CSubSurf *ss, CCVert *v) {
	if (ss->useAgeCounts) {
		byte *userData = CCS_getVertUserData(ss, v);
		return ss->currentAge - *((int*) &userData[ss->vertUserAgeOffset]);
	} else {
		return 0;
	}
}
void *CCS_getVertUserData(CSubSurf *ss, CCVert *v) {
	return VERT_getLevelData(v) + ss->meshIFC.vertDataSize*(ss->subdivLevels+1);
}
int CCS_getVertNumFaces(CCVert *v) {
	return v->numFaces;
}
CCFace *CCS_getVertFace(CCVert *v, int index) {
	if (index<0 || index>=v->numFaces) {
		return NULL;
	} else {
		return v->faces[index];
	}
}
int CCS_getVertNumEdges(CCVert *v) {
	return v->numEdges;
}
CCEdge *CCS_getVertEdge(CCVert *v, int index) {
	if (index<0 || index>=v->numEdges) {
		return NULL;
	} else {
		return v->edges[index];
	}
}
void *CCS_getVertData(CSubSurf *ss, CCVert *v) {
	return CCS_getVertLevelData(ss, v, ss->subdivLevels);
}
void *CCS_getVertLevelData(CSubSurf *ss, CCVert *v, int level) {
	if (level<0 || level>ss->subdivLevels) {
		return NULL;
	} else {
		return _vert_getCo(v, level, ss->meshIFC.vertDataSize);
	}
}

/* Edge accessors */

CCEdgeHDL CCS_getEdgeEdgeHandle(CCEdge *e) {
	return e->eHDL;
}
int CCS_getEdgeAge(CSubSurf *ss, CCEdge *e) {
	if (ss->useAgeCounts) {
		byte *userData = CCS_getEdgeUserData(ss, e);
		return ss->currentAge - *((int*) &userData[ss->edgeUserAgeOffset]);
	} else {
		return 0;
	}
}
void *CCS_getEdgeUserData(CSubSurf *ss, CCEdge *e) {
	return EDGE_getLevelData(e) + ss->meshIFC.vertDataSize *((ss->subdivLevels+1) + (1<<(ss->subdivLevels+1))-1);
}
int CCS_getEdgeNumFaces(CCEdge *e) {
	return e->numFaces;
}
CCFace *CCS_getEdgeFace(CCEdge *e, int index) {
	if (index<0 || index>=e->numFaces) {
		return NULL;
	} else {
		return e->faces[index];
	}
}
CCVert *CCS_getEdgeVert0(CCEdge *e) {
	return e->v0;
}
CCVert *CCS_getEdgeVert1(CCEdge *e) {
	return e->v1;
}
void *CCS_getEdgeDataArray(CSubSurf *ss, CCEdge *e) {
	return CCS_getEdgeData(ss, e, 0);
}
void *CCS_getEdgeData(CSubSurf *ss, CCEdge *e, int x) {
	return CCS_getEdgeLevelData(ss, e, x, ss->subdivLevels);
}
void *CCS_getEdgeLevelData(CSubSurf *ss, CCEdge *e, int x, int level) {
	if (level<0 || level>ss->subdivLevels) {
		return NULL;
	} else {
		return _edge_getCo(e, level, x, ss->meshIFC.vertDataSize);
	}
}
float CCS_getEdgeCrease(CCEdge *e) {
	return e->crease;
}

/* Face accessors */

CCFaceHDL CCS_getFaceFaceHandle(CSubSurf *ss, CCFace *f) {
	return f->fHDL;
}
int CCS_getFaceAge(CSubSurf *ss, CCFace *f) {
	if (ss->useAgeCounts) {
		byte *userData = CCS_getFaceUserData(ss, f);
		return ss->currentAge - *((int*) &userData[ss->faceUserAgeOffset]);
	} else {
		return 0;
	}
}
void *CCS_getFaceUserData(CSubSurf *ss, CCFace *f) {
	int maxGridSize = 1 + (1<<(ss->subdivLevels-1));
	return FACE_getCenterData(f) + ss->meshIFC.vertDataSize *(1 + f->numVerts*maxGridSize + f->numVerts*maxGridSize*maxGridSize);
}
int CCS_getFaceNumVerts(CCFace *f) {
	return f->numVerts;
}
CCVert *CCS_getFaceVert(CSubSurf *ss, CCFace *f, int index) {
	if (index<0 || index>=f->numVerts) {
		return NULL;
	} else {
		return FACE_getVerts(f)[index];
	}
}
CCEdge *CCS_getFaceEdge(CSubSurf *ss, CCFace *f, int index) {
	if (index<0 || index>=f->numVerts) {
		return NULL;
	} else {
		return FACE_getEdges(f)[index];
	}
}
int CCS_getFaceEdgeIndex(CCFace *f, CCEdge *e) {
	int i;

	for (i=0; i<f->numVerts; i++)
		if (FACE_getEdges(f)[i]==e)
			return i;

	return -1;
}
void *CCS_getFaceCenterData(CCFace *f) {
	return FACE_getCenterData(f);
}
void *CCS_getFaceGridEdgeDataArray(CSubSurf *ss, CCFace *f, int gridIndex) {
	return CCS_getFaceGridEdgeData(ss, f, gridIndex, 0);
}
void *CCS_getFaceGridEdgeData(CSubSurf *ss, CCFace *f, int gridIndex, int x) {
	return _face_getIECo(f, ss->subdivLevels, gridIndex, x, ss->subdivLevels, ss->meshIFC.vertDataSize);
}
void *CCS_getFaceGridDataArray(CSubSurf *ss, CCFace *f, int gridIndex) {
	return CCS_getFaceGridData(ss, f, gridIndex, 0, 0);
}
void *CCS_getFaceGridData(CSubSurf *ss, CCFace *f, int gridIndex, int x, int y) {
	return _face_getIFCo(f, ss->subdivLevels, gridIndex, x, y, ss->subdivLevels, ss->meshIFC.vertDataSize);
}

/*** External API iterator functions ***/

CCVertIterator *CCS_getVertIterator(CSubSurf *ss) {
	return (CCVertIterator*) _ehashIterator_new(ss->vMap);
}
CCEdgeIterator *CCS_getEdgeIterator(CSubSurf *ss) {
	return (CCEdgeIterator*) _ehashIterator_new(ss->eMap);
}
CCFaceIterator *CCS_getFaceIterator(CSubSurf *ss) {
	return (CCFaceIterator*) _ehashIterator_new(ss->fMap);
}

CCVert *CCVIter_getCurrent(CCVertIterator *vi) {
	return (CCVert*) _ehashIterator_getCurrent((EHashIterator*) vi);
}
int CCVIter_isStopped(CCVertIterator *vi) {
	return _ehashIterator_isStopped((EHashIterator*) vi);
}
void CCVIter_next(CCVertIterator *vi) {
	_ehashIterator_next((EHashIterator*) vi); 
}
void CCVIter_free(CCVertIterator *vi) {
	_ehashIterator_free((EHashIterator*) vi);
}

CCEdge *CCEIter_getCurrent(CCEdgeIterator *vi) {
	return (CCEdge*) _ehashIterator_getCurrent((EHashIterator*) vi);
}
int CCEIter_isStopped(CCEdgeIterator *vi) {
	return _ehashIterator_isStopped((EHashIterator*) vi);
}
void CCEIter_next(CCEdgeIterator *vi) {
	_ehashIterator_next((EHashIterator*) vi); 
}
void CCEIter_free(CCEdgeIterator *vi) {
	_ehashIterator_free((EHashIterator*) vi);
}

CCFace *CCFIter_getCurrent(CCFaceIterator *vi) {
	return (CCFace*) _ehashIterator_getCurrent((EHashIterator*) vi);
}
int CCFIter_isStopped(CCFaceIterator *vi) {
	return _ehashIterator_isStopped((EHashIterator*) vi);
}
void CCFIter_next(CCFaceIterator *vi) {
	_ehashIterator_next((EHashIterator*) vi); 
}
void CCFIter_free(CCFaceIterator *vi) {
	_ehashIterator_free((EHashIterator*) vi);
}

/*** Extern API final vert/edge/face interface ***/

int CCS_getNumFinalVerts(CSubSurf *ss) {
	int edgeSize = 1 + (1<<ss->subdivLevels);
	int gridSize = 1 + (1<<(ss->subdivLevels-1));
	int numFinalVerts = ss->vMap->numEntries + ss->eMap->numEntries*(edgeSize-2) + ss->fMap->numEntries + ss->numGrids*((gridSize-2) + ((gridSize-2)*(gridSize-2)));
	return numFinalVerts;
}
int CCS_getNumFinalEdges(CSubSurf *ss) {
	int edgeSize = 1 + (1<<ss->subdivLevels);
	int gridSize = 1 + (1<<(ss->subdivLevels-1));
	int numFinalEdges = ss->eMap->numEntries*(edgeSize-1) + ss->numGrids*((gridSize-1) + 2*((gridSize-2)*(gridSize-1)));
	return numFinalEdges;
}
int CCS_getNumFinalFaces(CSubSurf *ss) {
	int gridSize = 1 + (1<<(ss->subdivLevels-1));
	int numFinalFaces = ss->numGrids*((gridSize-1)*(gridSize-1));
	return numFinalFaces;
}
