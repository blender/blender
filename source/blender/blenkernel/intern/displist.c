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

/** \file
 * \ingroup bke
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_curve_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_vfont_types.h"

#include "BLI_bitmap.h"
#include "BLI_linklist.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_memarena.h"
#include "BLI_scanfill.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BKE_anim_path.h"
#include "BKE_curve.h"
#include "BKE_displist.h"
#include "BKE_font.h"
#include "BKE_key.h"
#include "BKE_lattice.h"
#include "BKE_lib_id.h"
#include "BKE_mball.h"
#include "BKE_mball_tessellate.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_object.h"

#include "BLI_sys_types.h"  // for intptr_t support

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

static void boundbox_displist_object(Object *ob);

void BKE_displist_elem_free(DispList *dl)
{
  if (dl) {
    if (dl->verts) {
      MEM_freeN(dl->verts);
    }
    if (dl->nors) {
      MEM_freeN(dl->nors);
    }
    if (dl->index) {
      MEM_freeN(dl->index);
    }
    if (dl->bevel_split) {
      MEM_freeN(dl->bevel_split);
    }
    MEM_freeN(dl);
  }
}

void BKE_displist_free(ListBase *lb)
{
  DispList *dl;

  while ((dl = BLI_pophead(lb))) {
    BKE_displist_elem_free(dl);
  }
}

DispList *BKE_displist_find_or_create(ListBase *lb, int type)
{
  LISTBASE_FOREACH (DispList *, dl, lb) {
    if (dl->type == type) {
      return dl;
    }
  }

  DispList *dl = MEM_callocN(sizeof(DispList), "find_disp");
  dl->type = type;
  BLI_addtail(lb, dl);

  return dl;
}

DispList *BKE_displist_find(ListBase *lb, int type)
{
  LISTBASE_FOREACH (DispList *, dl, lb) {
    if (dl->type == type) {
      return dl;
    }
  }

  return NULL;
}

bool BKE_displist_has_faces(ListBase *lb)
{
  LISTBASE_FOREACH (DispList *, dl, lb) {
    if (ELEM(dl->type, DL_INDEX3, DL_INDEX4, DL_SURF)) {
      return true;
    }
  }

  return false;
}

void BKE_displist_copy(ListBase *lbn, ListBase *lb)
{
  BKE_displist_free(lbn);

  LISTBASE_FOREACH (const DispList *, dl, lb) {
    DispList *dln = MEM_dupallocN(dl);
    BLI_addtail(lbn, dln);
    dln->verts = MEM_dupallocN(dl->verts);
    dln->nors = MEM_dupallocN(dl->nors);
    dln->index = MEM_dupallocN(dl->index);

    if (dl->bevel_split) {
      dln->bevel_split = MEM_dupallocN(dl->bevel_split);
    }
  }
}

void BKE_displist_normals_add(ListBase *lb)
{
  float *vdata, *ndata, nor[3];
  float *v1, *v2, *v3, *v4;
  float *n1, *n2, *n3, *n4;
  int a, b, p1, p2, p3, p4;

  LISTBASE_FOREACH (DispList *, dl, lb) {
    if (dl->type == DL_INDEX3) {
      if (dl->nors == NULL) {
        dl->nors = MEM_callocN(sizeof(float[3]), "dlnors");

        if (dl->flag & DL_BACK_CURVE) {
          dl->nors[2] = -1.0f;
        }
        else {
          dl->nors[2] = 1.0f;
        }
      }
    }
    else if (dl->type == DL_SURF) {
      if (dl->nors == NULL) {
        dl->nors = MEM_callocN(sizeof(float[3]) * dl->nr * dl->parts, "dlnors");

        vdata = dl->verts;
        ndata = dl->nors;

        for (a = 0; a < dl->parts; a++) {

          if (BKE_displist_surfindex_get(dl, a, &b, &p1, &p2, &p3, &p4) == 0) {
            break;
          }

          v1 = vdata + 3 * p1;
          n1 = ndata + 3 * p1;
          v2 = vdata + 3 * p2;
          n2 = ndata + 3 * p2;
          v3 = vdata + 3 * p3;
          n3 = ndata + 3 * p3;
          v4 = vdata + 3 * p4;
          n4 = ndata + 3 * p4;

          for (; b < dl->nr; b++) {
            normal_quad_v3(nor, v1, v3, v4, v2);

            add_v3_v3(n1, nor);
            add_v3_v3(n2, nor);
            add_v3_v3(n3, nor);
            add_v3_v3(n4, nor);

            v2 = v1;
            v1 += 3;
            v4 = v3;
            v3 += 3;
            n2 = n1;
            n1 += 3;
            n4 = n3;
            n3 += 3;
          }
        }
        a = dl->parts * dl->nr;
        v1 = ndata;
        while (a--) {
          normalize_v3(v1);
          v1 += 3;
        }
      }
    }
  }
}

void BKE_displist_count(ListBase *lb, int *totvert, int *totface, int *tottri)
{
  LISTBASE_FOREACH (DispList *, dl, lb) {
    int vert_tot = 0;
    int face_tot = 0;
    int tri_tot = 0;
    bool cyclic_u = dl->flag & DL_CYCL_U;
    bool cyclic_v = dl->flag & DL_CYCL_V;

    switch (dl->type) {
      case DL_SURF: {
        int segments_u = dl->nr - (cyclic_u == false);
        int segments_v = dl->parts - (cyclic_v == false);
        vert_tot = dl->nr * dl->parts;
        face_tot = segments_u * segments_v;
        tri_tot = face_tot * 2;
        break;
      }
      case DL_INDEX3: {
        vert_tot = dl->nr;
        face_tot = dl->parts;
        tri_tot = face_tot;
        break;
      }
      case DL_INDEX4: {
        vert_tot = dl->nr;
        face_tot = dl->parts;
        tri_tot = face_tot * 2;
        break;
      }
      case DL_POLY:
      case DL_SEGM: {
        vert_tot = dl->nr * dl->parts;
        break;
      }
    }

    *totvert += vert_tot;
    *totface += face_tot;
    *tottri += tri_tot;
  }
}

bool BKE_displist_surfindex_get(DispList *dl, int a, int *b, int *p1, int *p2, int *p3, int *p4)
{
  if ((dl->flag & DL_CYCL_V) == 0 && a == (dl->parts) - 1) {
    return false;
  }

  if (dl->flag & DL_CYCL_U) {
    (*p1) = dl->nr * a;
    (*p2) = (*p1) + dl->nr - 1;
    (*p3) = (*p1) + dl->nr;
    (*p4) = (*p2) + dl->nr;
    (*b) = 0;
  }
  else {
    (*p2) = dl->nr * a;
    (*p1) = (*p2) + 1;
    (*p4) = (*p2) + dl->nr;
    (*p3) = (*p1) + dl->nr;
    (*b) = 1;
  }

  if ((dl->flag & DL_CYCL_V) && a == dl->parts - 1) {
    (*p3) -= dl->nr * dl->parts;
    (*p4) -= dl->nr * dl->parts;
  }

  return true;
}

/* ****************** make displists ********************* */
#ifdef __INTEL_COMPILER
/* ICC with the optimization -02 causes crashes. */
#  pragma intel optimization_level 1
#endif

