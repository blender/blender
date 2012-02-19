/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2004 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Geoffrey Bantle.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BKE_BMESH_H__
#define __BKE_BMESH_H__

/** \file BKE_bmesh.h
 *  \ingroup bke
 *  \since January 2007
 *  \brief BMesh modeler structure and functions.
 *
 */

#include "DNA_listBase.h"
#include "BLI_utildefines.h"
#include "BLI_ghash.h"
#include "BLI_mempool.h"
#include "BLI_memarena.h"
#include "DNA_image_types.h"
#include "BKE_DerivedMesh.h"
//XXX #include "transform.h"
#include "bmesh.h"

/*forward declerations*/
struct BME_Vert;
struct BME_Edge;
struct BME_Poly;
struct BME_Loop;

/*NOTE: this is the bmesh 1.0 code.  it's completely outdated.*/

/*Notes on further structure Cleanup:
	-Remove the tflags, they belong in custom data layers
	-Remove the eflags completely, they are mostly not used
	-Remove the selection/vis/bevel weight flag/values ect and move them to custom data
	-Remove EID member and move to custom data
	-Add a radial cycle length, disk cycle length and loop cycle length attributes to custom data and have eulers maintain/use them if present.
	-Move data such as vertex coordinates/normals to custom data and leave pointers in structures to active layer data.
	-Remove BME_CycleNode structure?
*/
typedef struct BME_CycleNode{
	struct BME_CycleNode *next, *prev;
	void *data;
} BME_CycleNode;

typedef struct BME_Mesh
{
	ListBase verts, edges, polys;
	/*memory pools used for storing mesh elements*/
	struct BLI_mempool *vpool;
	struct BLI_mempool *epool;
	struct BLI_mempool *ppool;
	struct BLI_mempool *lpool;
	/*some scratch arrays used by eulers*/
	struct BME_Vert **vtar;
	struct BME_Edge **edar;
	struct BME_Loop **lpar;
	struct BME_Poly **plar;
	int vtarlen, edarlen, lparlen, plarlen;
	int totvert, totedge, totpoly, totloop;				/*record keeping*/
	int nextv, nexte, nextp, nextl;						/*Next element ID for verts/edges/faces/loops. Never reused*/
	struct CustomData vdata, edata, pdata, ldata;	/*Custom Data Layer information*/
} BME_Mesh;

typedef struct BME_Vert
{
	struct BME_Vert *next, *prev;
	int	EID;
	float co[3];									
	float no[3];									
	struct BME_Edge *edge;							/*first edge in the disk cycle for this vertex*/
	void *data;										/*custom vertex data*/
	int eflag1, eflag2;								/*reserved for use by eulers*/
	int tflag1, tflag2;								/*reserved for use by tools*/
	unsigned short flag, h;
	float bweight;
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
	unsigned short flag, h;
	float crease, bweight;
} BME_Edge;

typedef struct BME_Loop 
{	
	struct BME_Loop *next, *prev;					/*circularly linked list around face*/
	int EID;
	struct BME_CycleNode radial;					/*circularly linked list used to find faces around an edge*/
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
	unsigned int len;								/*total length of the face. Eulers should preserve this data*/
	void *data;										/*custom face data*/
	int eflag1, eflag2;								/*reserved for use by eulers*/
	int tflag1, tflag2;								/*reserved for use by tools*/
	unsigned short flag, h, mat_nr;
} BME_Poly;

/*EDGE UTILITIES*/
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

/*LOOP CYCLE*/
struct BME_Loop *BME_loop_find_loop(struct BME_Poly *f, struct BME_Vert *v);

/*MESH CREATION/DESTRUCTION*/
struct BME_Mesh *BME_make_mesh(int allocsize[4]);
void BME_free_mesh(struct BME_Mesh *bm);
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

/* bevel tool defines */
/* element flags */
#define BME_BEVEL_ORIG			1
#define BME_BEVEL_BEVEL			(1<<1)
#define BME_BEVEL_NONMAN		(1<<2)
#define BME_BEVEL_WIRE			(1<<3)

/* tool options */
#define BME_BEVEL_SELECT		1
#define BME_BEVEL_VERT			(1<<1)
#define BME_BEVEL_RADIUS		(1<<2)
#define BME_BEVEL_ANGLE			(1<<3)
#define BME_BEVEL_WEIGHT		(1<<4)
//~ #define BME_BEVEL_EWEIGHT		(1<<4)
//~ #define BME_BEVEL_VWEIGHT		(1<<5)
#define BME_BEVEL_PERCENT		(1<<6)
#define BME_BEVEL_EMIN			(1<<7)
#define BME_BEVEL_EMAX			(1<<8)
#define BME_BEVEL_RUNNING		(1<<9)
#define BME_BEVEL_RES			(1<<10)

#define BME_BEVEL_EVEN			(1<<11) /* this is a new setting not related to old (trunk bmesh bevel code) but adding
                                         * here because they are mixed - campbell */
#define BME_BEVEL_DIST			(1<<12) /* same as above */

typedef struct BME_TransData {
	BMesh *bm; /* the bmesh the vert belongs to */
	BMVert *v;  /* pointer to the vert this tdata applies to */
	float co[3];  /* the original coordinate */
	float org[3]; /* the origin */
	float vec[3]; /* a directional vector; always, always normalize! */
	void *loc;    /* a pointer to the data to transform (likely the vert's cos) */
	float factor; /* primary scaling factor; also accumulates number of weighted edges for beveling tool */
	float weight; /* another scaling factor; used primarily for propogating vertex weights to transforms; */
				  /* weight is also used across recursive bevels to help with the math */
	float maxfactor; /* the unscaled, original factor (used only by "edge verts" in recursive beveling) */
	float *max;   /* the maximum distance this vert can be transformed; negative is infinite
				   * it points to the "parent" maxfactor (where maxfactor makes little sense)
				   * where the max limit is stored (limits are stored per-corner) */
} BME_TransData;

typedef struct BME_TransData_Head {
	GHash *gh;       /* the hash structure for element lookup */
	MemArena *ma;    /* the memory "pool" we will be drawing individual elements from */
	int len;
} BME_TransData_Head;

typedef struct BME_Glob { /* stored in Global G for Transform() purposes */
	BMesh *bm;
	BME_TransData_Head *td;
	struct TransInfo *Trans; /* a pointer to the global Trans struct */
	int imval[2]; /* for restoring original mouse co when initTransform() is called multiple times */
	int options;
	int res;
} BME_Glob;

struct BME_TransData *BME_get_transdata(struct BME_TransData_Head *td, struct BMVert *v);
void BME_free_transdata(struct BME_TransData_Head *td);
float *BME_bevel_calc_polynormal(struct BME_Poly *f, struct BME_TransData_Head *td);
struct BMesh *BME_bevel(struct BMEditMesh *em, float value, int res, int options, int defgrp_index, float angle, BME_TransData_Head **rtd);

#endif
