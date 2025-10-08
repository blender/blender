/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <cmath>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "DNA_curve_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"

#include "BLI_index_range.hh"
#include "BLI_listbase.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_memarena.h"
#include "BLI_scanfill.h"
#include "BLI_span.hh"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BKE_anim_path.h"
#include "BKE_curve.hh"
#include "BKE_curve_legacy_convert.hh"
#include "BKE_displist.h"
#include "BKE_geometry_set.hh"
#include "BKE_key.hh"
#include "BKE_lib_id.hh"
#include "BKE_mesh.hh"
#include "BKE_modifier.hh"
#include "BKE_object.hh"
#include "BKE_object_types.hh"
#include "BKE_vfont.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

using blender::Array;
using blender::float3;
using blender::IndexRange;

static void displist_elem_free(DispList *dl)
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
    MEM_freeN(dl);
  }
}

void BKE_displist_free(ListBase *lb)
{
  while (DispList *dl = (DispList *)BLI_pophead(lb)) {
    displist_elem_free(dl);
  }
}

DispList *BKE_displist_find(ListBase *lb, int type)
{
  LISTBASE_FOREACH (DispList *, dl, lb) {
    if (dl->type == type) {
      return dl;
    }
  }

  return nullptr;
}

bool BKE_displist_surfindex_get(
    const DispList *dl, int a, int *b, int *p1, int *p2, int *p3, int *p4)
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

    if (nu->type == CU_BEZIER) {
      const BezTriple *bezt_first = &nu->bezt[0];
      const BezTriple *bezt_last = &nu->bezt[nu->pntsu - 1];
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
       * and resolution > 1. */
      const bool use_cyclic_sample = is_cyclic && (samples_len != 2);

      DispList *dl = MEM_callocN<DispList>(__func__);
      /* Add one to the length because of #BKE_curve_forward_diff_bezier. */
      dl->verts = MEM_malloc_arrayN<float>(3 * size_t(samples_len + 1), __func__);
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
      DispList *dl = MEM_callocN<DispList>(__func__);
      dl->verts = MEM_malloc_arrayN<float>(3 * size_t(len), __func__);
      BLI_addtail(r_dispbase, dl);
      dl->parts = 1;
      dl->nr = len;
      dl->col = nu->mat_nr;
      dl->charidx = nu->charidx;
      dl->type = is_cyclic ? DL_POLY : DL_SEGM;

      BKE_nurb_makeCurve(nu, dl->verts, nullptr, nullptr, nullptr, resolution, sizeof(float[3]));
    }
    else if (nu->type == CU_POLY) {
      const int len = nu->pntsu;
      DispList *dl = MEM_callocN<DispList>(__func__);
      dl->verts = MEM_malloc_arrayN<float>(3 * size_t(len), __func__);
      BLI_addtail(r_dispbase, dl);
      dl->parts = 1;
      dl->nr = len;
      dl->col = nu->mat_nr;
      dl->charidx = nu->charidx;
      dl->type = (is_cyclic && (dl->nr != 2)) ? DL_POLY : DL_SEGM;

      float (*coords)[3] = (float (*)[3])dl->verts;
      for (int i = 0; i < len; i++) {
        const BPoint *bp = &nu->bp[i];
        copy_v3_v3(coords[i], bp->vec);
      }
    }
  }
}