static void curve_to_displist(const Curve *cu,
                              const ListBase *nubase,
                              const bool for_render,
                              ListBase *r_dispbase)
{
  const bool editmode = (!for_render && (cu->editnurb || cu->editfont));

  LISTBASE_FOREACH (Nurb *, nu, nubase) {
    if (nu->hide != 0 && editmode) {
      continue;
    }
    if (!BKE_nurb_check_valid_u(nu)) {
      continue;
    }

    const int resolution = (for_render && cu->resolu_ren != 0) ? cu->resolu_ren : nu->resolu;
    const bool is_cyclic = nu->flagu & CU_NURB_CYCLIC;
    const BezTriple *bezt_first = &nu->bezt[0];
    const BezTriple *bezt_last = &nu->bezt[nu->pntsu - 1];

    if (nu->type == CU_BEZIER) {
      int samples_len = 0;
      for (int i = 1; i < nu->pntsu; i++) {
        const BezTriple *prevbezt = &nu->bezt[i - 1];
        const BezTriple *bezt = &nu->bezt[i];
        if (prevbezt->h2 == HD_VECT && bezt->h1 == HD_VECT) {
          samples_len++;
        }
        else {
          samples_len += resolution;
        }
      }
      if (is_cyclic) {
        /* If the curve is cyclic, sample the last edge between the last and first points. */
        if (bezt_first->h1 == HD_VECT && bezt_last->h2 == HD_VECT) {
          samples_len++;
        }
        else {
          samples_len += resolution;
        }
      }
      else {
        /* Otherwise, we only need one additional sample to complete the last edge. */
        samples_len++;
      }

      /* Check that there are more than two points so the curve doesn't loop back on itself. This
       * needs to be separate from `is_cyclic` because cyclic sampling can work with two points
       * and resolution > 1.  */
      const bool use_cyclic_sample = is_cyclic && (samples_len != 2);

      DispList *dl = MEM_callocN(sizeof(DispList), __func__);
      /* Add one to the length because of 'BKE_curve_forward_diff_bezier'. */
      dl->verts = MEM_mallocN(sizeof(float[3]) * (samples_len + 1), "dlverts");
      BLI_addtail(r_dispbase, dl);
      dl->parts = 1;
      dl->nr = samples_len;
      dl->col = nu->mat_nr;
      dl->charidx = nu->charidx;

      dl->type = use_cyclic_sample ? DL_POLY : DL_SEGM;

      float *data = dl->verts;
      for (int i = 1; i < nu->pntsu; i++) {
        const BezTriple *prevbezt = &nu->bezt[i - 1];
        const BezTriple *bezt = &nu->bezt[i];

        if (prevbezt->h2 == HD_VECT && bezt->h1 == HD_VECT) {
          copy_v3_v3(data, prevbezt->vec[1]);
          data += 3;
        }
        else {
          for (int j = 0; j < 3; j++) {
            BKE_curve_forward_diff_bezier(prevbezt->vec[1][j],
                                          prevbezt->vec[2][j],
                                          bezt->vec[0][j],
                                          bezt->vec[1][j],
                                          data + j,
                                          resolution,
                                          sizeof(float[3]));
          }
          data += 3 * resolution;
        }
      }
      if (is_cyclic) {
        if (bezt_first->h1 == HD_VECT && bezt_last->h2 == HD_VECT) {
          copy_v3_v3(data, bezt_last->vec[1]);
        }
        else {
          for (int j = 0; j < 3; j++) {
            BKE_curve_forward_diff_bezier(bezt_last->vec[1][j],
                                          bezt_last->vec[2][j],
                                          bezt_first->vec[0][j],
                                          bezt_first->vec[1][j],
                                          data + j,
                                          resolution,
                                          sizeof(float[3]));
          }
        }
      }
      else {
        copy_v3_v3(data, bezt_last->vec[1]);
      }
    }
    else if (nu->type == CU_NURBS) {
      const int len = (resolution * SEGMENTSU(nu));
      DispList *dl = MEM_callocN(sizeof(DispList), __func__);
      dl->verts = MEM_mallocN(len * sizeof(float[3]), "dlverts");
      BLI_addtail(r_dispbase, dl);
      dl->parts = 1;
      dl->nr = len;
      dl->col = nu->mat_nr;
      dl->charidx = nu->charidx;
      dl->type = is_cyclic ? DL_POLY : DL_SEGM;

      BKE_nurb_makeCurve(nu, dl->verts, NULL, NULL, NULL, resolution, sizeof(float[3]));
    }
    else if (nu->type == CU_POLY) {
      const int len = nu->pntsu;
      DispList *dl = MEM_callocN(sizeof(DispList), __func__);
      dl->verts = MEM_mallocN(len * sizeof(float[3]), "dlverts");
      BLI_addtail(r_dispbase, dl);
      dl->parts = 1;
      dl->nr = len;
      dl->col = nu->mat_nr;
      dl->charidx = nu->charidx;
      dl->type = (is_cyclic && (dl->nr != 2)) ? DL_POLY : DL_SEGM;

      float(*coords)[3] = (float(*)[3])dl->verts;
      for (int i = 0; i < len; i++) {
        const BPoint *bp = &nu->bp[i];
        copy_v3_v3(coords[i], bp->vec);
      }
    }
  }
}

/**
 * \param normal_proj: Optional normal that's used to project the scanfill verts into 2d coords.
 * Pass this along if known since it saves time calculating the normal.
 * This is also used to initialize #DispList.nors (one normal per display list).
 * \param flipnormal: Flip the normal (same as passing \a normal_proj negated)
 */
void BKE_displist_fill(const ListBase *dispbase,
                       ListBase *to,
                       const float normal_proj[3],
                       const bool flip_normal)
{
  if (dispbase == NULL) {
    return;
  }
  if (BLI_listbase_is_empty(dispbase)) {
    return;
  }

  const int scanfill_flag = BLI_SCANFILL_CALC_REMOVE_DOUBLES | BLI_SCANFILL_CALC_POLYS |
                            BLI_SCANFILL_CALC_HOLES;

  MemArena *sf_arena = BLI_memarena_new(BLI_SCANFILL_ARENA_SIZE, __func__);

  short colnr = 0;
  int charidx = 0;
  bool should_continue = true;
  while (should_continue) {
    should_continue = false;
    bool nextcol = false;

    ScanFillContext sf_ctx;
    BLI_scanfill_begin_arena(&sf_ctx, sf_arena);

    int totvert = 0;
    short dl_flag_accum = 0;
    short dl_rt_accum = 0;
    LISTBASE_FOREACH (const DispList *, dl, dispbase) {
      if (dl->type == DL_POLY) {
        if (charidx < dl->charidx) {
          should_continue = true;
        }
        else if (charidx == dl->charidx) { /* character with needed index */
          if (colnr == dl->col) {

            sf_ctx.poly_nr++;

            /* Make verts and edges. */
            ScanFillVert *sf_vert = NULL;
            ScanFillVert *sf_vert_last = NULL;
            ScanFillVert *sf_vert_new = NULL;
            for (int i = 0; i < dl->nr; i++) {
              sf_vert_last = sf_vert;
              sf_vert = BLI_scanfill_vert_add(&sf_ctx, &dl->verts[3 * i]);
              totvert++;
              if (sf_vert_last == NULL) {
                sf_vert_new = sf_vert;
              }
              else {
                BLI_scanfill_edge_add(&sf_ctx, sf_vert_last, sf_vert);
              }
            }

            if (sf_vert != NULL && sf_vert_new != NULL) {
              BLI_scanfill_edge_add(&sf_ctx, sf_vert, sf_vert_new);
            }
          }
          else if (colnr < dl->col) {
            /* got poly with next material at current char */
            should_continue = true;
            nextcol = true;
          }
        }
        dl_flag_accum |= dl->flag;
        dl_rt_accum |= dl->rt;
      }
    }

    const int triangles_len = BLI_scanfill_calc_ex(&sf_ctx, scanfill_flag, normal_proj);
    if (totvert != 0 && triangles_len != 0) {
      DispList *dlnew = MEM_callocN(sizeof(DispList), "filldisplist");
      dlnew->type = DL_INDEX3;
      dlnew->flag = (dl_flag_accum & (DL_BACK_CURVE | DL_FRONT_CURVE));
      dlnew->rt = (dl_rt_accum & CU_SMOOTH);
      dlnew->col = colnr;
      dlnew->nr = totvert;
      dlnew->parts = triangles_len;

      dlnew->index = MEM_mallocN(sizeof(int[3]) * triangles_len, "dlindex");
      dlnew->verts = MEM_mallocN(sizeof(float[3]) * totvert, "dlverts");

      /* vert data */
      int i;
      LISTBASE_FOREACH_INDEX (ScanFillVert *, sf_vert, &sf_ctx.fillvertbase, i) {
        copy_v3_v3(&dlnew->verts[3 * i], sf_vert->co);
        sf_vert->tmp.i = i; /* Index number. */
      }

      /* index data */
      int *index = dlnew->index;
      LISTBASE_FOREACH (ScanFillFace *, sf_tri, &sf_ctx.fillfacebase) {
        index[0] = sf_tri->v1->tmp.i;
        index[1] = flip_normal ? sf_tri->v3->tmp.i : sf_tri->v2->tmp.i;
        index[2] = flip_normal ? sf_tri->v2->tmp.i : sf_tri->v3->tmp.i;
        index += 3;
      }

      BLI_addhead(to, dlnew);
    }
    BLI_scanfill_end_arena(&sf_ctx, sf_arena);

    if (nextcol) {
      /* stay at current char but fill polys with next material */
      colnr++;
    }
    else {
      /* switch to next char and start filling from first material */
      charidx++;
      colnr = 0;
    }
  }

  BLI_memarena_free(sf_arena);
  /* do not free polys, needed for wireframe display */
}

