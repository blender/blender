/**
 *  bmesh.h    jan 2007
 *
 *	BMesh API.
 *
 * $Id: BKE_bmesh.h,v 1.00 2007/01/17 17:42:01 Briggs Exp $
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.	
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2004 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Geoffrey Bantle, Levi Schooley.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef BMESH_H
#define BMESH_H

#include "DNA_listBase.h"
#include "DNA_scene_types.h"
#include "DNA_customdata_types.h"

#include "BKE_customdata.h"

#include "BLI_mempool.h"

/*
short introduction:

the bmesh structure is a boundary representation, supporting non-manifold 
locally modifiable topology. the API is designed to allow clean, maintainable
code, that never (or almost never) directly inspects the underlying structure.

The API includes iterators, including many useful topological iterators;
walkers, which walk over a mesh, without the risk of hitting the recursion
limit; operators, which are logical, reusable mesh modules; topological
modification functions (like split face, join faces, etc), which are used for 
topological manipulations; and some (not yet finished) geometric utility 
functions.

some definitions:

tool flags: private flags for tools.  each operator has it's own private
            tool flag "layer", which it can use to flag elements.
	    tool flags are also used by various other parts of the api.
header flags: stores persistent flags, such as selection state, hide state,
              etc.  be careful of touching these.
*/

/*forward declarations*/
struct BMesh;
struct BMVert;
struct BMEdge;
struct BMFace;
struct BMLoop;
struct BMOperator;
struct Mesh;
struct EditMesh;

/*
 * BMHeader
 *
 * All mesh elements begin with a BMHeader. This structure 
 * hold several types of data
 *
 * 1: The type of the element (vert, edge, loop or face)
 * 2: Persistant "header" flags/markings (sharp, seam, select, hidden, ect)
      note that this is different from the "tool" flags.
 * 3: Unique ID in the bmesh.
 * 4: some elements for internal record keeping.
 *
*/

/*BMHeader->type*/
#define BM_VERT 	1
#define BM_EDGE 	2
#define BM_LOOP 	4
#define BM_FACE 	8
#define BM_ALL		(BM_VERT | BM_EDGE | BM_LOOP | BM_FACE)

/*BMHeader->flag*/
#define BM_SELECT	(1<<0)

#define BM_SEAM		(1<<1)
#define BM_FGON		(1<<2)
#define BM_HIDDEN	(1<<3)
#define BM_SHARP	(1<<4)
#define BM_SMOOTH	(1<<5)
#define BM_ACTIVE	(1<<6)
#define BM_NONORMCALC	(1<<7)
#define BM_PINNED	(1<<8)

typedef struct BMHeader {
	struct BMHeader *next, *prev;
	int 		EID;  /*Consider removing this/making it ifdeffed for debugging*/
	
	/*don't confuse this with tool flags.  this flag
	  member is what "header flag" means.*/
	int		flag;
	int		type; /*the element type, can be BM_VERT, BM_EDGE, BM_LOOP, or BM_FACE*/
	int		eflag1, eflag2;	/*Flags used by eulers. Try and get rid of/minimize some of these*/
	
	/*this is only used to store temporary integers.  
	  don't use it for anything else.
	  use the BMINDEX_GET and BMINDEX_SET macros to access it*/
	int index;
	struct BMFlagLayer *flags; /*Dynamically allocated block of flag layers for operators to use*/
	void *data; /*customdata block*/
} BMHeader;

typedef struct BMFlagLayer {
	int f1;
	short mask, pflag;
} BMFlagLayer;

#define BM_OVERLAP	(1<<14)			/*used by bmesh_verts_in_face*/
#define BM_EDGEVERT 	(1<<15) 		/*used by bmesh_make_ngon*/

/*
 * BMNode
 *
 * Used for circular/linked list functions that form basis of
 * adjacency system in BMesh. This should probably be hidden 
 * somewhere since tool authors never need to know about it.
 *
*/

typedef struct BMNode {
	struct BMNode *next, *prev;
	void *data;
} BMNode;

