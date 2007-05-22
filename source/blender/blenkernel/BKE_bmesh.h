/**
 * BKE_bmesh.h    jan 2007
 *
 *	BMesh modeler structure and functions.
 *
 * $Id: BKE_bmesh.h,v 1.00 2007/01/17 17:42:01 Briggs Exp $
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
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
 * Contributor(s): Geoffrey Bantle.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#ifndef BKE_BMESH_H
#define BKE_BMESH_H

#include "DNA_listBase.h"
#include "BLI_ghash.h"
#include "DNA_customdata_types.h"

struct BME_Vert;
struct BME_Edge;
struct BME_Poly;
struct BME_Loop;
struct RetopoPaintData;
struct DerivedMesh;

typedef struct BME_CycleNode{
	struct BME_CycleNode *next, *prev;
	void *data;
} BME_CycleNode;

typedef struct BME_Mesh
{
	ListBase verts, edges, polys, loops;
	int lock;										/*if set, all calls to eulers will fail.*/
	struct BME_Mesh *backup;						/*full copy of the mesh*/
	int totvert, totedge, totpoly, totloop;			/*record keeping*/
	int nextv, nexte, nextp, nextl;					/*Next element ID for verts/edges/faces/loops. Never reused*/
	struct CustomData vdata, edata, pdata, ldata;	/*Custom Data Layer information*/
	struct DerivedMesh *derivedFinal, *derivedCage;
	struct RetopoPaintData *retopo_paint_data; /*here for temporary code compatibility only*/
	int lastDataMask;
} BME_Mesh;

//60, 52, 52, 12 704
//60, 52, 84 


typedef struct BME_Vert
{
	struct BME_Vert *next, *prev;
	int	EID;
	struct BME_Edge *edge;							/*first edge in the disk cycle for this vertex*/
	float no[3];									/*vertex normal. Actually pointer to custom data block*/
	float co[3];									/*vertex location. Actually pointer to custom data block*/
	void *data;										/*custom vertex data*/
	int eflag1, eflag2;								/*reserved for use by eulers*/
	int tflag1, tflag2;								/*reserved for use by tools*/
	unsigned short flag, h;
} BME_Vert;

typedef struct BME_Edge
{
	struct BME_Edge *next, *prev;
	int EID;
	struct BME_Vert *v1, *v2;						/*note that order of vertex pointers means nothing to eulers*/
	struct BME_CycleNode d1, d2;					/*disk cycle nodes for v1 and v2 respectivley*/
	struct BME_Loop *loop;							/*first BME_Loop in the radial cycle around this edge*/
	void *data;										/*custom edge data*/
	int eflag1, eflag2;								/*reserved for use by eulers*/
	int tflag1, tflag2;								/*reserved for use by tools*/
	unsigned char flag, h, crease;
} BME_Edge;

typedef struct BME_Loop 
{	
	struct BME_Loop *next, *prev;					/*circularly linked list around face*/
	int EID;
	struct BME_CycleNode radial;					/*circularly linked list used to find faces around an edge*/
	struct BME_CycleNode *gref;						/*pointer to loop ref. Nasty.*/
	struct BME_Vert *v;								/*vertex that this loop starts at.*/
	struct BME_Edge *e;								/*edge this loop belongs to*/
	struct BME_Poly *f;								/*face this loop belongs to*/	
	void *data;										/*custom per face vertex data*/
	int eflag1, eflag2;								/*reserved for use by eulers*/
	int tflag1, tflag2;								/*reserved for use by tools*/
	unsigned short flag, h;
} BME_Loop;

typedef struct BME_Poly
{
	struct BME_Poly *next, *prev;
	int EID;
	struct BME_Loop *loopbase;						/*First editloop around Polygon.*/
	struct ListBase holes;							/*list of inner loops in the face*/
	unsigned int len;								/*total length of the face. Eulers should preserve this data*/
	void *data;										/*custom face data*/
	int eflag1, eflag2;								/*reserved for use by eulers*/
	int tflag1, tflag2;								/*reserved for use by tools*/
	unsigned short flag, h, mat_nr;
} BME_Poly;

//*EDGE UTILITIES*/
int BME_verts_in_edge(struct BME_Vert *v1, struct BME_Vert *v2, struct BME_Edge *e);
int BME_vert_in_edge(struct BME_Edge *e, BME_Vert *v);
struct BME_Vert *BME_edge_getothervert(struct BME_Edge *e, struct BME_Vert *v);

/*GENERAL CYCLE*/
int BME_cycle_length(void *h);

/*DISK CYCLE*/
struct BME_Edge *BME_disk_nextedge(struct BME_Edge *e, struct BME_Vert *v); 
struct BME_CycleNode *BME_disk_getpointer(struct BME_Edge *e, struct BME_Vert *v);
struct BME_Edge *BME_disk_next_edgeflag(struct BME_Edge *e, struct BME_Vert *v, int eflag, int tflag);
int BME_disk_count_edgeflag(struct BME_Vert *v, int eflag, int tflag);

/*RADIAL CYCLE*/
struct BME_Loop *BME_radial_nextloop(struct BME_Loop *l);
int BME_radial_find_face(struct BME_Edge *e,struct BME_Poly *f);

/*MESH CREATION/DESTRUCTION*/
struct BME_Mesh *BME_make_mesh(void);
void BME_free_mesh(struct BME_Mesh *bm);
struct BME_Mesh *BME_copy_mesh(struct BME_Mesh *bm);
/*FULL MESH VALIDATION*/
int BME_validate_mesh(struct BME_Mesh *bm, int halt);
/*ENTER/EXIT MODELLING LOOP*/
int BME_model_begin(struct BME_Mesh *bm);
void BME_model_end(struct BME_Mesh *bm);

/*MESH CONSTRUCTION API.*/
/*MAKE*/
struct BME_Vert *BME_MV(struct BME_Mesh *bm, float *vec);
struct BME_Edge *BME_ME(struct BME_Mesh *bm, struct BME_Vert *v1, struct BME_Vert *v2);
struct BME_Poly *BME_MF(struct BME_Mesh *bm, struct BME_Vert *v1, struct BME_Vert *v2, struct BME_Edge **elist, int len);
/*KILL*/
int BME_KV(struct BME_Mesh *bm, struct BME_Vert *v);
int BME_KE(struct BME_Mesh *bm, struct BME_Edge *e);
int BME_KF(struct BME_Mesh *bm, struct BME_Poly *bply);
/*SPLIT*/
struct BME_Vert *BME_SEMV(struct BME_Mesh *bm, struct BME_Vert *tv, struct BME_Edge *e, struct BME_Edge **re);
struct BME_Poly *BME_SFME(struct BME_Mesh *bm, struct BME_Poly *f, struct BME_Vert *v1, struct BME_Vert *v2, struct BME_Loop **rl);
/*JOIN*/
int BME_JEKV(struct BME_Mesh *bm, struct BME_Edge *ke, struct BME_Vert *kv);
struct BME_Poly *BME_JFKE(struct BME_Mesh *bm, struct BME_Poly *f1, struct BME_Poly *f2,struct BME_Edge *e); /*no reason to return BME_Poly pointer?*/
/*NORMAL FLIP(Is its own inverse)*/
int BME_loop_reverse(struct BME_Mesh *bm, struct BME_Poly *f);

/*TOOLS CODE*/
struct BME_Loop *BME_inset_edge(struct BME_Mesh *bm, struct BME_Loop *l, struct BME_Poly *f);
struct BME_Poly *BME_inset_poly(struct BME_Mesh *bm, struct BME_Poly *f);
#endif