static void bevels_to_filledpoly(const Curve *cu, ListBase *dispbase)
{
  ListBase front = {NULL, NULL};
  ListBase back = {NULL, NULL};

  LISTBASE_FOREACH (const DispList *, dl, dispbase) {
    if (dl->type == DL_SURF) {
      if ((dl->flag & DL_CYCL_V) && (dl->flag & DL_CYCL_U) == 0) {
        if ((cu->flag & CU_BACK) && (dl->flag & DL_BACK_CURVE)) {
          DispList *dlnew = MEM_callocN(sizeof(DispList), __func__);
          BLI_addtail(&front, dlnew);
          dlnew->verts = MEM_mallocN(sizeof(float[3]) * dl->parts, __func__);
          dlnew->nr = dl->parts;
          dlnew->parts = 1;
          dlnew->type = DL_POLY;
          dlnew->flag = DL_BACK_CURVE;
          dlnew->col = dl->col;
          dlnew->charidx = dl->charidx;

          const float *old_verts = dl->verts;
          float *new_verts = dlnew->verts;
          for (int i = 0; i < dl->parts; i++) {
            copy_v3_v3(new_verts, old_verts);
            new_verts += 3;
            old_verts += 3 * dl->nr;
          }
        }
        if ((cu->flag & CU_FRONT) && (dl->flag & DL_FRONT_CURVE)) {
          DispList *dlnew = MEM_callocN(sizeof(DispList), __func__);
          BLI_addtail(&back, dlnew);
          dlnew->verts = MEM_mallocN(sizeof(float[3]) * dl->parts, __func__);
          dlnew->nr = dl->parts;
          dlnew->parts = 1;
          dlnew->type = DL_POLY;
          dlnew->flag = DL_FRONT_CURVE;
          dlnew->col = dl->col;
          dlnew->charidx = dl->charidx;

          const float *old_verts = dl->verts + 3 * (dl->nr - 1);
          float *new_verts = dlnew->verts;
          for (int i = 0; i < dl->parts; i++) {
            copy_v3_v3(new_verts, old_verts);
            new_verts += 3;
            old_verts += 3 * dl->nr;
          }
        }
      }
    }
  }

  const float z_up[3] = {0.0f, 0.0f, -1.0f};
  BKE_displist_fill(&front, dispbase, z_up, true);
  BKE_displist_fill(&back, dispbase, z_up, false);

  BKE_displist_free(&front);
  BKE_displist_free(&back);

  BKE_displist_fill(dispbase, dispbase, z_up, false);
}

static void curve_to_filledpoly(Curve *cu, ListBase *UNUSED(nurb), ListBase *dispbase)
{
  if (!CU_DO_2DFILL(cu)) {
    return;
  }

  if (dispbase->first && ((DispList *)dispbase->first)->type == DL_SURF) {
    bevels_to_filledpoly(cu, dispbase);
  }
  else {
    const float z_up[3] = {0.0f, 0.0f, -1.0f};
    BKE_displist_fill(dispbase, dispbase, z_up, false);
  }
}

/* taper rules:
 * - only 1 curve
 * - first point left, last point right
 * - based on subdivided points in original curve, not on points in taper curve (still)
 */
static float displist_calc_taper(Depsgraph *depsgraph, Scene *scene, Object *taperobj, float fac)
{
  DispList *dl;

  if (taperobj == NULL || taperobj->type != OB_CURVE) {
    return 1.0;
  }

  dl = taperobj->runtime.curve_cache ? taperobj->runtime.curve_cache->disp.first : NULL;
  if (dl == NULL) {
    BKE_displist_make_curveTypes(depsgraph, scene, taperobj, false, false);
    dl = taperobj->runtime.curve_cache->disp.first;
  }
  if (dl) {
    float minx, dx, *fp;
    int a;

    /* horizontal size */
    minx = dl->verts[0];
    dx = dl->verts[3 * (dl->nr - 1)] - minx;
    if (dx > 0.0f) {
      fp = dl->verts;
      for (a = 0; a < dl->nr; a++, fp += 3) {
        if ((fp[0] - minx) / dx >= fac) {
          /* interpolate with prev */
          if (a > 0) {
            float fac1 = (fp[-3] - minx) / dx;
            float fac2 = (fp[0] - minx) / dx;
            if (fac1 != fac2) {
              return fp[1] * (fac1 - fac) / (fac1 - fac2) + fp[-2] * (fac - fac2) / (fac1 - fac2);
            }
          }
          return fp[1];
        }
      }
      return fp[-2];  // last y coord
    }
  }

  return 1.0;
}

float BKE_displist_calc_taper(
    Depsgraph *depsgraph, Scene *scene, Object *taperobj, int cur, int tot)
{
  float fac = ((float)cur) / (float)(tot - 1);

  return displist_calc_taper(depsgraph, scene, taperobj, fac);
}

void BKE_displist_make_mball(Depsgraph *depsgraph, Scene *scene, Object *ob)
{
  if (!ob || ob->type != OB_MBALL) {
    return;
  }

  if (ob == BKE_mball_basis_find(scene, ob)) {
    if (ob->runtime.curve_cache) {
      BKE_displist_free(&(ob->runtime.curve_cache->disp));
    }
    else {
      ob->runtime.curve_cache = MEM_callocN(sizeof(CurveCache), "CurveCache for MBall");
    }

    BKE_mball_polygonize(depsgraph, scene, ob, &ob->runtime.curve_cache->disp);
    BKE_mball_texspace_calc(ob);

    object_deform_mball(ob, &ob->runtime.curve_cache->disp);

    /* NOP for MBALLs anyway... */
    boundbox_displist_object(ob);
  }
}