typedef struct BMesh {
	ListBase verts, edges, polys;
	struct BLI_mempool *vpool;
	struct BLI_mempool *epool;
	struct BLI_mempool *lpool;
	struct BLI_mempool *ppool;
	struct BMVert **vtar;
	struct BMEdge **edar;
	struct BMLoop **lpar;
	struct BMFace **plar;
	int vtarlen, edarlen, lparlen, plarlen;
	int totvert, totedge, totface, totloop;	
	int totvertsel, totedgesel, totfacesel;
	int nextv, nexte, nextp, nextl;
	struct CustomData vdata, edata, pdata, ldata;
	int selectmode; /*now uses defines in DNA_scene_types.h*/
	struct BLI_mempool *flagpool;					/*memory pool for dynamically allocated flag layers*/
	int stackdepth;									/*current depth of operator stack*/
	int totflags, walkers;							/*total number of tool flag layers*/
	ListBase errorstack;

	/*selection order list*/
	ListBase selected;

	/*active face pointer*/
	struct BMFace *act_face;

	/*active shape key number, should really be in BMEditMesh, not here*/
	int shapenr;
} BMesh;

typedef struct BMVert {	
	struct BMHeader head;
	float co[3];									
	float no[3];									
	struct BMEdge *edge;
	float bweight;			/*please, someone just get rid of me...*/
} BMVert;

typedef struct BMEdge {
	struct BMHeader head;
	struct BMVert *v1, *v2;
	struct BMNode d1, d2;
	struct BMLoop *loop;
	float crease, bweight; /*make these custom data.... no really, please....*/
} BMEdge;

typedef struct BMLoop  {
	struct BMHeader head;
	struct BMNode radial;
	struct BMVert *v;
	struct BMEdge *e;
	struct BMFace *f;	
} BMLoop;

typedef struct BMFace {
	struct BMHeader head;
	struct BMLoop *loopbase;
	int len;
	float no[3];

	/*custom data again*/
	short mat_nr; 
} BMFace;

/*stub */
void bmesh_error(void);

/*Mesh Level Ops */
struct BMesh *BM_Make_Mesh(int allocsize[4]);
BMesh *BM_Copy_Mesh(BMesh *bmold);
void BM_Free_Mesh(struct BMesh *bm);

/*frees mesh, but not actual BMesh struct*/
void BM_Free_Mesh_Data(BMesh *bm);
void BM_Compute_Normals(struct BMesh *bm);

/*Construction*/
struct BMVert *BM_Make_Vert(struct BMesh *bm, float co[3], struct BMVert *example);
struct BMEdge *BM_Make_Edge(struct BMesh *bm, struct BMVert *v1, struct BMVert *v2, struct BMEdge *example, int nodouble);
struct BMFace *BM_Make_Quadtriangle(struct BMesh *bm, struct BMVert **verts, BMEdge **edges, int len, struct BMFace *example, int nodouble);

/*more easier to use version of BM_Make_Quadtriangle.
  creates edges if necassary.*/
BMFace *BM_Make_QuadTri(BMesh *bm, BMVert *v1, BMVert *v2, BMVert *v3, 
			BMVert *v4, BMFace *example, int nodouble);

/*makes an ngon from an unordered list of edges.  v1 and v2 must be the verts
defining edges[0], and define the winding of the new face.*/
struct BMFace *BM_Make_Ngon(struct BMesh *bm, struct BMVert *v1, struct BMVert *v2, struct BMEdge **edges, int len, int nodouble);

/*stuff for dealing with header flags*/
#define BM_TestHFlag(ele, f) (((BMHeader*)ele)->flag & (f))
#define BM_SetHFlag(ele, f) (((BMHeader*)ele)->flag = ((BMHeader*)ele)->flag | (f))
#define BM_ClearHFlag(ele, f) (((BMHeader*)ele)->flag = ((BMHeader*)ele)->flag & ~(f))

/*stuff for setting indices in elements.*/
#define BMINDEX_SET(ele, i) (((BMHeader*)(ele))->index = i)
#define BMINDEX_GET(ele) (((BMHeader*)(ele))->index)

/*copies loop data from adjacent faces*/
void BM_Face_CopyShared(BMesh *bm, BMFace *f);

/*copies attributes, e.g. customdata, header flags, etc, from one element
  to another of the same type.*/
void BM_Copy_Attributes(struct BMesh *source_mesh, struct BMesh *target_mesh, void *source, void *target);

/*Modification*/
/*join two adjacent faces together along an edge.  note that
  the faces must only be joined by on edge.  e is the edge you
  wish to dissolve.*/
struct BMFace *BM_Join_Faces(struct BMesh *bm, struct BMFace *f1, 
                             struct BMFace *f2, struct BMEdge *e);

/*split a face along two vertices.  returns the newly made face, and sets
  the nl member to a loop in the newly created edge.*/
