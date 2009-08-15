/* $Id$ */

typedef void* CCGMeshHDL;
typedef void* CCVertHDL;
typedef void* CCEdgeHDL;
typedef void* CCFaceHDL;

typedef struct _CCVert CCVert;
typedef struct _CCEdge CCEdge;
typedef struct _CCFace CCFace;

typedef struct _CCGMeshIFC CCGMeshIFC;
struct _CCGMeshIFC {
	int vertUserSize, edgeUserSize, faceUserSize;
	int vertDataSize;
};

/***/

typedef void* CCAllocHDL;

typedef struct _CCGAllocatorIFC CCGAllocatorIFC;
struct _CCGAllocatorIFC {
	void*		(*alloc)	(CCAllocHDL a, int numBytes);
	void*		(*realloc)	(CCAllocHDL a, void *ptr, int newSize, int oldSize);
	void		(*free)		(CCAllocHDL a, void *ptr);
	void		(*release)	(CCAllocHDL a);
};

/***/

typedef enum {
	eCCGError_None = 0,

	eCCGError_InvalidSyncState,
	eCCGError_InvalidValue,
} CCGError;

/***/

typedef struct _CSubSurf CSubSurf;

CSubSurf*	CCS_new			(CCGMeshIFC *ifc, int subdivisionLevels, CCGAllocatorIFC *allocatorIFC, CCAllocHDL allocator);
void		CCS_free		(CSubSurf *ss);

CCGError	CCS_sync		(CSubSurf *ss);

CCGError	CCS_initFullSync	(CSubSurf *ss);
CCGError	CCS_initPartialSync	(CSubSurf *ss);

CCGError	CCS_syncVert		(CSubSurf *ss, CCVertHDL vHDL, void *vertData, int seam, CCVert **v_r);
CCGError	CCS_syncEdge		(CSubSurf *ss, CCEdgeHDL eHDL, CCVertHDL e_vHDL0, CCVertHDL e_vHDL1, float crease, CCEdge **e_r);
CCGError	CCS_syncFace		(CSubSurf *ss, CCFaceHDL fHDL, int numVerts, CCVertHDL *vHDLs, CCFace **f_r);

CCGError	CCS_syncVertDel		(CSubSurf *ss, CCVertHDL vHDL);
CCGError	CCS_syncEdgeDel		(CSubSurf *ss, CCEdgeHDL eHDL);
CCGError	CCS_syncFaceDel		(CSubSurf *ss, CCFaceHDL fHDL);

CCGError	CCS_processSync		(CSubSurf *ss);

CCGError	CCS_setSubdivisionLevels(CSubSurf *ss, int subdivisionLevels);

CCGError	CCS_setAllowEdgeCreation(CSubSurf *ss, int allowEdgeCreation, float defaultCreaseValue, void *defaultUserData);
void		CCS_getAllowEdgeCreation(CSubSurf *ss, int *allowEdgeCreation_r, float *defaultCreaseValue_r, void *defaultUserData_r);

void		CCS_getUseAgeCounts	(CSubSurf *ss, int *useAgeCounts_r, int *vertUserOffset_r, int *edgeUserOffset_r, int *faceUserOffset_r);
CCGError	CCS_setUseAgeCounts	(CSubSurf *ss, int useAgeCounts, int vertUserOffset, int edgeUserOffset, int faceUserOffset);

CCGError	CCS_setCalcVertexNormals(CSubSurf *ss, int useVertNormals, int normalDataOffset);

/***/

int		CCS_getNumVerts		(CSubSurf *ss);
int		CCS_getNumEdges		(CSubSurf *ss);
int		CCS_getNumFaces	(CSubSurf *ss);

int		CCS_getSubdivisionLevels(CSubSurf *ss);
int		CCS_getEdgeSize		(CSubSurf *ss);
int		CCS_getEdgeLevelSize	(CSubSurf *ss, int level);
int		CCS_getGridSize		(CSubSurf *ss);
int		CCS_getGridLevelSize	(CSubSurf *ss, int level);

CCVert*		CCS_getVert		(CSubSurf *ss, CCVertHDL v);
CCVertHDL	CCS_getVertVertHandle	(CCVert *v);
int		CCS_getVertNumFaces	(CCVert *v);
CCFace*		CCS_getVertFace		(CCVert *v, int index);
int		CCS_getVertNumEdges	(CCVert *v);
CCEdge*		CCS_getVertEdge		(CCVert *v, int index);