void BKE_displist_make_mball_forRender(Depsgraph *depsgraph,
                                       Scene *scene,
                                       Object *ob,
                                       ListBase *dispbase)
{
  BKE_mball_polygonize(depsgraph, scene, ob, dispbase);
  BKE_mball_texspace_calc(ob);

  object_deform_mball(ob, dispbase);
}

static ModifierData *curve_get_tessellate_point(Scene *scene,
                                                Object *ob,
                                                const bool for_render,
                                                const bool editmode)
{
  VirtualModifierData virtualModifierData;
  ModifierData *md = BKE_modifiers_get_virtual_modifierlist(ob, &virtualModifierData);
  ModifierData *pretessellatePoint;
  int required_mode;

  if (for_render) {
    required_mode = eModifierMode_Render;
  }
  else {
    required_mode = eModifierMode_Realtime;
  }

  if (editmode) {
    required_mode |= eModifierMode_Editmode;
  }

  pretessellatePoint = NULL;
  for (; md; md = md->next) {
    const ModifierTypeInfo *mti = BKE_modifier_get_info(md->type);

    if (!BKE_modifier_is_enabled(scene, md, required_mode)) {
      continue;
    }
    if (mti->type == eModifierTypeType_Constructive) {
      return pretessellatePoint;
    }

    if (ELEM(md->type, eModifierType_Hook, eModifierType_Softbody, eModifierType_MeshDeform)) {
      pretessellatePoint = md;

      /* this modifiers are moving point of tessellation automatically
       * (some of them even can't be applied on tessellated curve), set flag
       * for information button in modifier's header
       */
      md->mode |= eModifierMode_ApplyOnSpline;
    }
    else if (md->mode & eModifierMode_ApplyOnSpline) {
      pretessellatePoint = md;
    }
  }

  return pretessellatePoint;
}

/* Return true if any modifier was applied. */
bool BKE_curve_calc_modifiers_pre(Depsgraph *depsgraph,
                                  Scene *scene,
                                  Object *ob,
                                  ListBase *source_nurb,
                                  ListBase *target_nurb,
                                  const bool for_render)
{
  VirtualModifierData virtualModifierData;
  ModifierData *md = BKE_modifiers_get_virtual_modifierlist(ob, &virtualModifierData);
  ModifierData *pretessellatePoint;
  Curve *cu = ob->data;
  int numElems = 0, numVerts = 0;
  const bool editmode = (!for_render && (cu->editnurb || cu->editfont));
  ModifierApplyFlag apply_flag = 0;
  float(*deformedVerts)[3] = NULL;
  float *keyVerts = NULL;
  int required_mode;
  bool modified = false;

  BKE_modifiers_clear_errors(ob);

  if (editmode) {
    apply_flag |= MOD_APPLY_USECACHE;
  }
  if (for_render) {
    apply_flag |= MOD_APPLY_RENDER;
    required_mode = eModifierMode_Render;
  }
  else {
    required_mode = eModifierMode_Realtime;
  }

  const ModifierEvalContext mectx = {depsgraph, ob, apply_flag};

  pretessellatePoint = curve_get_tessellate_point(scene, ob, for_render, editmode);

  if (editmode) {
    required_mode |= eModifierMode_Editmode;
  }

  if (!editmode) {
    keyVerts = BKE_key_evaluate_object(ob, &numElems);

    if (keyVerts) {
      BLI_assert(BKE_keyblock_curve_element_count(source_nurb) == numElems);

      /* split coords from key data, the latter also includes
       * tilts, which is passed through in the modifier stack.
       * this is also the reason curves do not use a virtual
       * shape key modifier yet. */
      deformedVerts = BKE_curve_nurbs_key_vert_coords_alloc(source_nurb, keyVerts, &numVerts);
    }
  }

  if (pretessellatePoint) {
    for (; md; md = md->next) {
      const ModifierTypeInfo *mti = BKE_modifier_get_info(md->type);

      if (!BKE_modifier_is_enabled(scene, md, required_mode)) {
        continue;
      }
      if (mti->type != eModifierTypeType_OnlyDeform) {
        continue;
      }

      if (!deformedVerts) {
        deformedVerts = BKE_curve_nurbs_vert_coords_alloc(source_nurb, &numVerts);
      }

      mti->deformVerts(md, &mectx, NULL, deformedVerts, numVerts);
      modified = true;

      if (md == pretessellatePoint) {
        break;
      }
    }
  }

  if (deformedVerts) {
    BKE_curve_nurbs_vert_coords_apply(target_nurb, deformedVerts, false);
    MEM_freeN(deformedVerts);
  }
  if (keyVerts) { /* these are not passed through modifier stack */
    BKE_curve_nurbs_key_vert_tilts_apply(target_nurb, keyVerts);
  }

  if (keyVerts) {
    MEM_freeN(keyVerts);
  }
  return modified;
}

static float (*displist_vert_coords_alloc(ListBase *dispbase, int *r_vert_len))[3]
{
  float(*allverts)[3], *fp;

  *r_vert_len = 0;

  LISTBASE_FOREACH (DispList *, dl, dispbase) {
    *r_vert_len += (dl->type == DL_INDEX3) ? dl->nr : dl->parts * dl->nr;
  }

  allverts = MEM_mallocN(sizeof(float[3]) * (*r_vert_len), "displist_vert_coords_alloc allverts");
  fp = (float *)allverts;
  LISTBASE_FOREACH (DispList *, dl, dispbase) {
    int ofs = 3 * ((dl->type == DL_INDEX3) ? dl->nr : dl->parts * dl->nr);
    memcpy(fp, dl->verts, sizeof(float) * ofs);
    fp += ofs;
  }

  return allverts;
}

static void displist_vert_coords_apply(ListBase *dispbase, float (*allverts)[3])
{
  const float *fp;

  fp = (float *)allverts;
  LISTBASE_FOREACH (DispList *, dl, dispbase) {
    int ofs = 3 * ((dl->type == DL_INDEX3) ? dl->nr : dl->parts * dl->nr);
    memcpy(dl->verts, fp, sizeof(float) * ofs);
    fp += ofs;
  }
}

