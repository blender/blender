/* $Id$ */

typedef void* CCGMeshHDL;
typedef void* CCGVertHDL;
typedef void* CCGEdgeHDL;
typedef void* CCGFaceHDL;

typedef struct _CCGMeshIFC CCGMeshIFC;
struct _CCGMeshIFC {
	int			vertUserSize, edgeUserSize, faceUserSize;

	int			vertDataSize;
	void		(*vertDataZero)		(CCGMeshHDL m, void *t);
	int			(*vertDataEqual)	(CCGMeshHDL m, void *a, void *b);
	void		(*vertDataCopy)		(CCGMeshHDL m, void *t, void *a);
	void		(*vertDataAdd)		(CCGMeshHDL m, void *ta, void *b);
	void		(*vertDataSub)		(CCGMeshHDL m, void *ta, void *b);
	void		(*vertDataMulN)		(CCGMeshHDL m, void *ta, double n);
	void		(*vertDataAvg4)		(CCGMeshHDL m, void *t, void *a, void *b, void *c, void *d);

	int			(*getNumVerts)		(CCGMeshHDL m);
	int			(*getNumEdges)		(CCGMeshHDL m);
	int			(*getNumFaces)		(CCGMeshHDL m);
	CCGVertHDL	(*getVert)			(CCGMeshHDL m, int idx);
	CCGEdgeHDL	(*getEdge)			(CCGMeshHDL m, int idx);
	CCGFaceHDL	(*getFace)			(CCGMeshHDL m, int idx);

	void		(*getVertData)		(CCGMeshHDL m, CCGVertHDL v, void *data_r);

	CCGVertHDL	(*getEdgeVert0)		(CCGMeshHDL m, CCGEdgeHDL e);
	CCGVertHDL	(*getEdgeVert1)		(CCGMeshHDL m, CCGEdgeHDL e);

	int			(*getFaceNumVerts)	(CCGMeshHDL m, CCGFaceHDL f);
	CCGVertHDL	(*getFaceVert)		(CCGMeshHDL m, CCGFaceHDL f, int idx);
};

/***/

typedef void* CCGAllocatorHDL;

typedef struct _CCGAllocatorIFC CCGAllocatorIFC;
struct _CCGAllocatorIFC {
	void*		(*alloc)			(CCGAllocatorHDL a, int numBytes);
	void*		(*realloc)			(CCGAllocatorHDL a, void *ptr, int newSize, int oldSize);
	void		(*free)				(CCGAllocatorHDL a, void *ptr);
	void		(*release)			(CCGAllocatorHDL a);
};

/***/

typedef enum {
	eCCGError_None = 0,

	eCCGError_InvalidSyncState,
	eCCGError_InvalidValue,
} CCGError;

/***/

typedef struct _CCGSubSurf CCGSubSurf;

CCGSubSurf*	ccgSubSurf_new	(CCGMeshIFC *ifc, CCGMeshHDL meshData, int subdivisionLevels, CCGAllocatorIFC *allocatorIFC, CCGAllocatorHDL allocator);
void		ccgSubSurf_free	(CCGSubSurf *ss);

CCGError	ccgSubSurf_sync	(CCGSubSurf *ss);

CCGError	ccgSubSurf_initFullSync		(CCGSubSurf *ss);
CCGError	ccgSubSurf_initPartialSync	(CCGSubSurf *ss);

CCGError	ccgSubSurf_syncVert		(CCGSubSurf *ss, CCGVertHDL vHDL, void *vertData);
CCGError	ccgSubSurf_syncEdge		(CCGSubSurf *ss, CCGEdgeHDL eHDL, CCGVertHDL e_vHDL0, CCGVertHDL e_vHDL1);
CCGError	ccgSubSurf_syncFace		(CCGSubSurf *ss, CCGFaceHDL fHDL, int numVerts, CCGVertHDL *vHDLs);

CCGError	ccgSubSurf_syncVertDel	(CCGSubSurf *ss, CCGVertHDL vHDL);
CCGError	ccgSubSurf_syncEdgeDel	(CCGSubSurf *ss, CCGEdgeHDL eHDL);
CCGError	ccgSubSurf_syncFaceDel	(CCGSubSurf *ss, CCGFaceHDL fHDL);

CCGError	ccgSubSurf_processSync	(CCGSubSurf *ss);

CCGError	ccgSubSurf_setSubdivisionLevels		(CCGSubSurf *ss, int subdivisionLevels);
CCGError	ccgSubSurf_setAllowEdgeCreation		(CCGSubSurf *ss, int allowEdgeCreation);
CCGError	ccgSubSurf_setUseAgeCounts			(CCGSubSurf *ss, int useAgeCounts, int vertUserOffset, int edgeUserOffset, int faceUserOffset);

/***/

typedef struct _CCGVert CCGVert;
typedef struct _CCGEdge CCGEdge;
typedef struct _CCGFace CCGFace;

int			ccgSubSurf_getNumVerts				(CCGSubSurf *ss);
int			ccgSubSurf_getNumEdges				(CCGSubSurf *ss);
int			ccgSubSurf_getNumFaces				(CCGSubSurf *ss);

int			ccgSubSurf_getSubdivisionLevels		(CCGSubSurf *ss);
int			ccgSubSurf_getEdgeSize				(CCGSubSurf *ss);
int			ccgSubSurf_getEdgeLevelSize			(CCGSubSurf *ss, int level);
int			ccgSubSurf_getGridSize				(CCGSubSurf *ss);
int			ccgSubSurf_getGridLevelSize			(CCGSubSurf *ss, int level);

