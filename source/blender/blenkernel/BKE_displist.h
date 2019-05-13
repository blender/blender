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

#ifndef __BKE_DISPLIST_H__
#define __BKE_DISPLIST_H__

/** \file
 * \ingroup bke
 * \brief display list (or rather multi purpose list) stuff.
 */
#include "DNA_customdata_types.h"
#include "BKE_customdata.h"

/* dl->type */
#define DL_POLY 0
#define DL_SEGM 1
#define DL_SURF 2
#define DL_INDEX3 4
#define DL_INDEX4 5
// #define DL_VERTCOL              6  // UNUSED
#define DL_VERTS 7

/* dl->flag */
enum {
  /** U/V swapped here compared with #Nurb.flagu, #Nurb.flagv and #CU_NURB_CYCLIC */
  DL_CYCL_U = (1 << 0),
  DL_CYCL_V = (1 << 1),

  DL_FRONT_CURVE = (1 << 2),
  DL_BACK_CURVE = (1 << 3),
};

/* prototypes */

struct Depsgraph;
struct LinkNode;
struct ListBase;
struct Main;
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

  unsigned int *bevel_split; /* BLI_bitmap */
} DispList;

void BKE_displist_copy(struct ListBase *lbn, struct ListBase *lb);
void BKE_displist_elem_free(DispList *dl);
DispList *BKE_displist_find_or_create(struct ListBase *lb, int type);
DispList *BKE_displist_find(struct ListBase *lb, int type);
void BKE_displist_normals_add(struct ListBase *lb);
void BKE_displist_count(struct ListBase *lb, int *totvert, int *totface, int *tottri);
void BKE_displist_free(struct ListBase *lb);
bool BKE_displist_has_faces(struct ListBase *lb);

void BKE_displist_make_surf(struct Depsgraph *depsgraph,
                            struct Scene *scene,
                            struct Object *ob,
                            struct ListBase *dispbase,
                            struct Mesh **r_final,
                            const bool for_render,
                            const bool for_orco);
void BKE_displist_make_curveTypes(struct Depsgraph *depsgraph,
                                  struct Scene *scene,
                                  struct Object *ob,
                                  const bool for_render,
                                  const bool for_orco,
                                  struct LinkNode *ob_cyclic_list);
void BKE_displist_make_curveTypes_forRender(struct Depsgraph *depsgraph,
                                            struct Scene *scene,
                                            struct Object *ob,
                                            struct ListBase *dispbase,
                                            struct Mesh **r_final,
                                            const bool for_orco,
                                            struct LinkNode *ob_cyclic_list);
void BKE_displist_make_mball(struct Depsgraph *depsgraph, struct Scene *scene, struct Object *ob);
void BKE_displist_make_mball_forRender(struct Depsgraph *depsgraph,
                                       struct Scene *scene,
                                       struct Object *ob,
                                       struct ListBase *dispbase);

bool BKE_displist_surfindex_get(DispList *dl, int a, int *b, int *p1, int *p2, int *p3, int *p4);
void BKE_displist_fill(struct ListBase *dispbase,
                       struct ListBase *to,
                       const float normal_proj[3],
                       const bool flipnormal);

float BKE_displist_calc_taper(
    struct Depsgraph *depsgraph, struct Scene *scene, struct Object *taperobj, int cur, int tot);

void BKE_displist_minmax(struct ListBase *dispbase, float min[3], float max[3]);

#endif