static void curve_calc_modifiers_post(Depsgraph *depsgraph,
                                      Scene *scene,
                                      Object *ob,
                                      ListBase *nurb,
                                      ListBase *dispbase,
                                      Mesh **r_final,
                                      const bool for_render,
                                      const bool force_mesh_conversion)
{
  VirtualModifierData virtualModifierData;
  ModifierData *md = BKE_modifiers_get_virtual_modifierlist(ob, &virtualModifierData);
  ModifierData *pretessellatePoint;
  Curve *cu = ob->data;
  int required_mode = 0, totvert = 0;
  const bool editmode = (!for_render && (cu->editnurb || cu->editfont));
  Mesh *modified = NULL, *mesh_applied;
  float(*vertCos)[3] = NULL;
  int useCache = !for_render;
  ModifierApplyFlag apply_flag = 0;

  if (for_render) {
    apply_flag |= MOD_APPLY_RENDER;
    required_mode = eModifierMode_Render;
  }
  else {
    required_mode = eModifierMode_Realtime;
  }

  const ModifierEvalContext mectx_deform = {
      depsgraph, ob, editmode ? apply_flag | MOD_APPLY_USECACHE : apply_flag};
  const ModifierEvalContext mectx_apply = {
      depsgraph, ob, useCache ? apply_flag | MOD_APPLY_USECACHE : apply_flag};

  pretessellatePoint = curve_get_tessellate_point(scene, ob, for_render, editmode);

  if (editmode) {
    required_mode |= eModifierMode_Editmode;
  }

  if (pretessellatePoint) {
    md = pretessellatePoint->next;
  }

  if (r_final && *r_final) {
    BKE_id_free(NULL, *r_final);
  }

  for (; md; md = md->next) {
    const ModifierTypeInfo *mti = BKE_modifier_get_info(md->type);

    if (!BKE_modifier_is_enabled(scene, md, required_mode)) {
      continue;
    }

    /* If we need normals, no choice, have to convert to mesh now. */
    bool need_normal = mti->dependsOnNormals != NULL && mti->dependsOnNormals(md);
    /* XXX 2.8 : now that batch cache is stored inside the ob->data
     * we need to create a Mesh for each curve that uses modifiers. */
    if (modified == NULL /* && need_normal */) {
      if (vertCos != NULL) {
        displist_vert_coords_apply(dispbase, vertCos);
      }

      if (ELEM(ob->type, OB_CURVE, OB_FONT) && (cu->flag & CU_DEFORM_FILL)) {
        curve_to_filledpoly(cu, nurb, dispbase);
      }

      modified = BKE_mesh_new_nomain_from_curve_displist(ob, dispbase);
    }

    if (mti->type == eModifierTypeType_OnlyDeform ||
        (mti->type == eModifierTypeType_DeformOrConstruct && !modified)) {
      if (modified) {
        if (!vertCos) {
          vertCos = BKE_mesh_vert_coords_alloc(modified, &totvert);
        }
        if (need_normal) {
          BKE_mesh_ensure_normals(modified);
        }
        mti->deformVerts(md, &mectx_deform, modified, vertCos, totvert);
      }
      else {
        if (!vertCos) {
          vertCos = displist_vert_coords_alloc(dispbase, &totvert);
        }
        mti->deformVerts(md, &mectx_deform, NULL, vertCos, totvert);
      }
    }
    else {
      if (!r_final) {
        /* makeDisplistCurveTypes could be used for beveling, where derived mesh
         * is totally unnecessary, so we could stop modifiers applying
         * when we found constructive modifier but derived mesh is unwanted result
         */
        break;
      }

      if (modified) {
        if (vertCos) {
          Mesh *temp_mesh = (Mesh *)BKE_id_copy_ex(
              NULL, &modified->id, NULL, LIB_ID_COPY_LOCALIZE);
          BKE_id_free(NULL, modified);
          modified = temp_mesh;

          BKE_mesh_vert_coords_apply(modified, vertCos);
        }
      }
      else {
        if (vertCos) {
          displist_vert_coords_apply(dispbase, vertCos);
        }

        if (ELEM(ob->type, OB_CURVE, OB_FONT) && (cu->flag & CU_DEFORM_FILL)) {
          curve_to_filledpoly(cu, nurb, dispbase);
        }

        modified = BKE_mesh_new_nomain_from_curve_displist(ob, dispbase);
      }

      if (vertCos) {
        /* Vertex coordinates were applied to necessary data, could free it */
        MEM_freeN(vertCos);
        vertCos = NULL;
      }

      if (need_normal) {
        BKE_mesh_ensure_normals(modified);
      }
      mesh_applied = mti->modifyMesh(md, &mectx_apply, modified);

      if (mesh_applied) {
        /* Modifier returned a new derived mesh */

        if (modified && modified != mesh_applied) { /* Modifier  */
          BKE_id_free(NULL, modified);
        }
        modified = mesh_applied;
      }
    }
  }

  if (vertCos) {
    if (modified) {
      Mesh *temp_mesh = (Mesh *)BKE_id_copy_ex(NULL, &modified->id, NULL, LIB_ID_COPY_LOCALIZE);
      BKE_id_free(NULL, modified);
      modified = temp_mesh;

      BKE_mesh_vert_coords_apply(modified, vertCos);
      BKE_mesh_calc_normals_mapping_simple(modified);

      MEM_freeN(vertCos);
    }
    else {
      displist_vert_coords_apply(dispbase, vertCos);
      MEM_freeN(vertCos);
      vertCos = NULL;
    }
  }

  if (r_final) {
    if (force_mesh_conversion && !modified) {
      /* XXX 2.8 : This is a workaround for by some deeper technical debts:
       * - DRW Batch cache is stored inside the ob->data.
       * - Curve data is not COWed for instances that use different modifiers.
       * This can causes the modifiers to be applied on all user of the same data-block
       * (see T71055)
       *
       * The easy workaround is to force to generate a Mesh that will be used for display data
       * since a Mesh output is already used for generative modifiers.
       * However it does not fix problems with actual edit data still being shared.
       *
       * The right solution would be to COW the Curve data block at the input of the modifier
       * stack just like what the mesh modifier does.
       */
      modified = BKE_mesh_new_nomain_from_curve_displist(ob, dispbase);
    }

    if (modified) {

      /* XXX2.8(Sybren): make sure the face normals are recalculated as well */
      BKE_mesh_ensure_normals(modified);

      /* Special tweaks, needed since neither BKE_mesh_new_nomain_from_template() nor
       * BKE_mesh_new_nomain_from_curve_displist() properly duplicate mat info...
       */
      BLI_strncpy(modified->id.name, cu->id.name, sizeof(modified->id.name));
      *((short *)modified->id.name) = ID_ME;
      MEM_SAFE_FREE(modified->mat);
      /* Set flag which makes it easier to see what's going on in a debugger. */
      modified->id.tag |= LIB_TAG_COPIED_ON_WRITE_EVAL_RESULT;
      modified->mat = MEM_dupallocN(cu->mat);
      modified->totcol = cu->totcol;

      (*r_final) = modified;
    }
    else {
      (*r_final) = NULL;
    }
  }
  else if (modified != NULL) {
    /* Pretty stupid to generate that whole mesh if it's unused, yet we have to free it. */
    BKE_id_free(NULL, modified);
  }
}

static void displist_surf_indices(DispList *dl)
{
  int a, b, p1, p2, p3, p4;
  int *index;

  dl->totindex = 0;

  index = dl->index = MEM_mallocN(sizeof(int[4]) * (dl->parts + 1) * (dl->nr + 1),
                                  "index array nurbs");

  for (a = 0; a < dl->parts; a++) {

    if (BKE_displist_surfindex_get(dl, a, &b, &p1, &p2, &p3, &p4) == 0) {
      break;
    }

    for (; b < dl->nr; b++, index += 4) {
      index[0] = p1;
      index[1] = p2;
      index[2] = p4;
      index[3] = p3;

      dl->totindex++;

      p2 = p1;
      p1++;
      p4 = p3;
      p3++;
    }
  }
}

