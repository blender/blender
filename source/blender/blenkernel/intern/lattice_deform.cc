/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 *
 * Deform coordinates by a lattice object (used by modifier).
 */

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_simd.h"
#include "BLI_task.h"
#include "BLI_utildefines.h"

#include "DNA_curve_types.h"
#include "DNA_lattice_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BKE_curve.h"
#include "BKE_displist.h"
#include "BKE_editmesh.h"
#include "BKE_key.h"
#include "BKE_lattice.h"
#include "BKE_mesh.hh"
#include "BKE_modifier.h"
#include "BKE_object.h"

#include "BKE_deform.h"

/* -------------------------------------------------------------------- */
/** \name Lattice Deform API
 * \{ */

struct LatticeDeformData {
  /* Convert from object space to deform space */
  float latmat[4][4];
  /* Cached reference to the lattice to use for evaluation. When in edit mode this attribute
   * is set to the edit mode lattice. */
  const Lattice *lt;
  /* Preprocessed lattice points (converted to deform space). */
  float *latticedata;
  /* Prefetched DeformWeights of the lattice. */
  float *lattice_weights;
};

LatticeDeformData *BKE_lattice_deform_data_create(const Object *oblatt, const Object *ob)
{
  /* we make an array with all differences */
  Lattice *lt = BKE_object_get_lattice(oblatt);
  BPoint *bp;
  DispList *dl = oblatt->runtime.curve_cache ?
                     BKE_displist_find(&oblatt->runtime.curve_cache->disp, DL_VERTS) :
                     nullptr;
  const float *co = dl ? dl->verts : nullptr;
  float *fp, imat[4][4];
  float fu, fv, fw;
  int u, v, w;
  float *latticedata;
  float *lattice_weights = nullptr;
  float latmat[4][4];
  LatticeDeformData *lattice_deform_data;

  bp = lt->def;

  const int32_t num_points = lt->pntsu * lt->pntsv * lt->pntsw;
  /* We allocate one additional float for SSE2 optimizations. Without this
   * the SSE2 instructions for the last item would read in unallocated memory. */
  fp = latticedata = static_cast<float *>(
      MEM_mallocN(sizeof(float[3]) * num_points + sizeof(float), "latticedata"));

  /* for example with a particle system: (ob == nullptr) */
  if (ob == nullptr) {
    /* In deform-space, calc matrix. */
    invert_m4_m4(latmat, oblatt->object_to_world);

    /* back: put in deform array */
    invert_m4_m4(imat, latmat);
  }
  else {
    /* In deform-space, calc matrix. */
    invert_m4_m4(imat, oblatt->object_to_world);
    mul_m4_m4m4(latmat, imat, ob->object_to_world);

    /* back: put in deform array. */
    invert_m4_m4(imat, latmat);
  }

  /* Prefetch lattice deform group weights. */
  int defgrp_index = -1;
  const MDeformVert *dvert = BKE_lattice_deform_verts_get(oblatt);
  if (lt->vgroup[0] && dvert) {
    defgrp_index = BKE_id_defgroup_name_index(&lt->id, lt->vgroup);

    if (defgrp_index != -1) {
      lattice_weights = static_cast<float *>(
          MEM_malloc_arrayN(num_points, sizeof(float), "lattice_weights"));
      for (int index = 0; index < num_points; index++) {
        lattice_weights[index] = BKE_defvert_find_weight(dvert + index, defgrp_index);
      }
    }
  }

  for (w = 0, fw = lt->fw; w < lt->pntsw; w++, fw += lt->dw) {
    for (v = 0, fv = lt->fv; v < lt->pntsv; v++, fv += lt->dv) {
      for (u = 0, fu = lt->fu; u < lt->pntsu; u++, bp++, co += 3, fp += 3, fu += lt->du) {
        if (dl) {
          fp[0] = co[0] - fu;
          fp[1] = co[1] - fv;
          fp[2] = co[2] - fw;
        }
        else {
          fp[0] = bp->vec[0] - fu;
          fp[1] = bp->vec[1] - fv;
          fp[2] = bp->vec[2] - fw;
        }

        mul_mat3_m4_v3(imat, fp);
      }
    }
  }

  lattice_deform_data = static_cast<LatticeDeformData *>(
      MEM_mallocN(sizeof(LatticeDeformData), "Lattice Deform Data"));
  lattice_deform_data->latticedata = latticedata;
  lattice_deform_data->lattice_weights = lattice_weights;
  lattice_deform_data->lt = lt;
  copy_m4_m4(lattice_deform_data->latmat, latmat);

  return lattice_deform_data;
}