struct BMFace *BM_Split_Face(struct BMesh *bm, struct BMFace *f,  
                             struct BMVert *v1, struct BMVert *v2, 
			     struct BMLoop **nl, struct BMEdge *example);

/*dissolves a vert shared only by two edges*/
void BM_Collapse_Vert(struct BMesh *bm, struct BMEdge *ke, struct BMVert *kv, 
		      float fac);

/*splits an edge.  ne is set to the new edge created.*/
struct BMVert *BM_Split_Edge(struct BMesh *bm, struct BMVert *v, 
                             struct BMEdge *e, struct BMEdge **ne,
                             float percent);

/*split an edge multiple times evenly*/
struct BMVert  *BM_Split_Edge_Multi(struct BMesh *bm, struct BMEdge *e, 
	                            int numcuts);

/*connect two verts together, through a face they share.  this function may
  be removed in the future.*/
BMEdge *BM_Connect_Verts(BMesh *bm, BMVert *v1, BMVert *v2, BMFace **nf);

/*rotates an edge topologically, either clockwise (if ccw=0) or counterclockwise
  (if ccw is 1).*/
BMEdge *BM_Rotate_Edge(BMesh *bm, BMEdge *e, int ccw);

/*updates a face normal*/
void BM_Face_UpdateNormal(BMesh *bm, BMFace *f);

/*updates face and vertex normals incident on an edge*/
void BM_Edge_UpdateNormals(BMesh *bm, BMEdge *e);

/*update a vert normal (but not the faces incident on it)*/
void BM_Vert_UpdateNormal(BMesh *bm, BMVert *v);

void BM_flip_normal(BMesh *bm, BMFace *f);

/*dissolves all faces around a vert, and removes it.*/
int BM_Dissolve_Disk(BMesh *bm, BMVert *v);

/*dissolves vert, in more situations then BM_Dissolve_Disk
  (e.g. if the vert is part of a wire edge, etc).*/
int BM_Dissolve_Vert(BMesh *bm, BMVert *v);


/*Interpolation*/
void BM_Data_Interp_From_Verts(struct BMesh *bm, struct BMVert *v1, struct BMVert *v2, struct BMVert *v, float fac);
void BM_Data_Facevert_Edgeinterp(struct BMesh *bm, struct BMVert *v1, struct BMVert *v2, struct BMVert *v, struct BMEdge *e1, float fac);
//void bmesh_data_interp_from_face(struct BMesh *bm, struct BMFace *source, struct BMFace *target);
void BM_add_data_layer(BMesh *em, CustomData *data, int type);
void BM_add_data_layer_named(BMesh *bm, CustomData *data, int type, char *name);
void BM_free_data_layer(BMesh *em, CustomData *data, int type);


/*computes the centroid of a face, using the center of the bounding box*/
int BM_Compute_Face_Center(BMesh *bm, BMFace *f, float center[3]);

void BM_SelectMode_Flush(BMesh *bm);

/*convert an editmesh to a bmesh*/
BMesh *editmesh_to_bmesh(struct EditMesh *em);

/*initializes editmesh to bmesh operator, but doesn't execute.
  this is used in situations where you need to get access to the
  conversion operator's editmesh->bmesh mapping slot (e.g. if you
  need to find the bmesh edge that corrusponds to a specific editmesh
  edge).*/
BMesh *init_editmesh_to_bmesh(struct EditMesh *em, struct BMOperator *op);

/*converts a bmesh to an editmesh*/
struct EditMesh *bmesh_to_editmesh(BMesh *bm);

/*convert between bmesh and Mesh flags*/
int BMFlags_To_MEFlags(void *element);

/*convert between Mesh and bmesh flags
  type must be BM_VERT/BM_EDGE/BM_FACE,
  and represents the type of the element
  parameter (the three defines map to
  MVert, MEdge, and MPoly, respectively).*/
int MEFlags_To_BMFlags(int flag, int type);

/*convert MLoop*** in a bmface to mtface and mcol in
  an MFace*/
void BM_loops_to_corners(BMesh *bm, struct Mesh *me, int findex,
                         BMFace *f, int numTex, int numCol);

/*include the rest of the API*/
#include "bmesh_filters.h"
#include "bmesh_iterators.h"
#include "bmesh_marking.h"
#include "bmesh_operator_api.h"
#include "bmesh_operators.h"
#include "bmesh_error.h"
#include "bmesh_queries.h"
#include "bmesh_walkers.h"

#endif /* BMESH_H */