void BKE_displist_make_surf(Depsgraph *depsgraph,
                            Scene *scene,
                            Object *ob,
                            ListBase *dispbase,
                            Mesh **r_final,
                            const bool for_render,
                            const bool for_orco)
{
  ListBase nubase = {NULL, NULL};
  Curve *cu = ob->data;
  DispList *dl;
  float *data;
  int len;
  bool force_mesh_conversion = false;

  if (!for_render && cu->editnurb) {
    BKE_nurbList_duplicate(&nubase, BKE_curve_editNurbs_get(cu));
  }
  else {
    BKE_nurbList_duplicate(&nubase, &cu->nurb);
  }

  if (!for_orco) {
    force_mesh_conversion = BKE_curve_calc_modifiers_pre(
        depsgraph, scene, ob, &nubase, &nubase, for_render);
  }

  LISTBASE_FOREACH (Nurb *, nu, &nubase) {
    if (!(for_render || nu->hide == 0) || !BKE_nurb_check_valid_uv(nu)) {
      continue;
    }

    int resolu = nu->resolu, resolv = nu->resolv;

    if (for_render) {
      if (cu->resolu_ren) {
        resolu = cu->resolu_ren;
      }
      if (cu->resolv_ren) {
        resolv = cu->resolv_ren;
      }
    }

    if (nu->pntsv == 1) {
      len = SEGMENTSU(nu) * resolu;

      dl = MEM_callocN(sizeof(DispList), "makeDispListsurf");
      dl->verts = MEM_mallocN(len * sizeof(float[3]), "dlverts");

      BLI_addtail(dispbase, dl);
      dl->parts = 1;
      dl->nr = len;
      dl->col = nu->mat_nr;
      dl->charidx = nu->charidx;

      /* dl->rt will be used as flag for render face and */
      /* CU_2D conflicts with R_NOPUNOFLIP */
      dl->rt = nu->flag & ~CU_2D;

      data = dl->verts;
      if (nu->flagu & CU_NURB_CYCLIC) {
        dl->type = DL_POLY;
      }
      else {
        dl->type = DL_SEGM;
      }

      BKE_nurb_makeCurve(nu, data, NULL, NULL, NULL, resolu, sizeof(float[3]));
    }
    else {
      len = (nu->pntsu * resolu) * (nu->pntsv * resolv);

      dl = MEM_callocN(sizeof(DispList), "makeDispListsurf");
      dl->verts = MEM_mallocN(len * sizeof(float[3]), "dlverts");
      BLI_addtail(dispbase, dl);

      dl->col = nu->mat_nr;
      dl->charidx = nu->charidx;

      /* dl->rt will be used as flag for render face and */
      /* CU_2D conflicts with R_NOPUNOFLIP */
      dl->rt = nu->flag & ~CU_2D;

      data = dl->verts;
      dl->type = DL_SURF;

      dl->parts = (nu->pntsu * resolu); /* in reverse, because makeNurbfaces works that way */
      dl->nr = (nu->pntsv * resolv);
      if (nu->flagv & CU_NURB_CYCLIC) {
        dl->flag |= DL_CYCL_U; /* reverse too! */
      }
      if (nu->flagu & CU_NURB_CYCLIC) {
        dl->flag |= DL_CYCL_V;
      }

      BKE_nurb_makeFaces(nu, data, 0, resolu, resolv);

      /* gl array drawing: using indices */
      displist_surf_indices(dl);
    }
  }

  if (!for_orco) {
    BKE_nurbList_duplicate(&ob->runtime.curve_cache->deformed_nurbs, &nubase);
    curve_calc_modifiers_post(
        depsgraph, scene, ob, &nubase, dispbase, r_final, for_render, force_mesh_conversion);
  }

  BKE_nurbList_free(&nubase);
}

static void rotateBevelPiece(const Curve *cu,
                             const BevPoint *bevp,
                             const BevPoint *nbevp,
                             const DispList *dlb,
                             const float bev_blend,
                             const float widfac,
                             const float radius_factor,
                             float **r_data)
{
  float *data = *r_data;
  const float *fp = dlb->verts;
  for (int b = 0; b < dlb->nr; b++, fp += 3, data += 3) {
    if (cu->flag & CU_3D) {
      float vec[3], quat[4];

      vec[0] = fp[1] + widfac;
      vec[1] = fp[2];
      vec[2] = 0.0;

      if (nbevp == NULL) {
        copy_v3_v3(data, bevp->vec);
        copy_qt_qt(quat, bevp->quat);
      }
      else {
        interp_v3_v3v3(data, bevp->vec, nbevp->vec, bev_blend);
        interp_qt_qtqt(quat, bevp->quat, nbevp->quat, bev_blend);
      }

      mul_qt_v3(quat, vec);

      data[0] += radius_factor * vec[0];
      data[1] += radius_factor * vec[1];
      data[2] += radius_factor * vec[2];
    }
    else {
      float sina, cosa;

      if (nbevp == NULL) {
        copy_v3_v3(data, bevp->vec);
        sina = bevp->sina;
        cosa = bevp->cosa;
      }
      else {
        interp_v3_v3v3(data, bevp->vec, nbevp->vec, bev_blend);

        /* perhaps we need to interpolate angles instead. but the thing is
         * cosa and sina are not actually sine and cosine
         */
        sina = nbevp->sina * bev_blend + bevp->sina * (1.0f - bev_blend);
        cosa = nbevp->cosa * bev_blend + bevp->cosa * (1.0f - bev_blend);
      }

      data[0] += radius_factor * (widfac + fp[1]) * sina;
      data[1] += radius_factor * (widfac + fp[1]) * cosa;
      data[2] += radius_factor * fp[2];
    }
  }

  *r_data = data;
}

static void fillBevelCap(Nurb *nu, DispList *dlb, float *prev_fp, ListBase *dispbase)
{
  DispList *dl;

  dl = MEM_callocN(sizeof(DispList), "makeDispListbev2");
  dl->verts = MEM_mallocN(sizeof(float[3]) * dlb->nr, "dlverts");
  memcpy(dl->verts, prev_fp, sizeof(float[3]) * dlb->nr);

  dl->type = DL_POLY;

  dl->parts = 1;
  dl->nr = dlb->nr;
  dl->col = nu->mat_nr;
  dl->charidx = nu->charidx;

  /* dl->rt will be used as flag for render face and */
  /* CU_2D conflicts with R_NOPUNOFLIP */
  dl->rt = nu->flag & ~CU_2D;

  BLI_addtail(dispbase, dl);
}

static void calc_bevfac_segment_mapping(
    BevList *bl, float bevfac, float spline_length, int *r_bev, float *r_blend)
{
  float normlen, normsum = 0.0f;
  float *seglen = bl->seglen;
  int *segbevcount = bl->segbevcount;
  int bevcount = 0, nr = bl->nr;

  float bev_fl = bevfac * (bl->nr - 1);
  *r_bev = (int)bev_fl;

  while (bevcount < nr - 1) {
    normlen = *seglen / spline_length;
    if (normsum + normlen > bevfac) {
      bev_fl = bevcount + (bevfac - normsum) / normlen * *segbevcount;
      *r_bev = (int)bev_fl;
      *r_blend = bev_fl - *r_bev;
      break;
    }
    normsum += normlen;
    bevcount += *segbevcount;
    segbevcount++;
    seglen++;
  }
}

static void calc_bevfac_spline_mapping(
    BevList *bl, float bevfac, float spline_length, int *r_bev, float *r_blend)
{
  const float len_target = bevfac * spline_length;
  BevPoint *bevp = bl->bevpoints;
  float len_next = 0.0f, len = 0.0f;
  int i = 0, nr = bl->nr;

  while (nr--) {
    bevp++;
    len_next = len + bevp->offset;
    if (len_next > len_target) {
      break;
    }
    len = len_next;
    i++;
  }

  *r_bev = i;
  *r_blend = (len_target - len) / bevp->offset;
}

static void calc_bevfac_mapping_default(
    BevList *bl, int *r_start, float *r_firstblend, int *r_steps, float *r_lastblend)
{
  *r_start = 0;
  *r_steps = bl->nr;
  *r_firstblend = 1.0f;
  *r_lastblend = 1.0f;
}