void BKE_displist_fill(const ListBase *dispbase,
                       ListBase *to,
                       const float normal_proj[3],
                       const bool flip_normal)
{
  if (dispbase == nullptr) {
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
            ScanFillVert *sf_vert = nullptr;
            ScanFillVert *sf_vert_last = nullptr;
            ScanFillVert *sf_vert_new = nullptr;
            for (int i = 0; i < dl->nr; i++) {
              sf_vert_last = sf_vert;
              sf_vert = BLI_scanfill_vert_add(&sf_ctx, &dl->verts[3 * i]);
              totvert++;
              if (sf_vert_last == nullptr) {
                sf_vert_new = sf_vert;
              }
              else {
                BLI_scanfill_edge_add(&sf_ctx, sf_vert_last, sf_vert);
              }
            }

            if (sf_vert != nullptr && sf_vert_new != nullptr) {
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
      DispList *dlnew = MEM_callocN<DispList>(__func__);
      dlnew->type = DL_INDEX3;
      dlnew->flag = (dl_flag_accum & (DL_BACK_CURVE | DL_FRONT_CURVE));
      dlnew->rt = (dl_rt_accum & CU_SMOOTH);
      dlnew->col = colnr;
      dlnew->nr = totvert;
      dlnew->parts = triangles_len;

      dlnew->index = MEM_malloc_arrayN<int>(3 * size_t(triangles_len), __func__);
      dlnew->verts = MEM_malloc_arrayN<float>(3 * size_t(totvert), __func__);

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
  ListBase front = {nullptr, nullptr};
  ListBase back = {nullptr, nullptr};

  LISTBASE_FOREACH (const DispList *, dl, dispbase) {
    if (dl->type == DL_SURF) {
      if ((dl->flag & DL_CYCL_V) && (dl->flag & DL_CYCL_U) == 0) {
        if ((cu->flag & CU_BACK) && (dl->flag & DL_BACK_CURVE)) {
          DispList *dlnew = MEM_callocN<DispList>(__func__);
          BLI_addtail(&front, dlnew);
          dlnew->verts = MEM_malloc_arrayN<float>(3 * size_t(dl->parts), __func__);
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
          DispList *dlnew = MEM_callocN<DispList>(__func__);
          BLI_addtail(&back, dlnew);
          dlnew->verts = MEM_malloc_arrayN<float>(3 * size_t(dl->parts), __func__);
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

static void curve_to_filledpoly(const Curve *cu, ListBase *dispbase)
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
static float displist_calc_taper(Depsgraph *depsgraph,
                                 const Scene *scene,
                                 Object *taperobj,
                                 float fac)
{
  if (taperobj == nullptr || taperobj->type != OB_CURVES_LEGACY) {
    return 1.0;
  }

  DispList *dl = taperobj->runtime->curve_cache ?
                     (DispList *)taperobj->runtime->curve_cache->disp.first :
                     nullptr;
  if (dl == nullptr) {
    BKE_displist_make_curveTypes(depsgraph, scene, taperobj, false);
    dl = (DispList *)taperobj->runtime->curve_cache->disp.first;
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
      return fp[-2]; /* Last y coordinate. */
    }
  }

  return 1.0;
}

float BKE_displist_calc_taper(
    Depsgraph *depsgraph, const Scene *scene, Object *taperobj, int cur, int tot)
{
  const float fac = float(cur) / float(tot - 1);

  return displist_calc_taper(depsgraph, scene, taperobj, fac);
}

static ModifierData *curve_get_tessellate_point(const Scene *scene,
                                                const Object *ob,
                                                const bool for_render,
                                                const bool editmode)
{
  VirtualModifierData virtual_modifier_data;
  ModifierData *md = BKE_modifiers_get_virtual_modifierlist(ob, &virtual_modifier_data);

  ModifierMode required_mode = for_render ? eModifierMode_Render : eModifierMode_Realtime;
  if (editmode) {
    required_mode = (ModifierMode)(int(required_mode) | eModifierMode_Editmode);
  }

  ModifierData *pretessellatePoint = nullptr;
  for (; md; md = md->next) {
    const ModifierTypeInfo *mti = BKE_modifier_get_info((ModifierType)md->type);

    if (!BKE_modifier_is_enabled(scene, md, required_mode)) {
      continue;
    }
    if (mti->type == ModifierTypeType::Constructive) {
      return pretessellatePoint;
    }

    if (md->type == eModifierType_Smooth) {
      /* Smooth modifier works with mesh edges explicitly
       * (so needs tessellation, thus cannot work on control points). */
      md->mode &= ~eModifierMode_ApplyOnSpline;
      return pretessellatePoint;
    }
    if (ELEM(md->type, eModifierType_Hook, eModifierType_Softbody, eModifierType_MeshDeform)) {
      pretessellatePoint = md;

      /* this modifiers are moving point of tessellation automatically
       * (some of them even can't be applied on tessellated curve), set flag
       * for information button in modifier's header. */
      md->mode |= eModifierMode_ApplyOnSpline;
    }
    else if (md->mode & eModifierMode_ApplyOnSpline) {
      pretessellatePoint = md;
    }
  }

  return pretessellatePoint;
}

void BKE_curve_calc_modifiers_pre(Depsgraph *depsgraph,
                                  const Scene *scene,
                                  Object *ob,
                                  ListBase *source_nurb,
                                  ListBase *target_nurb,
                                  const bool for_render)
{
  const Curve *cu = (const Curve *)ob->data;

  BKE_modifiers_clear_errors(ob);

  const bool editmode = (!for_render && (cu->editnurb || cu->editfont));
  ModifierMode required_mode = for_render ? eModifierMode_Render : eModifierMode_Realtime;
  if (editmode) {
    required_mode = (ModifierMode)(int(required_mode) | eModifierMode_Editmode);
  }

  ModifierApplyFlag apply_flag = ModifierApplyFlag(0);
  if (editmode) {
    apply_flag = MOD_APPLY_USECACHE;
  }
  if (for_render) {
    apply_flag = MOD_APPLY_RENDER;
  }

  float *keyVerts = nullptr;
  Array<float3> deformedVerts;
  if (!editmode) {
    int numElems = 0;
    keyVerts = BKE_key_evaluate_object(ob, &numElems);

    if (keyVerts) {
      BLI_assert(BKE_keyblock_curve_element_count(source_nurb) == numElems);

      /* split coords from key data, the latter also includes
       * tilts, which is passed through in the modifier stack.
       * this is also the reason curves do not use a virtual
       * shape key modifier yet. */
      deformedVerts = BKE_curve_nurbs_key_vert_coords_alloc(source_nurb, keyVerts);
    }
  }

  const ModifierEvalContext mectx = {depsgraph, ob, apply_flag};
  ModifierData *pretessellatePoint = curve_get_tessellate_point(scene, ob, for_render, editmode);

  if (pretessellatePoint) {
    VirtualModifierData virtual_modifier_data;
    for (ModifierData *md = BKE_modifiers_get_virtual_modifierlist(ob, &virtual_modifier_data); md;
         md = md->next)
    {
      const ModifierTypeInfo *mti = BKE_modifier_get_info((ModifierType)md->type);

      if (!BKE_modifier_is_enabled(scene, md, required_mode)) {
        continue;
      }
      if (mti->type != ModifierTypeType::OnlyDeform) {
        continue;
      }

      blender::bke::ScopedModifierTimer modifier_timer{*md};

      if (deformedVerts.is_empty()) {
        deformedVerts = BKE_curve_nurbs_vert_coords_alloc(source_nurb);
      }

      mti->deform_verts(md, &mectx, nullptr, deformedVerts);

      if (md == pretessellatePoint) {
        break;
      }
    }
  }

  if (!deformedVerts.is_empty()) {
    BKE_curve_nurbs_vert_coords_apply(target_nurb, deformedVerts, false);
  }
  if (keyVerts) { /* these are not passed through modifier stack */
    BKE_curve_nurbs_key_vert_tilts_apply(target_nurb, keyVerts);
  }

  if (keyVerts) {
    MEM_freeN(keyVerts);
  }
}

/**
 * \return True if the deformed curve control point data should be implicitly
 * converted directly to a mesh, or false if it can be left as curve data via the #Curves type.
 */
static bool do_curve_implicit_mesh_conversion(const Curve *curve,
                                              ModifierData *first_modifier,
                                              const Scene *scene,
                                              const ModifierMode required_mode,
                                              const bool editmode)
{
  /* Skip implicit filling and conversion to mesh when using "fast text editing". */
  if ((curve->flag & CU_FAST) && editmode) {
    return false;
  }

  /* Do implicit conversion to mesh with the object bevel mode. */
  if (curve->bevel_mode == CU_BEV_MODE_OBJECT && curve->bevobj != nullptr) {
    return true;
  }

  /* 2D curves are sometimes implicitly filled and converted to a mesh. */
  if (CU_DO_2DFILL(curve)) {
    return true;
  }

  /* Curve objects with implicit "tube" meshes should convert implicitly to a mesh. */
  if (curve->extrude != 0.0f || curve->bevel_radius != 0.0f) {
    return true;
  }

  /* If a non-geometry-nodes modifier is enabled before a nodes modifier,
   * force conversion to mesh, since only the nodes modifier supports curve data. */
  ModifierData *md = first_modifier;
  for (; md; md = md->next) {
    if (BKE_modifier_is_enabled(scene, md, required_mode)) {
      if (md->type == eModifierType_Nodes) {
        break;
      }
      return true;
    }
  }

  return false;
}

static blender::bke::GeometrySet curve_calc_modifiers_post(Depsgraph *depsgraph,
                                                           const Scene *scene,
                                                           Object *ob,
                                                           const ListBase *dispbase,
                                                           const bool for_render)
{
  const Curve *cu = (const Curve *)ob->data;
  const bool editmode = (!for_render && (cu->editnurb || cu->editfont));
  const bool use_cache = !for_render;

  ModifierApplyFlag apply_flag = for_render ? MOD_APPLY_RENDER : ModifierApplyFlag(0);
  ModifierMode required_mode = for_render ? eModifierMode_Render : eModifierMode_Realtime;
  if (editmode) {
    required_mode = ModifierMode(int(required_mode) | eModifierMode_Editmode);
  }

  const ModifierEvalContext mectx_deform = {
      depsgraph, ob, editmode ? (apply_flag | MOD_APPLY_USECACHE) : apply_flag};
  const ModifierEvalContext mectx_apply = {
      depsgraph, ob, use_cache ? (apply_flag | MOD_APPLY_USECACHE) : apply_flag};

  ModifierData *pretessellatePoint = curve_get_tessellate_point(scene, ob, for_render, editmode);

  VirtualModifierData virtual_modifier_data;
  ModifierData *md = pretessellatePoint == nullptr ?
                         BKE_modifiers_get_virtual_modifierlist(ob, &virtual_modifier_data) :
                         pretessellatePoint->next;

  blender::bke::GeometrySet geometry_set;
  if (ob->type == OB_SURF ||
      do_curve_implicit_mesh_conversion(cu, md, scene, required_mode, editmode))
  {
    Mesh *mesh = BKE_mesh_new_nomain_from_curve_displist(ob, dispbase);
    geometry_set.replace_mesh(mesh);
  }
  else {
    geometry_set.replace_curves(
        blender::bke::curve_legacy_to_curves(*cu, ob->runtime->curve_cache->deformed_nurbs));
  }

  for (; md; md = md->next) {
    const ModifierTypeInfo *mti = BKE_modifier_get_info((ModifierType)md->type);
    if (!BKE_modifier_is_enabled(scene, md, required_mode)) {
      continue;
    }

    blender::bke::ScopedModifierTimer modifier_timer{*md};

    if (md->type == eModifierType_Nodes) {
      mti->modify_geometry_set(md, &mectx_apply, &geometry_set);
      continue;
    }

    if (!geometry_set.has_mesh()) {
      geometry_set.replace_mesh(BKE_mesh_new_nomain(0, 0, 0, 0));
    }
    Mesh *mesh = geometry_set.get_mesh_for_write();

    if (mti->type == ModifierTypeType::OnlyDeform) {
      mti->deform_verts(md, &mectx_deform, mesh, mesh->vert_positions_for_write());
      mesh->tag_positions_changed();
    }
    else {
      Mesh *output_mesh = mti->modify_mesh(md, &mectx_apply, mesh);
      if (mesh != output_mesh) {
        geometry_set.replace_mesh(output_mesh);
      }
    }
  }

  if (geometry_set.has_mesh()) {
    Mesh *final_mesh = geometry_set.get_mesh_for_write();
    STRNCPY(final_mesh->id.name, cu->id.name);
    *((short *)final_mesh->id.name) = ID_ME;
  }

  return geometry_set;
}

static void displist_surf_indices(DispList *dl)
{
  int b, p1, p2, p3, p4;

  dl->totindex = 0;

  int *index = dl->index = MEM_malloc_arrayN<int>(4 * size_t(dl->parts + 1) * size_t(dl->nr + 1),
                                                  __func__);

  for (int a = 0; a < dl->parts; a++) {

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

static blender::bke::GeometrySet evaluate_surface_object(Depsgraph *depsgraph,
                                                         const Scene *scene,
                                                         Object *ob,
                                                         const bool for_render,
                                                         ListBase *r_dispbase)
{
  BLI_assert(ob->type == OB_SURF);
  const Curve *cu = (const Curve *)ob->data;

  ListBase *deformed_nurbs = &ob->runtime->curve_cache->deformed_nurbs;

  if (!for_render && cu->editnurb) {
    BKE_nurbList_duplicate(deformed_nurbs, BKE_curve_editNurbs_get_for_read(cu));
  }
  else {
    BKE_nurbList_duplicate(deformed_nurbs, &cu->nurb);
  }

  BKE_curve_calc_modifiers_pre(depsgraph, scene, ob, deformed_nurbs, deformed_nurbs, for_render);

  LISTBASE_FOREACH (const Nurb *, nu, deformed_nurbs) {
    if (!(for_render || nu->hide == 0) || !BKE_nurb_check_valid_uv(nu)) {
      continue;
    }

    const int resolu = (for_render && cu->resolu_ren) ? cu->resolu_ren : nu->resolu;
    const int resolv = (for_render && cu->resolv_ren) ? cu->resolv_ren : nu->resolv;

    if (nu->pntsv == 1) {
      const int len = SEGMENTSU(nu) * resolu;

      DispList *dl = MEM_callocN<DispList>(__func__);
      dl->verts = MEM_malloc_arrayN<float>(3 * size_t(len), __func__);

      BLI_addtail(r_dispbase, dl);
      dl->parts = 1;
      dl->nr = len;
      dl->col = nu->mat_nr;
      dl->charidx = nu->charidx;
      dl->rt = nu->flag;

      float *data = dl->verts;
      if (nu->flagu & CU_NURB_CYCLIC) {
        dl->type = DL_POLY;
      }
      else {
        dl->type = DL_SEGM;
      }

      BKE_nurb_makeCurve(nu, data, nullptr, nullptr, nullptr, resolu, sizeof(float[3]));
    }
    else {
      const int len = (nu->pntsu * resolu) * (nu->pntsv * resolv);

      DispList *dl = MEM_callocN<DispList>(__func__);
      dl->verts = MEM_malloc_arrayN<float>(3 * size_t(len), __func__);
      BLI_addtail(r_dispbase, dl);

      dl->col = nu->mat_nr;
      dl->charidx = nu->charidx;
      dl->rt = nu->flag;

      float *data = dl->verts;
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

  curve_to_filledpoly(cu, r_dispbase);
  blender::bke::GeometrySet geometry_set = curve_calc_modifiers_post(
      depsgraph, scene, ob, r_dispbase, for_render);
  if (!geometry_set.has_mesh()) {
    geometry_set.replace_mesh(BKE_mesh_new_nomain(0, 0, 0, 0));
  }
  return geometry_set;
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

      if (nbevp == nullptr) {
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

      if (nbevp == nullptr) {
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

static void fillBevelCap(const Nurb *nu,
                         const DispList *dlb,
                         const float *prev_fp,
                         ListBase *dispbase)
{
  DispList *dl = MEM_callocN<DispList>(__func__);
  dl->verts = MEM_malloc_arrayN<float>(3 * size_t(dlb->nr), __func__);
  memcpy(dl->verts, prev_fp, sizeof(float[3]) * dlb->nr);

  dl->type = DL_POLY;

  dl->parts = 1;
  dl->nr = dlb->nr;
  dl->col = nu->mat_nr;
  dl->charidx = nu->charidx;
  dl->rt = nu->flag;

  BLI_addtail(dispbase, dl);
}

static void calc_bevfac_segment_mapping(
    const BevList *bl, float bevfac, float spline_length, int *r_bev, float *r_blend)
{
  float normsum = 0.0f;
  float *seglen = bl->seglen;
  int *segbevcount = bl->segbevcount;
  int bevcount = 0, nr = bl->nr;

  float bev_fl = bevfac * (bl->nr - 1);
  *r_bev = int(bev_fl);

  while (bevcount < nr - 1) {
    float normlen = *seglen / spline_length;
    if (normsum + normlen > bevfac) {
      bev_fl = bevcount + (bevfac - normsum) / normlen * *segbevcount;
      *r_bev = int(bev_fl);
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
    const BevList *bl, float bevfac, float spline_length, int *r_bev, float *r_blend)
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
    const BevList *bl, int *r_start, float *r_firstblend, int *r_steps, float *r_lastblend)
{
  *r_start = 0;
  *r_steps = bl->nr;
  *r_firstblend = 1.0f;
  *r_lastblend = 1.0f;
}

static void calc_bevfac_mapping(const Curve *cu,
                                const BevList *bl,
                                const Nurb *nu,
                                int *r_start,
                                float *r_firstblend,
                                int *r_steps,
                                float *r_lastblend)
{
  float tmpf, total_length = 0.0f;
  int end = 0, i;

  if ((BKE_nurb_check_valid_u(nu) == false) ||
      /* not essential, but skips unnecessary calculation */
      (min_ff(cu->bevfac1, cu->bevfac2) == 0.0f && max_ff(cu->bevfac1, cu->bevfac2) == 1.0f))
  {
    calc_bevfac_mapping_default(bl, r_start, r_firstblend, r_steps, r_lastblend);
    return;
  }

  if (ELEM(cu->bevfac1_mapping, CU_BEVFAC_MAP_SEGMENT, CU_BEVFAC_MAP_SPLINE) ||
      ELEM(cu->bevfac2_mapping, CU_BEVFAC_MAP_SEGMENT, CU_BEVFAC_MAP_SPLINE))
  {
    for (i = 0; i < SEGMENTSU(nu); i++) {
      total_length += bl->seglen[i];
    }
  }

  switch (cu->bevfac1_mapping) {
    case CU_BEVFAC_MAP_RESOLU: {
      const float start_fl = cu->bevfac1 * (bl->nr - 1);
      *r_start = int(start_fl);
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
      end = int(end_fl);

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
    std::swap(*r_start, end);
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

static blender::bke::GeometrySet evaluate_curve_type_object(Depsgraph *depsgraph,
                                                            const Scene *scene,
                                                            Object *ob,
                                                            const bool for_render,
                                                            ListBase *r_dispbase)
{
  BLI_assert(ELEM(ob->type, OB_CURVES_LEGACY, OB_FONT));
  const Curve *cu = (const Curve *)ob->data;

  ListBase *deformed_nurbs = &ob->runtime->curve_cache->deformed_nurbs;

  if (ob->type == OB_FONT) {
    BKE_vfont_to_curve_nubase(ob, FO_EDIT, deformed_nurbs);
  }
  else {
    BKE_nurbList_duplicate(deformed_nurbs, BKE_curve_nurbs_get_for_read(cu));
  }

  BKE_curve_calc_modifiers_pre(depsgraph, scene, ob, deformed_nurbs, deformed_nurbs, for_render);

  BKE_curve_bevelList_make(ob, deformed_nurbs, for_render);

  if ((cu->flag & CU_PATH) ||
      DEG_get_eval_flags_for_id(depsgraph, &ob->id) & DAG_EVAL_NEED_CURVE_PATH)
  {
    BKE_anim_path_calc_data(ob);
  }

  /* If curve has no bevel will return nothing */
  ListBase dlbev = BKE_curve_bevel_make(cu);

  /* no bevel or extrude, and no width correction? */
  if (BLI_listbase_is_empty(&dlbev) && cu->offset == 1.0f) {
    curve_to_displist(cu, deformed_nurbs, for_render, r_dispbase);
  }
  else {
    const float widfac = cu->offset - 1.0f;

    const BevList *bl = (BevList *)ob->runtime->curve_cache->bev.first;
    const Nurb *nu = (Nurb *)deformed_nurbs->first;
    for (; bl && nu; bl = bl->next, nu = nu->next) {
      float *data;

      if (bl->nr == 0) { /* blank bevel lists can happen */
        continue;
      }

      /* exception handling; curve without bevel or extrude, with width correction */
      if (BLI_listbase_is_empty(&dlbev)) {
        DispList *dl = MEM_callocN<DispList>("makeDispListbev");
        dl->verts = MEM_malloc_arrayN<float>(3 * size_t(bl->nr), "dlverts");
        BLI_addtail(r_dispbase, dl);

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
        dl->rt = nu->flag;

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
        ListBase bottom_capbase = {nullptr, nullptr};
        ListBase top_capbase = {nullptr, nullptr};
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
          /* For each part of the bevel use a separate display-block. */
          DispList *dl = MEM_callocN<DispList>(__func__);
          dl->verts = data = MEM_malloc_arrayN<float>(3 * size_t(dlb->nr) * size_t(steps),
                                                      __func__);
          BLI_addtail(r_dispbase, dl);

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
          dl->rt = nu->flag;

          /* for each point of poly make a bevel piece */
          BevPoint *bevp_first = bl->bevpoints;
          BevPoint *bevp_last = &bl->bevpoints[bl->nr - 1];
          BevPoint *bevp = &bl->bevpoints[start];
          for (int i = start, a = 0; a < steps; i++, bevp++, a++) {
            float radius_factor = 1.0;
            float *cur_data = data;

            if (cu->taperobj == nullptr) {
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
                  taper_factor = (float(a) - (1.0f - first_blend)) / len;
                }
              }
              else {
                float len = bl->nr - 1;
                taper_factor = float(i) / len;

                if (a == 0) {
                  taper_factor += (1.0f - first_blend) / len;
                }
                else if (a == steps - 1) {
                  taper_factor -= (1.0f - last_blend) / len;
                }
              }

              radius_factor = displist_calc_taper(depsgraph, scene, cu->taperobj, taper_factor);

              if (cu->taper_radius_mode == CU_TAPER_RADIUS_MULTIPLY) {
                radius_factor *= bevp->radius;
              }
              else if (cu->taper_radius_mode == CU_TAPER_RADIUS_ADD) {
                radius_factor += bevp->radius;
              }
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
              rotateBevelPiece(cu, bevp, nullptr, dlb, 0.0f, widfac, radius_factor, &data);
            }

            if ((cu->flag & CU_FILL_CAPS) && !(nu->flagu & CU_NURB_CYCLIC)) {
              if (a == 1) {
                /* Can occur when the `bevp->vec` is NAN, see: #141612. */
                if (len_squared_v3(bevp->dir) > 0.0f) {
                  fillBevelCap(nu, dlb, cur_data - 3 * dlb->nr, &bottom_capbase);
                  copy_v3_v3(bottom_no, bevp->dir);
                }
              }
              if (a == steps - 1) {
                /* Can occur when the `bevp->vec` is NAN, see: #141612. */
                if (len_squared_v3(bevp->dir) > 0.0f) {
                  fillBevelCap(nu, dlb, cur_data, &top_capbase);
                  negate_v3_v3(top_no, bevp->dir);
                }
              }
            }
          }

          /* gl array drawing: using indices */
          displist_surf_indices(dl);
        }

        if (bottom_capbase.first) {
          BKE_displist_fill(&bottom_capbase, r_dispbase, bottom_no, false);
          BKE_displist_free(&bottom_capbase);
        }
        if (top_capbase.first) {
          BKE_displist_fill(&top_capbase, r_dispbase, top_no, false);
          BKE_displist_free(&top_capbase);
        }
      }
    }
  }

  BKE_displist_free(&dlbev);

  curve_to_filledpoly(cu, r_dispbase);
  return curve_calc_modifiers_post(depsgraph, scene, ob, r_dispbase, for_render);
}

void BKE_displist_make_curveTypes(Depsgraph *depsgraph,
                                  const Scene *scene,
                                  Object *ob,
                                  const bool for_render)
{
  BLI_assert(ELEM(ob->type, OB_SURF, OB_CURVES_LEGACY, OB_FONT));

  BKE_object_free_derived_caches(ob);

  /* It's important to retrieve this after calling #BKE_object_free_derived_caches,
   * which may reset the object data pointer in some cases. */
  const Curve &original_curve = *static_cast<const Curve *>(ob->data);

  ob->runtime->curve_cache = MEM_callocN<CurveCache>(__func__);
  ListBase *dispbase = &ob->runtime->curve_cache->disp;

  if (ob->type == OB_SURF) {
    blender::bke::GeometrySet geometry = evaluate_surface_object(
        depsgraph, scene, ob, for_render, dispbase);
    ob->runtime->geometry_set_eval = new blender::bke::GeometrySet(std::move(geometry));
  }
  else {
    blender::bke::GeometrySet geometry = evaluate_curve_type_object(
        depsgraph, scene, ob, for_render, dispbase);

    if (geometry.has_curves()) {
      /* Create a copy of the original curve and add necessary pointers to evaluated and edit mode
       * data. This is needed for a few reasons:
       * - Existing code from before curve evaluation was changed to use #GeometrySet expected to
       *   have a copy of the original curve data. (Any evaluated data was placed in
       *   #Object.runtime->curve_cache).
       * - The result of modifier evaluation is not a #Curve data-block but a #Curves data-block,
       *   which can support constructive modifiers and geometry nodes.
       * - The dependency graph has handling of edit mode pointers (see #update_edit_mode_pointers)
       *   but it doesn't seem to work in this case.
       *
       * Since the plan is to replace this legacy curve object with the curves data-block
       * (see #95355), this somewhat hacky inefficient solution is relatively temporary.
       */
      Curve &cow_curve = *reinterpret_cast<Curve *>(
          BKE_id_copy_ex(nullptr, &original_curve.id, nullptr, LIB_ID_COPY_LOCALIZE));
      cow_curve.curve_eval = geometry.get_curves();
      /* Copy edit mode pointers necessary for drawing to the duplicated curve. */
      cow_curve.editnurb = original_curve.editnurb;
      cow_curve.editfont = original_curve.editfont;
      cow_curve.edit_data_from_original = true;
      BKE_object_eval_assign_data(ob, &cow_curve.id, true);
    }

    ob->runtime->geometry_set_eval = new blender::bke::GeometrySet(std::move(geometry));
  }
}

void BKE_displist_minmax(const ListBase *dispbase, float min[3], float max[3])
{
  bool empty = true;

  LISTBASE_FOREACH (const DispList *, dl, dispbase) {
    const int tot = dl->type == DL_INDEX3 ? dl->nr : dl->nr * dl->parts;
    for (const int i : IndexRange(tot)) {
      minmax_v3v3_v3(min, max, &dl->verts[i * 3]);
    }
    if (tot != 0) {
      empty = false;
    }
  }

  if (empty) {
    zero_v3(min);
    zero_v3(max);
  }
}
