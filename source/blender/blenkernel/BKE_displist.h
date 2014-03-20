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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BKE_DISPLIST_H__
#define __BKE_DISPLIST_H__

/** \file BKE_displist.h
 *  \ingroup bke
 *  \brief display list (or rather multi purpose list) stuff.
 */
#include "DNA_customdata_types.h"
#include "BKE_customdata.h"

/* dl->type */
#define DL_POLY                 0
#define DL_SEGM                 1
#define DL_SURF                 2
#define DL_INDEX3               4
#define DL_INDEX4               5
// #define DL_VERTCOL              6  // UNUSED
#define DL_VERTS                7

/* dl->flag */
#define DL_CYCL_U       1
#define DL_CYCL_V       2
#define DL_FRONT_CURVE  4
#define DL_BACK_CURVE   8


/* prototypes */

struct Base;
struct Scene;
struct Object;
struct Curve;
struct ListBase;
struct Material;
struct Bone;
struct Mesh;
struct DerivedMesh;
struct EvaluationContext;

/* used for curves, nurbs, mball, importing */
typedef struct DispList {
	struct DispList *next, *prev;
	short type, flag;
	int parts, nr;
	short col, rt;              /* rt used by initrenderNurbs */
	float *verts, *nors;
	int *index;
	unsigned int *col1, *col2;
	int charidx;
	int totindex;               /* indexed array drawing surfaces */

	unsigned int *bevelSplitFlag;
} DispList;

void BKE_displist_copy(struct ListBase *lbn, struct ListBase *lb);
void BKE_displist_elem_free(DispList *dl);
DispList *BKE_displist_find_or_create(struct ListBase *lb, int type);
DispList *BKE_displist_find(struct ListBase *lb, int type);
void BKE_displist_normals_add(struct ListBase *lb);
void BKE_displist_count(struct ListBase *lb, int *totvert, int *totface, int *tottri);
void BKE_displist_free(struct ListBase *lb);
bool BKE_displist_has_faces(struct ListBase *lb);

void BKE_displist_make_surf(struct Scene *scene, struct Object *ob, struct ListBase *dispbase, struct DerivedMesh **r_dm_final,
                            const bool for_render, const bool for_orco, const bool use_render_resolution);
void BKE_displist_make_curveTypes(struct Scene *scene, struct Object *ob, const bool for_orco);
void BKE_displist_make_curveTypes_forRender(struct Scene *scene, struct Object *ob, struct ListBase *dispbase, struct DerivedMesh **r_dm_final,
                                            const bool for_orco, const bool use_render_resolution);
void BKE_displist_make_curveTypes_forOrco(struct Scene *scene, struct Object *ob, struct ListBase *dispbase);
void BKE_displist_make_mball(struct EvaluationContext *eval_ctx, struct Scene *scene, struct Object *ob);
void BKE_displist_make_mball_forRender(struct EvaluationContext *eval_ctx, struct Scene *scene, struct Object *ob, struct ListBase *dispbase);

bool BKE_displist_surfindex_get(DispList *dl, int a, int *b, int *p1, int *p2, int *p3, int *p4);
void BKE_displist_fill(struct ListBase *dispbase, struct ListBase *to, const float normal_proj[3], const bool flipnormal);

float BKE_displist_calc_taper(struct Scene *scene, struct Object *taperobj, int cur, int tot);

/* add Orco layer to the displist object which has got derived mesh and return orco */
float *BKE_displist_make_orco(struct Scene *scene, struct Object *ob, struct DerivedMesh *dm_final,
                              const bool for_render, const bool use_render_resolution);

void BKE_displist_minmax(struct ListBase *dispbase, float min[3], float max[3]);

#endif