static void calc_bevfac_mapping(Curve *cu,
                                BevList *bl,
                                Nurb *nu,
                                int *r_start,
                                float *r_firstblend,
                                int *r_steps,
                                float *r_lastblend)
{
  float tmpf, total_length = 0.0f;
  int end = 0, i;

  if ((BKE_nurb_check_valid_u(nu) == false) ||
      /* not essential, but skips unnecessary calculation */
      (min_ff(cu->bevfac1, cu->bevfac2) == 0.0f && max_ff(cu->bevfac1, cu->bevfac2) == 1.0f)) {
    calc_bevfac_mapping_default(bl, r_start, r_firstblend, r_steps, r_lastblend);
    return;
  }

  if (ELEM(cu->bevfac1_mapping, CU_BEVFAC_MAP_SEGMENT, CU_BEVFAC_MAP_SPLINE) ||
      ELEM(cu->bevfac2_mapping, CU_BEVFAC_MAP_SEGMENT, CU_BEVFAC_MAP_SPLINE)) {
    for (i = 0; i < SEGMENTSU(nu); i++) {
      total_length += bl->seglen[i];
    }
  }

  switch (cu->bevfac1_mapping) {
    case CU_BEVFAC_MAP_RESOLU: {
      const float start_fl = cu->bevfac1 * (bl->nr - 1);
      *r_start = (int)start_fl;
      *r_firstblend = 1.0f - (start_fl - (*r_start));
      break;
    }
    case CU_BEVFAC_MAP_SEGMENT: {
      calc_bevfac_segment_mapping(bl, cu->bevfac1, total_length, r_start, r_firstblend);
      *r_firstblend = 1.0f - *r_firstblend;
      break;
    }
    case CU_BEVFAC_MAP_SPLINE: {
      calc_bevfac_spline_mapping(bl, cu->bevfac1, total_length, r_start, r_firstblend);
      *r_firstblend = 1.0f - *r_firstblend;
      break;
    }
  }

  switch (cu->bevfac2_mapping) {
    case CU_BEVFAC_MAP_RESOLU: {
      const float end_fl = cu->bevfac2 * (bl->nr - 1);
      end = (int)end_fl;

      *r_steps = 2 + end - *r_start;
      *r_lastblend = end_fl - end;
      break;
    }
    case CU_BEVFAC_MAP_SEGMENT: {
      calc_bevfac_segment_mapping(bl, cu->bevfac2, total_length, &end, r_lastblend);
      *r_steps = end - *r_start + 2;
      break;
    }
    case CU_BEVFAC_MAP_SPLINE: {
      calc_bevfac_spline_mapping(bl, cu->bevfac2, total_length, &end, r_lastblend);
      *r_steps = end - *r_start + 2;
      break;
    }
  }

  if (end < *r_start || (end == *r_start && *r_lastblend < 1.0f - *r_firstblend)) {
    SWAP(int, *r_start, end);
    tmpf = *r_lastblend;
    *r_lastblend = 1.0f - *r_firstblend;
    *r_firstblend = 1.0f - tmpf;
    *r_steps = end - *r_start + 2;
  }

  if (*r_start + *r_steps > bl->nr) {
    *r_steps = bl->nr - *r_start;
    *r_lastblend = 1.0f;
  }
}

static void do_makeDispListCurveTypes(Depsgraph *depsgraph,
                                      Scene *scene,
                                      Object *ob,
                                      ListBase *dispbase,
                                      const bool for_render,
                                      const bool for_orco,
                                      Mesh **r_final)
{
  Curve *cu = ob->data;

  /* we do allow duplis... this is only displist on curve level */
  if (!ELEM(ob->type, OB_SURF, OB_CURVE, OB_FONT)) {
    return;
  }

  if (ob->type == OB_SURF) {
    BKE_displist_make_surf(depsgraph, scene, ob, dispbase, r_final, for_render, for_orco);
  }
  else if (ELEM(ob->type, OB_CURVE, OB_FONT)) {
    ListBase dlbev;
    ListBase nubase = {NULL, NULL};
    bool force_mesh_conversion = false;

    BKE_curve_bevelList_free(&ob->runtime.curve_cache->bev);

    /* We only re-evaluate path if evaluation is not happening for orco.
     * If the calculation happens for orco, we should never free data which
     * was needed before and only not needed for orco calculation.
     */
    if (!for_orco) {
      if (ob->runtime.curve_cache->path) {
        free_path(ob->runtime.curve_cache->path);
      }
      ob->runtime.curve_cache->path = NULL;
    }

    if (ob->type == OB_FONT) {
      BKE_vfont_to_curve_nubase(ob, FO_EDIT, &nubase);
    }
    else {
      BKE_nurbList_duplicate(&nubase, BKE_curve_nurbs_get(cu));
    }

    if (!for_orco) {
      force_mesh_conversion = BKE_curve_calc_modifiers_pre(
          depsgraph, scene, ob, &nubase, &nubase, for_render);
    }

    BKE_curve_bevelList_make(ob, &nubase, for_render);

    /* If curve has no bevel will return nothing */
    BKE_curve_bevel_make(ob, &dlbev);

    /* no bevel or extrude, and no width correction? */
    if (!dlbev.first && cu->width == 1.0f) {
      curve_to_displist(cu, &nubase, for_render, dispbase);
    }
    else {
      const float widfac = cu->width - 1.0f;
      BevList *bl = ob->runtime.curve_cache->bev.first;
      Nurb *nu = nubase.first;

      for (; bl && nu; bl = bl->next, nu = nu->next) {
        float *data;

        if (bl->nr == 0) { /* blank bevel lists can happen */
          continue;
        }

        /* exception handling; curve without bevel or extrude, with width correction */
        if (BLI_listbase_is_empty(&dlbev)) {
          DispList *dl = MEM_callocN(sizeof(DispList), "makeDispListbev");
          dl->verts = MEM_mallocN(sizeof(float[3]) * bl->nr, "dlverts");
          BLI_addtail(dispbase, dl);

          if (bl->poly != -1) {
            dl->type = DL_POLY;
          }
          else {
            dl->type = DL_SEGM;
            dl->flag = (DL_FRONT_CURVE | DL_BACK_CURVE);
          }

          dl->parts = 1;
          dl->nr = bl->nr;
          dl->col = nu->mat_nr;
          dl->charidx = nu->charidx;

          /* dl->rt will be used as flag for render face and */
          /* CU_2D conflicts with R_NOPUNOFLIP */
          dl->rt = nu->flag & ~CU_2D;

          int a = dl->nr;
          BevPoint *bevp = bl->bevpoints;
          data = dl->verts;
          while (a--) {
            data[0] = bevp->vec[0] + widfac * bevp->sina;
            data[1] = bevp->vec[1] + widfac * bevp->cosa;
            data[2] = bevp->vec[2];
            bevp++;
            data += 3;
          }
        }
        else {
          ListBase bottom_capbase = {NULL, NULL};
          ListBase top_capbase = {NULL, NULL};
          float bottom_no[3] = {0.0f};
          float top_no[3] = {0.0f};
          float first_blend = 0.0f, last_blend = 0.0f;
          int start, steps = 0;

          if (nu->flagu & CU_NURB_CYCLIC) {
            calc_bevfac_mapping_default(bl, &start, &first_blend, &steps, &last_blend);
          }
          else {
            if (fabsf(cu->bevfac2 - cu->bevfac1) < FLT_EPSILON) {
              continue;
            }

            calc_bevfac_mapping(cu, bl, nu, &start, &first_blend, &steps, &last_blend);
          }

          LISTBASE_FOREACH (DispList *, dlb, &dlbev) {
            /* for each part of the bevel use a separate displblock */
            DispList *dl = MEM_callocN(sizeof(DispList), "makeDispListbev1");
            dl->verts = data = MEM_mallocN(sizeof(float[3]) * dlb->nr * steps, "dlverts");
            BLI_addtail(dispbase, dl);

            dl->type = DL_SURF;

            dl->flag = dlb->flag & (DL_FRONT_CURVE | DL_BACK_CURVE);
            if (dlb->type == DL_POLY) {
              dl->flag |= DL_CYCL_U;
            }
            if ((bl->poly >= 0) && (steps > 2)) {
              dl->flag |= DL_CYCL_V;
            }

            dl->parts = steps;
            dl->nr = dlb->nr;
            dl->col = nu->mat_nr;
            dl->charidx = nu->charidx;

            /* dl->rt will be used as flag for render face and */
            /* CU_2D conflicts with R_NOPUNOFLIP */
            dl->rt = nu->flag & ~CU_2D;

            dl->bevel_split = BLI_BITMAP_NEW(steps, "bevel_split");

            /* for each point of poly make a bevel piece */
            BevPoint *bevp_first = bl->bevpoints;
            BevPoint *bevp_last = &bl->bevpoints[bl->nr - 1];
            BevPoint *bevp = &bl->bevpoints[start];
            for (int i = start, a = 0; a < steps; i++, bevp++, a++) {
              float radius_factor = 1.0;
              float *cur_data = data;

              if (cu->taperobj == NULL) {
                radius_factor = bevp->radius;
              }
              else {
                float taper_factor;
                if (cu->flag & CU_MAP_TAPER) {
                  float len = (steps - 3) + first_blend + last_blend;

                  if (a == 0) {
                    taper_factor = 0.0f;
                  }
                  else if (a == steps - 1) {
                    taper_factor = 1.0f;
                  }
                  else {
                    taper_factor = ((float)a - (1.0f - first_blend)) / len;
                  }
                }
                else {
                  float len = bl->nr - 1;
                  taper_factor = (float)i / len;

                  if (a == 0) {
                    taper_factor += (1.0f - first_blend) / len;
                  }
                  else if (a == steps - 1) {
                    taper_factor -= (1.0f - last_blend) / len;
                  }
                }

                radius_factor = displist_calc_taper(depsgraph, scene, cu->taperobj, taper_factor);
              }

              if (bevp->split_tag) {
                BLI_BITMAP_ENABLE(dl->bevel_split, a);
              }

              /* rotate bevel piece and write in data */
              if ((a == 0) && (bevp != bevp_last)) {
                rotateBevelPiece(
                    cu, bevp, bevp + 1, dlb, 1.0f - first_blend, widfac, radius_factor, &data);
              }
              else if ((a == steps - 1) && (bevp != bevp_first)) {
                rotateBevelPiece(
                    cu, bevp, bevp - 1, dlb, 1.0f - last_blend, widfac, radius_factor, &data);
              }
              else {
                rotateBevelPiece(cu, bevp, NULL, dlb, 0.0f, widfac, radius_factor, &data);
              }

              if ((cu->flag & CU_FILL_CAPS) && !(nu->flagu & CU_NURB_CYCLIC)) {
                if (a == 1) {
                  fillBevelCap(nu, dlb, cur_data - 3 * dlb->nr, &bottom_capbase);
                  copy_v3_v3(bottom_no, bevp->dir);
                }
                if (a == steps - 1) {
                  fillBevelCap(nu, dlb, cur_data, &top_capbase);
                  negate_v3_v3(top_no, bevp->dir);
                }
              }
            }

            /* gl array drawing: using indices */
            displist_surf_indices(dl);
          }

          if (bottom_capbase.first) {
            BKE_displist_fill(&bottom_capbase, dispbase, bottom_no, false);
            BKE_displist_fill(&top_capbase, dispbase, top_no, false);
            BKE_displist_free(&bottom_capbase);
            BKE_displist_free(&top_capbase);
          }
        }
      }
      BKE_displist_free(&dlbev);
    }

    if (!(cu->flag & CU_DEFORM_FILL)) {
      curve_to_filledpoly(cu, &nubase, dispbase);
    }

    if (!for_orco) {
      if ((cu->flag & CU_PATH) ||
          DEG_get_eval_flags_for_id(depsgraph, &ob->id) & DAG_EVAL_NEED_CURVE_PATH) {
        calc_curvepath(ob, &nubase);
      }

      BKE_nurbList_duplicate(&ob->runtime.curve_cache->deformed_nurbs, &nubase);
      curve_calc_modifiers_post(
          depsgraph, scene, ob, &nubase, dispbase, r_final, for_render, force_mesh_conversion);
    }

    if (cu->flag & CU_DEFORM_FILL && !ob->runtime.data_eval) {
      curve_to_filledpoly(cu, &nubase, dispbase);
    }

    BKE_nurbList_free(&nubase);
  }
}