void BKE_lattice_deform_data_eval_co(LatticeDeformData *lattice_deform_data,
                                     float co[3],
                                     float weight)
{
  float *latticedata = lattice_deform_data->latticedata;
  float *lattice_weights = lattice_deform_data->lattice_weights;
  BLI_assert(latticedata);
  const Lattice *lt = lattice_deform_data->lt;
  float u, v, w, tu[4], tv[4], tw[4];
  float vec[3];
  int idx_w, idx_v, idx_u;
  int ui, vi, wi, uu, vv, ww;

  /* vgroup influence */
  float co_prev[4] = {0}, weight_blend = 0.0f;
  copy_v3_v3(co_prev, co);
#if BLI_HAVE_SSE2
  __m128 co_vec = _mm_loadu_ps(co_prev);
#endif

  /* co is in local coords, treat with latmat */
  mul_v3_m4v3(vec, lattice_deform_data->latmat, co);

  /* u v w coords */

  if (lt->pntsu > 1) {
    u = (vec[0] - lt->fu) / lt->du;
    ui = int(floor(u));
    u -= ui;
    key_curve_position_weights(u, tu, lt->typeu);
  }
  else {
    tu[0] = tu[2] = tu[3] = 0.0;
    tu[1] = 1.0;
    ui = 0;
  }

  if (lt->pntsv > 1) {
    v = (vec[1] - lt->fv) / lt->dv;
    vi = int(floor(v));
    v -= vi;
    key_curve_position_weights(v, tv, lt->typev);
  }
  else {
    tv[0] = tv[2] = tv[3] = 0.0;
    tv[1] = 1.0;
    vi = 0;
  }

  if (lt->pntsw > 1) {
    w = (vec[2] - lt->fw) / lt->dw;
    wi = int(floor(w));
    w -= wi;
    key_curve_position_weights(w, tw, lt->typew);
  }
  else {
    tw[0] = tw[2] = tw[3] = 0.0;
    tw[1] = 1.0;
    wi = 0;
  }

  const int w_stride = lt->pntsu * lt->pntsv;
  const int idx_w_max = (lt->pntsw - 1) * lt->pntsu * lt->pntsv;
  const int v_stride = lt->pntsu;
  const int idx_v_max = (lt->pntsv - 1) * lt->pntsu;
  const int idx_u_max = (lt->pntsu - 1);

  for (ww = wi - 1; ww <= wi + 2; ww++) {
    w = weight * tw[ww - wi + 1];
    idx_w = CLAMPIS(ww * w_stride, 0, idx_w_max);
    for (vv = vi - 1; vv <= vi + 2; vv++) {
      v = w * tv[vv - vi + 1];
      idx_v = CLAMPIS(vv * v_stride, 0, idx_v_max);
      for (uu = ui - 1; uu <= ui + 2; uu++) {
        u = v * tu[uu - ui + 1];
        idx_u = CLAMPIS(uu, 0, idx_u_max);
        const int idx = idx_w + idx_v + idx_u;
#if BLI_HAVE_SSE2
        {
          __m128 weight_vec = _mm_set1_ps(u);
          /* We need to address special case for last item to avoid accessing invalid memory. */
          __m128 lattice_vec;
          if (idx * 3 == idx_w_max) {
            copy_v3_v3((float *)&lattice_vec, &latticedata[idx * 3]);
          }
          else {
            /* When not on last item, we can safely access one extra float, it will be ignored
             * anyway. */
            lattice_vec = _mm_loadu_ps(&latticedata[idx * 3]);
          }
          co_vec = _mm_add_ps(co_vec, _mm_mul_ps(lattice_vec, weight_vec));
        }
#else
        madd_v3_v3fl(co, &latticedata[idx * 3], u);
#endif
        if (lattice_weights) {
          weight_blend += (u * lattice_weights[idx]);
        }
      }
    }
  }
#if BLI_HAVE_SSE2
  {
    copy_v3_v3(co, (float *)&co_vec);
  }
#endif

  if (lattice_weights) {
    interp_v3_v3v3(co, co_prev, co, weight_blend);
  }
}

