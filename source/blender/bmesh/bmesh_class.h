#ifndef _BMESH_CLASS_H
#define _BMESH_CLASS_H

#include "DNA_listBase.h"
#include "DNA_customdata_types.h"

struct BMesh;
struct BMVert;
struct BMEdge;
struct BMLoop;
struct BMFace;
struct BMBaseVert;
struct BMBaseEdge;
struct BMBaseLoop;
struct BMBaseFace;
struct BMLayerType;
struct BMSubClassLayer;
struct BMFlagLayer;
struct BLI_mempool;

/*
 UPDATE: ok, this hasn't been all that useful.  Need to rip this out and just go with original 
         structs. 
       
        well, much of the actual code was great, but this inheritance thing isn't so
        useful, need to just make CDDM better.
*/
/*
ok: we have a simple subclassing system, to layer in bmesh api features (and
let people subclass the api).  There's also a separate, compile-time system
that will end up being the back-end to a "lite" bmesh API for modifiers.

there are two seperate and distinct subtyping strategies here.  one is with
macros and struct inheritence, and the other is more dynamic.  this is because
of two competing motivations for subclassing: the ability to code faster,
less memory intensive BMTools that don't use adjacency info, and the ability
to hook into higher-level API functions for things like multires interpolation,
which needs much more then what is provided in the CustomData API.

The first strategy is part of a plan to replace CDDM with a bmesh-like API
(which is much easier for me then rewriting array, mirror, in CDDM, which
would be a huge pain).
*/

/*note: it is very important for BMHeader to start with two
  pointers. this is a requirement of mempool's method of
  iteration.
*/
typedef struct BMHeader {
	void *data; /*customdata layers*/
	void *layerdata; /*dynamic subclass data, doesn't include BMTool and adjacency which use a static compile-time method */
	int eid; /*element id*/
	short type; /*element geometric type (verts/edges/loops/faces)*/
	short flag; /*this would be a CD layer, see below*/
	short eflag1, eflag2;
	int sysflag, index; /*note: do *not* touch sysflag! and use BMINDEX_GET/SET macros for index*/
	struct BMFlagLayer *flags;
} BMHeader;

/*note: need some way to specify custom locations for custom data layers.  so we can
make them point directly into structs.  and some way to make it only happen to the
active layer, and properly update when switching active layers.*/

/*alloc type a: smallest mesh possible*/
#define BM_BASE_VHEAD\
	BMHeader head;\
	float co[3];\
	float no[3];

typedef struct BMBaseVert {
	BM_BASE_VHEAD
} BMBaseVert;

#define BM_BASE_EHEAD(vtype)\
	BMHeader head;\
	struct vtype *v1, *v2;

typedef struct BMBaseEdge {
	BM_BASE_EHEAD(BMBaseVert)
} BMBaseEdge;

#define BM_BASE_LHEAD(vtype, etype, ltype)\
	BMHeader head;\
	struct vtype *v;\
	struct etype *e;\
	struct ltype *next, *prev; /*won't be able to use listbase API, ger, due to head*/\
	int _index; /*used for sorting during tesselation*/

typedef struct BMBaseLoop {
	BM_BASE_LHEAD(BMBaseVert, BMBaseEdge, BMBaseLoop)
} BMBaseLoop;

#define BM_BASE_LSTHEAD(listtype, looptype)\
	struct listtype *next, *prev;\
	struct looptype *first, *last;

typedef struct BMBaseLoopList {
	BM_BASE_LSTHEAD(BMBaseLoopList, BMBaseLoop)
} BMBaseLoopList;

#define BM_BASE_FHEAD\
	BMHeader head;\
	int len; /*includes all boundary loops*/\
	int totbounds; /*total boundaries, is one plus the number of holes in the face*/\
	ListBase loops;\
	float no[3]; /*yes, we do store this here*/\
	short mat_nr;
	
typedef struct BMBaseFace {
	BM_BASE_FHEAD
} BMBaseFace;

typedef struct BMFlagLayer {
	short f, pflag; /*flags*/
	int index; /*generic index*/
} BMFlagLayer;

#define BM_ADJ_VHEAD(etype)\
	BM_BASE_VHEAD\
	struct etype *e;

typedef struct BMVert {
	BM_ADJ_VHEAD(BMEdge)
} BMVert;

#define BM_ADJ_EHEAD(vtype, etype, ltype)\
	BM_BASE_EHEAD(vtype)\
	struct ltype *l;\
	/*disk cycle pointers*/\
	struct {\
		struct etype *next, *prev;\
	} dlink1;\
	struct {\
		struct etype *next, *prev;\
	} dlink2;

typedef struct BMEdge {
	BM_ADJ_EHEAD(BMVert, BMEdge, BMLoop)
} BMEdge;

#define BM_ADJ_LHEAD(vtype, etype, ltype, ftype)\
	BM_BASE_LHEAD(vtype, etype, ltype)\
	struct ltype *radial_next, *radial_prev;\
	struct ftype *f;

typedef struct BMLoop {
	BM_ADJ_LHEAD(BMVert, BMEdge, BMLoop, BMFace)
} BMLoop;