void BKE_displist_make_curveTypes(
    Depsgraph *depsgraph, Scene *scene, Object *ob, const bool for_render, const bool for_orco)
{
  ListBase *dispbase;

  /* The same check for duplis as in do_makeDispListCurveTypes.
   * Happens when curve used for constraint/bevel was converted to mesh.
   * check there is still needed for render displist and orco displists. */
  if (!ELEM(ob->type, OB_SURF, OB_CURVE, OB_FONT)) {
    return;
  }

  BKE_object_free_derived_caches(ob);

  if (!ob->runtime.curve_cache) {
    ob->runtime.curve_cache = MEM_callocN(sizeof(CurveCache), "CurveCache for curve types");
  }

  dispbase = &(ob->runtime.curve_cache->disp);

  Mesh *mesh_eval = NULL;
  do_makeDispListCurveTypes(depsgraph, scene, ob, dispbase, for_render, for_orco, &mesh_eval);

  if (mesh_eval != NULL) {
    BKE_object_eval_assign_data(ob, &mesh_eval->id, true);
  }

  boundbox_displist_object(ob);
}

void BKE_displist_make_curveTypes_forRender(Depsgraph *depsgraph,
                                            Scene *scene,
                                            Object *ob,
                                            ListBase *dispbase,
                                            Mesh **r_final,
                                            const bool for_orco)
{
  if (ob->runtime.curve_cache == NULL) {
    ob->runtime.curve_cache = MEM_callocN(sizeof(CurveCache), "CurveCache for Curve");
  }

  do_makeDispListCurveTypes(depsgraph, scene, ob, dispbase, true, for_orco, r_final);
}

void BKE_displist_minmax(ListBase *dispbase, float min[3], float max[3])
{
  const float *vert;
  int a, tot = 0;
  int doit = 0;

  LISTBASE_FOREACH (DispList *, dl, dispbase) {
    tot = (dl->type == DL_INDEX3) ? dl->nr : dl->nr * dl->parts;
    vert = dl->verts;
    for (a = 0; a < tot; a++, vert += 3) {
      minmax_v3v3_v3(min, max, vert);
    }
    doit |= (tot != 0);
  }

  if (!doit) {
    /* there's no geometry in displist, use zero-sized boundbox */
    zero_v3(min);
    zero_v3(max);
  }
}

/* this is confusing, there's also min_max_object, applying the obmat... */
static void boundbox_displist_object(Object *ob)
{
  if (ELEM(ob->type, OB_CURVE, OB_SURF, OB_FONT)) {
    /* Curve's BB is already calculated as a part of modifier stack,
     * here we only calculate object BB based on final display list.
     */

    /* object's BB is calculated from final displist */
    if (ob->runtime.bb == NULL) {
      ob->runtime.bb = MEM_callocN(sizeof(BoundBox), "boundbox");
    }

    Mesh *mesh_eval = BKE_object_get_evaluated_mesh(ob);
    if (mesh_eval) {
      BKE_object_boundbox_calc_from_mesh(ob, mesh_eval);
    }
    else {
      float min[3], max[3];

      INIT_MINMAX(min, max);
      BKE_displist_minmax(&ob->runtime.curve_cache->disp, min, max);
      BKE_boundbox_init_from_minmax(ob->runtime.bb, min, max);

      ob->runtime.bb->flag &= ~BOUNDBOX_DIRTY;
    }
  }
}