void BKE_lattice_deform_data_destroy(LatticeDeformData *lattice_deform_data)
{
  if (lattice_deform_data->latticedata) {
    MEM_freeN(lattice_deform_data->latticedata);
  }

  MEM_freeN(lattice_deform_data);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Lattice Deform #BKE_lattice_deform_coords API
 *
 * #BKE_lattice_deform_coords and related functions.
 * \{ */

struct LatticeDeformUserdata {
  LatticeDeformData *lattice_deform_data;
  float (*vert_coords)[3];
  const MDeformVert *dvert;
  int defgrp_index;
  float fac;
  bool invert_vgroup;

  /** Specific data types. */
  struct {
    int cd_dvert_offset;
  } bmesh;
};

static void lattice_deform_vert_with_dvert(const LatticeDeformUserdata *data,
                                           const int index,
                                           const MDeformVert *dvert)
{
  if (dvert != nullptr) {
    const float weight = data->invert_vgroup ?
                             1.0f - BKE_defvert_find_weight(dvert, data->defgrp_index) :
                             BKE_defvert_find_weight(dvert, data->defgrp_index);
    if (weight > 0.0f) {
      BKE_lattice_deform_data_eval_co(
          data->lattice_deform_data, data->vert_coords[index], weight * data->fac);
    }
  }
  else {
    BKE_lattice_deform_data_eval_co(
        data->lattice_deform_data, data->vert_coords[index], data->fac);
  }
}

static void lattice_deform_vert_task(void *__restrict userdata,
                                     const int index,
                                     const TaskParallelTLS *__restrict /*tls*/)
{
  const LatticeDeformUserdata *data = static_cast<const LatticeDeformUserdata *>(userdata);
  lattice_deform_vert_with_dvert(data, index, data->dvert ? &data->dvert[index] : nullptr);
}

static void lattice_vert_task_editmesh(void *__restrict userdata,
                                       MempoolIterData *iter,
                                       const TaskParallelTLS *__restrict /*tls*/)
{
  const LatticeDeformUserdata *data = static_cast<const LatticeDeformUserdata *>(userdata);
  BMVert *v = (BMVert *)iter;
  MDeformVert *dvert = static_cast<MDeformVert *>(
      BM_ELEM_CD_GET_VOID_P(v, data->bmesh.cd_dvert_offset));
  lattice_deform_vert_with_dvert(data, BM_elem_index_get(v), dvert);
}

static void lattice_vert_task_editmesh_no_dvert(void *__restrict userdata,
                                                MempoolIterData *iter,
                                                const TaskParallelTLS *__restrict /*tls*/)
{
  const LatticeDeformUserdata *data = static_cast<const LatticeDeformUserdata *>(userdata);
  BMVert *v = (BMVert *)iter;
  lattice_deform_vert_with_dvert(data, BM_elem_index_get(v), nullptr);
}

static void lattice_deform_coords_impl(const Object *ob_lattice,
                                       const Object *ob_target,
                                       float (*vert_coords)[3],
                                       const int vert_coords_len,
                                       const short flag,
                                       const char *defgrp_name,
                                       const float fac,
                                       const Mesh *me_target,
                                       BMEditMesh *em_target)
{
  LatticeDeformData *lattice_deform_data;
  const MDeformVert *dvert = nullptr;
  int defgrp_index = -1;
  int cd_dvert_offset = -1;

  if (ob_lattice->type != OB_LATTICE) {
    return;
  }

  lattice_deform_data = BKE_lattice_deform_data_create(ob_lattice, ob_target);

  /* Check whether to use vertex groups (only possible if ob_target is a Mesh or Lattice).
   * We want either a Mesh/Lattice with no derived data, or derived data with deformverts.
   */
  if (defgrp_name && defgrp_name[0] && ob_target && ELEM(ob_target->type, OB_MESH, OB_LATTICE)) {
    defgrp_index = BKE_id_defgroup_name_index(me_target ? &me_target->id : (ID *)ob_target->data,
                                              defgrp_name);

    if (defgrp_index != -1) {
      /* if there's derived data without deformverts, don't use vgroups */
      if (em_target) {
        cd_dvert_offset = CustomData_get_offset(&em_target->bm->vdata, CD_MDEFORMVERT);
      }
      else if (me_target) {
        dvert = static_cast<const MDeformVert *>(
            CustomData_get_layer(&me_target->vert_data, CD_MDEFORMVERT));
      }
      else if (ob_target->type == OB_LATTICE) {
        dvert = ((Lattice *)ob_target->data)->dvert;
      }
      else {
        dvert = BKE_mesh_deform_verts((Mesh *)ob_target->data);
      }
    }
  }

  LatticeDeformUserdata data{};
  data.lattice_deform_data = lattice_deform_data;
  data.vert_coords = vert_coords;
  data.dvert = dvert;
  data.defgrp_index = defgrp_index;
  data.fac = fac;
  data.invert_vgroup = (flag & MOD_LATTICE_INVERT_VGROUP) != 0;
  data.bmesh.cd_dvert_offset = cd_dvert_offset;

  if (em_target != nullptr) {
    /* While this could cause an extra loop over mesh data, in most cases this will
     * have already been properly set. */
    BM_mesh_elem_index_ensure(em_target->bm, BM_VERT);

    TaskParallelSettings settings;
    BLI_parallel_mempool_settings_defaults(&settings);

    if (cd_dvert_offset != -1) {
      BLI_task_parallel_mempool(
          em_target->bm->vpool, &data, lattice_vert_task_editmesh, &settings);
    }
    else {
      BLI_task_parallel_mempool(
          em_target->bm->vpool, &data, lattice_vert_task_editmesh_no_dvert, &settings);
    }
  }
  else {
    TaskParallelSettings settings;
    BLI_parallel_range_settings_defaults(&settings);
    settings.min_iter_per_thread = 32;
    BLI_task_parallel_range(0, vert_coords_len, &data, lattice_deform_vert_task, &settings);
  }

  BKE_lattice_deform_data_destroy(lattice_deform_data);
}

void BKE_lattice_deform_coords(const Object *ob_lattice,
                               const Object *ob_target,
                               float (*vert_coords)[3],
                               const int vert_coords_len,
                               const short flag,
                               const char *defgrp_name,
                               float fac)
{
  lattice_deform_coords_impl(ob_lattice,
                             ob_target,
                             vert_coords,
                             vert_coords_len,
                             flag,
                             defgrp_name,
                             fac,
                             nullptr,
                             nullptr);
}

void BKE_lattice_deform_coords_with_mesh(const Object *ob_lattice,
                                         const Object *ob_target,
                                         float (*vert_coords)[3],
                                         const int vert_coords_len,
                                         const short flag,
                                         const char *defgrp_name,
                                         const float fac,
                                         const Mesh *me_target)
{
  lattice_deform_coords_impl(ob_lattice,
                             ob_target,
                             vert_coords,
                             vert_coords_len,
                             flag,
                             defgrp_name,
                             fac,
                             me_target,
                             nullptr);
}

void BKE_lattice_deform_coords_with_editmesh(const Object *ob_lattice,
                                             const Object *ob_target,
                                             float (*vert_coords)[3],
                                             const int vert_coords_len,
                                             const short flag,
                                             const char *defgrp_name,
                                             const float fac,
                                             BMEditMesh *em_target)
{
  lattice_deform_coords_impl(ob_lattice,
                             ob_target,
                             vert_coords,
                             vert_coords_len,
                             flag,
                             defgrp_name,
                             fac,
                             nullptr,
                             em_target);
}

/** \} */