int		CCS_getVertAge		(CSubSurf *ss, CCVert *v);
void*		CCS_getVertUserData	(CSubSurf *ss, CCVert *v);
void*		CCS_getVertData		(CSubSurf *ss, CCVert *v);
void*		CCS_getVertLevelData	(CSubSurf *ss, CCVert *v, int level);

CCEdge*		CCS_getEdge		(CSubSurf *ss, CCEdgeHDL e);
CCEdgeHDL	CCS_getEdgeEdgeHandle	(CCEdge *e);
int		CCS_getEdgeNumFaces	(CCEdge *e);
CCFace*		CCS_getEdgeFace		(CCEdge *e, int index);
CCVert*		CCS_getEdgeVert0	(CCEdge *e);
CCVert*		CCS_getEdgeVert1	(CCEdge *e);
float		CCS_getEdgeCrease	(CCEdge *e);

int		CCS_getEdgeAge		(CSubSurf *ss, CCEdge *e);
void*		CCS_getEdgeUserData	(CSubSurf *ss, CCEdge *e);
void*		CCS_getEdgeDataArray	(CSubSurf *ss, CCEdge *e);
void*		CCS_getEdgeData		(CSubSurf *ss, CCEdge *e, int x);
void*		CCS_getEdgeLevelData	(CSubSurf *ss, CCEdge *e, int x, int level);

CCFace*		CCS_getFace		(CSubSurf *ss, CCFaceHDL f);
CCFaceHDL	CCS_getFaceFaceHandle	(CSubSurf *ss, CCFace *f);
int		CCS_getFaceNumVerts	(CCFace *f);
CCVert*		CCS_getFaceVert		(CSubSurf *ss, CCFace *f, int index);
CCEdge*		CCS_getFaceEdge		(CSubSurf *ss, CCFace *f, int index);
int		CCS_getFaceEdgeIndex	(CCFace *f, CCEdge *e);

int		CCS_getFaceAge		(CSubSurf *ss, CCFace *f);
void*		CCS_getFaceUserData	(CSubSurf *ss, CCFace *f);
void*		CCS_getFaceCenterData	(CCFace *f);
void*		CCS_getFaceGridEdgeDataArray	(CSubSurf *ss, CCFace *f, int gridIndex);
void*		CCS_getFaceGridEdgeData		(CSubSurf *ss, CCFace *f, int gridIndex, int x);
void*		CCS_getFaceGridDataArray	(CSubSurf *ss, CCFace *f, int gridIndex);
void*		CCS_getFaceGridData		(CSubSurf *ss, CCFace *f, int gridIndex, int x, int y);

int		CCS_getNumFinalVerts		(CSubSurf *ss);
int		CCS_getNumFinalEdges		(CSubSurf *ss);
int		CCS_getNumFinalFaces		(CSubSurf *ss);

/***/

typedef struct _CCVertIterator CCVertIterator;
typedef struct _CCEdgeIterator CCEdgeIterator;
typedef struct _CCFaceIterator CCFaceIterator;

CCVertIterator*	CCS_getVertIterator		(CSubSurf *ss);
CCEdgeIterator*	CCS_getEdgeIterator		(CSubSurf *ss);
CCFaceIterator*	CCS_getFaceIterator		(CSubSurf *ss);

CCVert*		CCVIter_getCurrent		(CCVertIterator *vi);
int		CCVIter_isStopped		(CCVertIterator *vi);
void		CCVIter_next			(CCVertIterator *vi);
void		CCVIter_free			(CCVertIterator *vi);

CCEdge*		CCEIter_getCurrent		(CCEdgeIterator *ei);
int		CCEIter_isStopped		(CCEdgeIterator *ei);
void		CCEIter_next			(CCEdgeIterator *ei);
void		CCEIter_free			(CCEdgeIterator *ei);

CCFace*		CCFIter_getCurrent		(CCFaceIterator *fi);
int		CCFIter_isStopped		(CCFaceIterator *fi);
void		CCFIter_next			(CCFaceIterator *fi);
void		CCFIter_free			(CCFaceIterator *fi);
