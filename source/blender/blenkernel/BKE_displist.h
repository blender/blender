/*
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
 */

#pragma once

/** \file
 * \ingroup bke
 * \brief display list (or rather multi purpose list) stuff.
 */
#include "BKE_customdata.h"
#include "DNA_customdata_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** #DispList.type */
enum {
  /** A closed polygon (that can be filled).  */
  DL_POLY = 0,
  /** An open polygon.  */
  DL_SEGM = 1,
  /** A grid surface that respects #DL_CYCL_U & #DL_CYCL_V.  */
  DL_SURF = 2,
  /** Triangles. */
  DL_INDEX3 = 4,
  /** Quads, with support for triangles (when values of the 3rd and 4th indices match). */
  DL_INDEX4 = 5,
  // DL_VERTCOL = 6, /* UNUSED */
  /** Isolated points. */
  DL_VERTS = 7,
};

/** #DispList.type */
enum {
  /** U/V swapped here compared with #Nurb.flagu, #Nurb.flagv and #CU_NURB_CYCLIC */
  DL_CYCL_U = (1 << 0),
  DL_CYCL_V = (1 << 1),

  DL_FRONT_CURVE = (1 << 2),
  DL_BACK_CURVE = (1 << 3),
};

/* prototypes */

struct Depsgraph;
struct ListBase;
struct Mesh;
struct Object;
struct Scene;

/* used for curves, nurbs, mball, importing */
typedef struct DispList {
  struct DispList *next, *prev;
  short type, flag;
  int parts, nr;
  short col, rt; /* rt used by initrenderNurbs */
  float *verts, *nors;
  int *index;
  int charidx;
  int totindex; /* indexed array drawing surfaces */
} DispList;

void BKE_displist_copy(struct ListBase *lbn, const struct ListBase *lb);
void BKE_displist_elem_free(DispList *dl);
DispList *BKE_displist_find_or_create(struct ListBase *lb, int type);
DispList *BKE_displist_find(struct ListBase *lb, int type);
void BKE_displist_normals_add(struct ListBase *lb);
void BKE_displist_count(const struct ListBase *lb, int *totvert, int *totface, int *tottri);
void BKE_displist_free(struct ListBase *lb);
bool BKE_displist_has_faces(const struct ListBase *lb);

void BKE_displist_make_surf(struct Depsgraph *depsgraph,
                            const struct Scene *scene,
                            struct Object *ob,
                            struct ListBase *dispbase,
                            struct Mesh **r_final,
                            const bool for_render,
                            const bool for_orco);
void BKE_displist_make_curveTypes(struct Depsgraph *depsgraph,
                                  const struct Scene *scene,
                                  struct Object *ob,
                                  const bool for_render,
                                  const bool for_orco);
void BKE_displist_make_curveTypes_forRender(struct Depsgraph *depsgraph,
                                            const struct Scene *scene,
                                            struct Object *ob,
                                            struct ListBase *dispbase,
                                            struct Mesh **r_final,
                                            const bool for_orco);
void BKE_displist_make_mball(struct Depsgraph *depsgraph, struct Scene *scene, struct Object *ob);
void BKE_displist_make_mball_forRender(struct Depsgraph *depsgraph,
                                       struct Scene *scene,
                                       struct Object *ob,
                                       struct ListBase *dispbase);

bool BKE_curve_calc_modifiers_pre(struct Depsgraph *depsgraph,
                                  const struct Scene *scene,
                                  struct Object *ob,
                                  struct ListBase *source_nurb,
                                  struct ListBase *target_nurb,
                                  const bool for_render);
bool BKE_displist_surfindex_get(
    const struct DispList *dl, int a, int *b, int *p1, int *p2, int *p3, int *p4);

void BKE_displist_fill(const struct ListBase *dispbase,
                       struct ListBase *to,
                       const float normal_proj[3],
                       const bool flip_normal);

float BKE_displist_calc_taper(struct Depsgraph *depsgraph,
                              const struct Scene *scene,
                              struct Object *taperobj,
                              int cur,
                              int tot);

void BKE_displist_minmax(const struct ListBase *dispbase, float min[3], float max[3]);

#ifdef __cplusplus
}
#endif
