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
#include "DNA_customdata_types.h"
#include "BLI_mempool.h"
#include "BKE_customdata.h"

/*forward declarations*/
struct BMVert;
struct BMEdge;
struct BMFace;
struct BMLoop;

/*
 * BMHeader
 *
 * All mesh elements begin with a BMHeader. This structure 
 * hold several types of data
 *
 * 1: The type of the element (vert, edge, loop or face)
 * 2: Persistant flags/markings (sharp, seam, select, hidden, ect)
 * 3: Unique ID in the bmesh.
 * 4: some elements for internal record keeping.
 *
*/

/*BMHeader->type*/
#define BM_VERT 	1
#define BM_EDGE 	2
#define BM_FACE 	4
#define BM_LOOP 	8
#define BM_ALL		BM_VERT | BM_EDGE | BM_FACE | BM_LOOP

/*BMHeader->flag*/
#define BM_SELECT	(1<<0)

#define BM_SEAM		(1<<1)
#define BM_FGON		(1<<2)
#define BM_HIDDEN	(1<<3)
#define BM_SHARP	(1<<4)
#define BM_SMOOTH	(1<<5)

typedef struct BMHeader {
	struct BMHeader *next, *prev;
	int 		EID;  /*Consider removing this/making it ifdeffed for debugging*/
	int 		flag, type;
	int			eflag1, eflag2;	/*Flags used by eulers. Try and get rid of/minimize some of these*/
	struct BMFlagLayer *flags; /*Dynamically allocated block of flag layers for operators to use*/
} BMHeader;

typedef struct BMFlagLayer {
	int f1;
	short mask, pflag;
} BMFlagLayer;

#define BM_OVERLAP		(1<<14)			/*used by bmesh_verts_in_face*/
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
	int nextv, nexte, nextp, nextl;
	struct CustomData vdata, edata, pdata, ldata;
	int selectmode;
	struct BLI_mempool *flagpool;					/*memory pool for dynamically allocated flag layers*/
	int stackdepth;									/*current depth of operator stack*/
	int totflags, walkers;							/*total number of tool flag layers*/
	ListBase errorstack;
} BMesh;

typedef struct BMVert {	
	struct BMHeader head;
	float co[3];									
	float no[3];									
	struct BMEdge *edge;
	void *data;
	void *tmp;													/*what?*/
	float bweight;												/*please, someone just get rid of me...*/
} BMVert;

typedef struct BMEdge {
	struct BMHeader head;
	struct BMVert *v1, *v2;
	struct BMNode d1, d2;
	struct BMLoop *loop;
	void *data;
	float crease, bweight; /*make these custom data.... no really, please....*/
} BMEdge;

typedef struct BMLoop  {
	struct BMHeader head;
	struct BMNode radial;
	struct BMVert *v;
	struct BMEdge *e;
	struct BMFace *f;	
	void *data;
} BMLoop;

typedef struct BMFace {
	struct BMHeader head;
	struct BMLoop *loopbase;
	int len;
	void *data;
	float no[3];

	/*custom data again*/
	short mat_nr; 
} BMFace;

/*stub */
void bmesh_error(void);

/*Mesh Level Ops */
struct BMesh *BM_Make_Mesh(int allocsize[4]);
void BM_Free_Mesh(struct BMesh *bm);
void BM_Compute_Normals(struct BMesh *bm);

/*Construction*/
struct BMVert *BM_Make_Vert(struct BMesh *bm, float co[3], struct BMVert *example);
struct BMEdge *BM_Make_Edge(struct BMesh *bm, struct BMVert *v1, struct BMVert *v2, struct BMEdge *example, int nodouble);
struct BMFace *BM_Make_Quadtriangle(struct BMesh *bm, struct BMVert **verts, BMEdge **edges, int len, struct BMFace *example, int nodouble);
struct BMFace *BM_Make_Ngon(struct BMesh *bm, struct BMVert *v1, struct BMVert *v2, struct BMEdge **edges, int len, int nodouble);
/*copies loop data from adjacent faces*/
void BM_Face_CopyShared(BMesh *bm, BMFace *f);
void BM_Copy_Attributes(struct BMesh *source_mesh, struct BMesh *target_mesh, void *source, void *target);
void BM_remove_tagged_faces(struct BMesh *bm, int flag);
void BM_remove_tagged_edges(struct BMesh *bm, int flag);
void BM_remove_tagged_verts(struct BMesh *bm, int flag);


/*Modification*/
struct BMFace *BM_Join_Faces(struct BMesh *bm, struct BMFace *f1, struct BMFace *f2, struct BMEdge *e, int calcnorm, int weldUVs);
struct BMFace *BM_Split_Face(struct BMesh *bm, struct BMFace *f, struct BMVert *v1, struct BMVert *v2, struct BMLoop **nl, struct BMEdge *example, int calcnorm);
void BM_Collapse_Vert(struct BMesh *bm, struct BMEdge *ke, struct BMVert *kv, float fac, int calcnorm);
struct BMVert *BM_Split_Edge(struct BMesh *bm, struct BMVert *v, struct BMEdge *e, struct BMEdge **ne, float percent, int calcnorm);
struct BMVert  *BM_Split_Edge_Multi(struct BMesh *bm, struct BMEdge *e, int numcuts);
BMEdge *BM_Connect_Verts(BMesh *bm, BMVert *v1, BMVert *v2, BMFace **nf);

/*dissolves vert surrounded by faces*/
int BM_Dissolve_Disk(BMesh *bm, BMVert *v);

/*dissolves vert, in more situations then BM_Dissolve_Disk.*/
int BM_Dissolve_Vert(BMesh *bm, BMVert *v);

/*Interpolation*/
void BM_Data_Interp_From_Verts(struct BMesh *bm, struct BMVert *v1, struct BMVert *v2, struct BMVert *v, float fac);
void BM_Data_Facevert_Edgeinterp(struct BMesh *bm, struct BMVert *v1, struct BMVert *v2, struct BMVert *v, struct BMEdge *e1, float fac);
//void bmesh_data_interp_from_face(struct BMesh *bm, struct BMFace *source, struct BMFace *target);

struct EditMesh;
BMesh *editmesh_to_bmesh(struct EditMesh *em);
struct EditMesh *bmesh_to_editmesh(BMesh *bm);

/*include the rest of the API*/
#include "bmesh_filters.h"
#include "bmesh_iterators.h"
#include "bmesh_marking.h"
#include "bmesh_operators.h"
#include "bmesh_queries.h"
#include "bmesh_walkers.h"

#endif /* BMESH_H */
