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

/*NOTE: this is the bmesh 1.0 code.  it's completely outdated.*/

/* uncomment to use the new bevel operator as a modifier */
// #define USE_BM_BEVEL_OP_AS_MOD

/* bevel tool defines */
/* element flags */
#define BME_BEVEL_ORIG          1
#define BME_BEVEL_BEVEL         (1 << 1)
#define BME_BEVEL_NONMAN        (1 << 2)
#define BME_BEVEL_WIRE          (1 << 3)

/* tool options */
#define BME_BEVEL_SELECT        1
#define BME_BEVEL_VERT          (1 << 1)
#define BME_BEVEL_RADIUS        (1 << 2)
#define BME_BEVEL_ANGLE         (1 << 3)
#define BME_BEVEL_WEIGHT        (1 << 4)
//~ #define BME_BEVEL_EWEIGHT		(1<<4)
//~ #define BME_BEVEL_VWEIGHT		(1<<5)
#define BME_BEVEL_PERCENT       (1 << 6)
#define BME_BEVEL_EMIN          (1 << 7)
#define BME_BEVEL_EMAX          (1 << 8)
#define BME_BEVEL_RUNNING       (1 << 9)
#define BME_BEVEL_RES           (1 << 10)

#define BME_BEVEL_EVEN          (1 << 11) /* this is a new setting not related to old (trunk bmesh bevel code) but adding
	                                       * here because they are mixed - campbell */
#define BME_BEVEL_DIST          (1 << 12) /* same as above */

typedef struct BME_TransData {
	struct BMesh *bm; /* the bmesh the vert belongs to */
	struct BMVert *v;  /* pointer to the vert this tdata applies to */
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
	struct GHash *gh;       /* the hash structure for element lookup */
	struct MemArena *ma;    /* the memory "pool" we will be drawing individual elements from */
	int len;
} BME_TransData_Head;

/* this is no longer used */
typedef struct BME_Glob { /* stored in Global G for Transform() purposes */
	struct BMesh *bm;
	BME_TransData_Head *td;
	struct TransInfo *Trans; /* a pointer to the global Trans struct */
	int imval[2]; /* for restoring original mouse co when initTransform() is called multiple times */
	int options;
	int res;
} BME_Glob;

struct BME_TransData *BME_get_transdata(struct BME_TransData_Head *td, struct BMVert *v);
void BME_free_transdata(struct BME_TransData_Head *td);
struct BMesh *BME_bevel(struct BMEditMesh *em, float value, int res, int options, int defgrp_index, float angle,
                        BME_TransData_Head **rtd, int do_tessface);

#endif
