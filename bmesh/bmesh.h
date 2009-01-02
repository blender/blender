/**
 *  bmesh.h    jan 2007
 *
 *	BM API.
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
	all mesh elements should share this beginning layout
	we can pack this a little tighter now... 
	
	BMHeader *next, *prev;
	int EID;
	int eflag1, eflag2;
	short systemflag, type;
	struct BMFlagLayer *flags;
*/

/*Defines for BMHeader->type*/
#define BMESH_VERT 					1
#define BMESH_EDGE 					2
#define BMESH_FACE 					4
#define BMESH_LOOP 					8
#define BMESH_ALL					BMESH_VERT | BMESH_EDGE | BMESH_FACE | BMESH_LOOP

/*Masks for BMHeader->flag
	Note: Its entirely possible that any temporal flags should be moved
	into the dynamically allocated flag layers and only reserve BMHeader->flag
	for things like select, hide, ect
*/	
#define BMESH_SELECT				1
#define BMESH_HIDDEN				2
#define BMESH_DIRTY					4			/*Not used yet*/
#define BMESH_NEW					8			
#define BMESH_OVERLAP				16			/*used by bmesh_verts_in_face*/
#define BMESH_EDGEVERT 				32 			/*used by bmesh_make_ngon*/
#define BMESH_DELETE				64
#define BMESH_AUX1					128 			/*different for edges/verts/faces/ect*/
#define BMESH_AUX2					256 			/*different for edges/verts/faces/ect*/
#define BMESH_AUX3					512 			/*different for edges/verts/faces/ect*/

#define BMESH_SHARP					BMESH_AUX1			/*for edges*/
#define BMESH_SEAM					BMESH_AUX2			/*for edges*/
#define BMESH_FGON					BMESH_AUX3			/*for edges, to be depreceated*/

#define BMESH_SMOOTH				BMESH_AUX1			/*for faces*/
#define BMESH_TEMP_FLAGS			BMESH_DIRTY|BMESH_NEW|BMESH_OVERLAP|BMESH_EDGEVERT|BMESH_DELETE

/*All Mesh elements start with this structure*/
typedef struct BMHeader
{
	struct BMHeader *next, *prev;
	int 		EID;
	int 		flag;								/*mesh flags, never to be (ab)used by the api itself!*/
	int			eflag1, eflag2;						/*Flags used by eulers. Try and get rid of/minimize some of these*/
	short		type, pad1, pad2, pad3;				/*Type of element this is head to*/
	struct BMFlagLayer *flags;					/*Dynamically allocated block of flag layers for operators to use*/
} BMHeader;


/*Used for circular linked list functions*/
typedef struct BMNode{
	struct BMNode *next, *prev;
	void *data;
}BMNode;

/*Used by operator API to give each operator private flag space
	-Perhaps want to change this to have a union for storing float/long/pointer/ect
	-pflag should be used for system/API stuff
*/

typedef struct BMFlagLayer{
	int f1;
	short mask, pflag;
}BMFlagLayer;

struct BMOperator;
typedef struct BMesh
{
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
	struct BLI_mempool *flagpool;								/*memory pool for dynamically allocated flag layers*/
	int stackdepth;												/*current depth of operator stack*/
	int totflags;												/*total number of tool flag layers*/
}BMesh;

typedef struct BMVert
{	
	struct BMHeader head;
	float co[3];									
	float no[3];									
	struct BMEdge *edge;
	void *data;
	void *tmp;													/*what?*/
	float bweight;												/*please, someone just get rid of me...*/
} BMVert;

typedef struct BMEdge
{
	struct BMHeader head;
	struct BMVert *v1, *v2;
	struct BMNode d1, d2;
	struct BMLoop *loop;
	void *data;
	float crease, bweight;										/*make these custom data.... no really, please....*/
}BMEdge;

typedef struct BMLoop 
{	
	struct BMHeader head;
	struct BMNode radial;
	struct BMVert *v;
	struct BMEdge *e;
	struct BMFace *f;	
	void *data;
}BMLoop;

typedef struct BMFace
{
	struct BMHeader head;
	struct BMLoop *loopbase;
	unsigned int len;
	void *data;
	float no[3];
	unsigned short mat_nr;									/*custom data again, and get rid of the unsigned short nonsense...*/
}BMFace;

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
void BM_Copy_Attributes(struct BMesh *source_mesh, struct BMesh *target_mesh, void *source, void *target);
void BM_Delete_Face(struct BMesh *bm, struct BMFace *f);
void BM_Delete_Edge(struct BMesh *bm, struct BMVert *v);
void BM_Delete_Vert(struct BMesh *bm, struct BMVert *v);

/*Modification*/
struct BMFace *BM_Join_Faces(struct BMesh *bm, struct BMFace *f1, struct BMFace *f2, struct BMEdge *e, int calcnorm, int weldUVs);
struct BMFace *BM_Split_Face(struct BMesh *bm, struct BMFace *f, struct BMVert *v1, struct BMVert *v2, struct BMLoop **nl, struct BMEdge *example, int calcnorm);
void BM_Collapse_Vert(struct BMesh *bm, struct BMEdge *ke, struct BMVert *kv, float fac, int calcnorm);
struct BMVert *BM_Split_Edge(struct BMesh *bm, struct BMVert *v, struct BMEdge *e, struct BMEdge **ne, float percent, int calcnorm);
struct BMVert  *BM_Split_Edge_Multi(struct BMesh *bm, struct BMEdge *e, int numcuts);

/*Interpolation*/
void BM_Data_Interp_From_Verts(struct BMesh *bm, struct BMVert *v1, struct BMVert *v2, struct BMVert *v, float fac);
void BM_Data_Facevert_Edgeinterp(struct BMesh *bm, struct BMVert *v1, struct BMVert *v2, struct BMVert *v, struct BMEdge *e1, float fac);
//void bmesh_data_interp_from_face(struct BMesh *bm, struct BMFace *source, struct BMFace *target);

/*include the rest of the API*/
#include "bmesh_filters.h"
#include "bmesh_iterators.h"
#include "bmesh_marking.h"
#include "bmesh_operators.h"
#include "bmesh_queries.h"
#endif