CCGVertHDL	ccgSubSurf_getVertVertHandle		(CCGSubSurf *ss, CCGVert *v);
int			ccgSubSurf_getVertNumFaces			(CCGSubSurf *ss, CCGVert *v);
CCGFace*	ccgSubSurf_getVertFace				(CCGSubSurf *ss, CCGVert *v, int index);
int			ccgSubSurf_getVertNumEdges			(CCGSubSurf *ss, CCGVert *v);
CCGEdge*	ccgSubSurf_getVertEdge				(CCGSubSurf *ss, CCGVert *v, int index);

int			ccgSubSurf_getVertAge				(CCGSubSurf *ss, CCGVert *v);
void*		ccgSubSurf_getVertUserData			(CCGSubSurf *ss, CCGVert *v);
void*		ccgSubSurf_getVertData				(CCGSubSurf *ss, CCGVert *v);
void*		ccgSubSurf_getVertLevelData			(CCGSubSurf *ss, CCGVert *v, int level);

CCGEdgeHDL	ccgSubSurf_getEdgeEdgeHandle		(CCGSubSurf *ss, CCGEdge *e);
int			ccgSubSurf_getEdgeNumFaces			(CCGSubSurf *ss, CCGEdge *e);
CCGFace*	ccgSubSurf_getEdgeFace				(CCGSubSurf *ss, CCGEdge *e, int index);
CCGVert*	ccgSubSurf_getEdgeVert0				(CCGSubSurf *ss, CCGEdge *e);
CCGVert*	ccgSubSurf_getEdgeVert1				(CCGSubSurf *ss, CCGEdge *e);

int			ccgSubSurf_getEdgeAge				(CCGSubSurf *ss, CCGEdge *e);
void*		ccgSubSurf_getEdgeUserData			(CCGSubSurf *ss, CCGEdge *e);
void*		ccgSubSurf_getEdgeDataArray			(CCGSubSurf *ss, CCGEdge *e);
void*		ccgSubSurf_getEdgeData				(CCGSubSurf *ss, CCGEdge *e, int x);
void*		ccgSubSurf_getEdgeLevelData			(CCGSubSurf *ss, CCGEdge *e, int x, int level);

CCGFaceHDL	ccgSubSurf_getFaceFaceHandle		(CCGSubSurf *ss, CCGFace *f);
int			ccgSubSurf_getFaceNumVerts			(CCGSubSurf *ss, CCGFace *f);
CCGVert*	ccgSubSurf_getFaceVert				(CCGSubSurf *ss, CCGFace *f, int index);
CCGEdge*	ccgSubSurf_getFaceEdge				(CCGSubSurf *ss, CCGFace *f, int index);

int			ccgSubSurf_getFaceAge				(CCGSubSurf *ss, CCGFace *f);
void*		ccgSubSurf_getFaceUserData			(CCGSubSurf *ss, CCGFace *f);
void*		ccgSubSurf_getFaceCenterData		(CCGSubSurf *ss, CCGFace *f);
void*		ccgSubSurf_getFaceGridEdgeDataArray	(CCGSubSurf *ss, CCGFace *f, int gridIndex);
void*		ccgSubSurf_getFaceGridEdgeData		(CCGSubSurf *ss, CCGFace *f, int gridIndex, int x);
void*		ccgSubSurf_getFaceGridDataArray		(CCGSubSurf *ss, CCGFace *f, int gridIndex);
void*		ccgSubSurf_getFaceGridData			(CCGSubSurf *ss, CCGFace *f, int gridIndex, int x, int y);

int			ccgSubSurf_getNumFinalVerts		(CCGSubSurf *ss);
int			ccgSubSurf_getNumFinalEdges		(CCGSubSurf *ss);
int			ccgSubSurf_getNumFinalFaces		(CCGSubSurf *ss);

/***/

typedef struct _CCGVertIterator CCGVertIterator;
typedef struct _CCGEdgeIterator CCGEdgeIterator;
typedef struct _CCGFaceIterator CCGFaceIterator;

CCGVertIterator*	ccgSubSurf_getVertIterator	(CCGSubSurf *ss);
CCGEdgeIterator*	ccgSubSurf_getEdgeIterator	(CCGSubSurf *ss);
CCGFaceIterator*	ccgSubSurf_getFaceIterator	(CCGSubSurf *ss);

CCGVert*			ccgVertIterator_getCurrent	(CCGVertIterator *vi);
int					ccgVertIterator_isStopped	(CCGVertIterator *vi);
void				ccgVertIterator_next		(CCGVertIterator *vi);
void				ccgVertIterator_free		(CCGVertIterator *vi);

CCGEdge*			ccgEdgeIterator_getCurrent	(CCGEdgeIterator *ei);
int					ccgEdgeIterator_isStopped	(CCGEdgeIterator *ei);
void				ccgEdgeIterator_next		(CCGEdgeIterator *ei);
void				ccgEdgeIterator_free		(CCGEdgeIterator *ei);

CCGFace*			ccgFaceIterator_getCurrent	(CCGFaceIterator *fi);
int					ccgFaceIterator_isStopped	(CCGFaceIterator *fi);
void				ccgFaceIterator_next		(CCGFaceIterator *fi);
void				ccgFaceIterator_free		(CCGFaceIterator *fi);