typedef struct BMLoopList {
	BM_BASE_LSTHEAD(BMLoopList, BMLoop)
} BMLoopList;

#define BM_ADJ_FHEAD\
	BM_BASE_FHEAD
typedef struct BMFace {
	BM_ADJ_FHEAD
} BMFace;

/*this is part of the lower-level face splitting API, higer-level
stuff will be preferred*/
typedef struct BMFaceCut {
	BMLoop *l1, *l2;
	ListBase origface_loops;
	ListBase newface_loops;
	BMFace *new_f; /*empty new face to be filled by api*/
} BMFaceCut;

/*ok, in nearly (if not all) cases the subclasses will use CustomData to store stuff, but they store things
here as well (except it's not saved in files obviously, or interpolating, or all the other things the CD
system does)*/
#define BMSC_GETSELF(bm, e, type) (void*)(((char*)((BMHeader*)(e))->layerdata) + bm->layer_offsets[(type)->__index])
#define BMSC_DEFAULT_LAYERSIZE sizeof(LayerType)

#define BM_SUBCLASS_HEAD	struct BMLayerType *type; int __index;

typedef struct BMSubClassLayer {
	BM_SUBCLASS_HEAD
} BMSubClassLayer;

typedef struct BMLayerType {
	int vsize, esize, lsize, fsize;
	int meshsize; /*size of custom mesh structure, if exists*/
	
	/*note that allocation is done entirely outside of the subclass functions, thus the need for
	  the above struct size parameters*/
	void (*new_mesh)(struct BMesh *bm, void *self);
	void (*free_mesh)(struct BMesh *bm, void *self);
	
	/*these functions may return NULL if this child class doesn't need to store anything
	outside of the CustomData API*/
	void (*new_vert)(struct BMesh *bm, BMBaseVert *v, void *self);
	void (*new_edge)(struct BMesh *bm, BMBaseEdge *e, void *self); 
	void (*new_loop)(struct BMesh *bm, BMBaseLoop *l, void *self, BMBaseFace *f);
	void (*new_face)(struct BMesh *bm, BMBaseFace *f, void *self);
	
	void (*free_vert)(struct BMesh *bm, BMBaseVert *v);
	void (*free_edge)(struct BMesh *bm, BMBaseEdge *e);
	void (*free_loop)(struct BMesh *bm, BMBaseLoop *l);
	void (*free_face)(struct BMesh *bm, BMBaseFace *f);

	void (*copy_vert)(struct BMesh *bm, BMBaseVert *v);
	void (*copy_edge)(struct BMesh *bm, BMBaseEdge *e);
	void (*copy_loop)(struct BMesh *bm, BMBaseLoop *l);
	void (*copy_face)(struct BMesh *bm, BMBaseFace *f);

	/*hrm, I wonder if I should have this at all, faces_from_faces might be
	  better all by itself*/
	void (*split_face)(struct BMesh *bm, void *f, BMFaceCut *cuts, int totcut);

	/*interpolates non-CustomData-stored data.  faces in dest overlap some or all faces in source*/
	void (*faces_from_faces)(struct BMesh *bm, BMFace **sources, int totsource, BMFace **dests, int totdest);
	
	int required_base_layers; /*sets if BMTool flags or adjacency data layers are needed*/
	int customdata_required_layers; /*mask of required CD layers*/
} BMLayerType;

typedef struct BMesh {
	int totvert, totedge, totloop, totface;
	int totvertsel, totedgesel, totfacesel;
	
	/*element pools*/
	struct BLI_mempool *vpool, *epool, *lpool, *fpool;

	/*subclass data layer pools*/
	struct BLI_mempool *svpool, *sepool, *slpool, *sfpool;

	/*operator api stuff*/
	struct BLI_mempool *toolflagpool;
	int stackdepth;
	struct BMOperator *currentop;
	
	CustomData vdata, edata, ldata, pdata;

	struct BLI_mempool *looplistpool;
	
	/*stuff for compile-time subclassing*/
	int baselevel, totlayer;
	BMSubClassLayer *layers; /*does not include base types*/
	int *layer_offsets;
	
	/*should be copy of scene select mode*/
	int selectmode;
	
	/*ID of the shape key this bmesh came from*/
	int shapenr;
	
	int walkers, totflags;
	ListBase selected, error_stack;
	
	BMFace *act_face;

	ListBase errorstack;
} BMesh;

void BM_Copy_Vert(BMesh *bm, BMVert *destv, BMVert *source);
void BM_Copy_Edge(BMesh *bm, BMEdge *deste, BMEdge *source);
void BM_Copy_Loop(BMesh *bm, BMLoop *destl, BMLoop *source);
void BM_Copy_Face(BMesh *bm, BMFace *destf, BMFace *source);

#define LAYER_BASE	1
#define LAYER_TOOL	2
#define LAYER_ADJ	4
#define MAX_LAYERS	2 /*does not include base*/

#define BM_VERT		1
#define BM_EDGE		2
#define BM_LOOP		4
#define BM_FACE		8

#endif /* _BMESH_CLASS_H */
