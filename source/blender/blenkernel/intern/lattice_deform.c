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
 *
 * Deform coordinates by a lattice object (used by modifier).
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_task.h"
#include "BLI_utildefines.h"

#include "DNA_curve_types.h"
#include "DNA_lattice_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BKE_curve.h"
#include "BKE_displist.h"
#include "BKE_key.h"
#include "BKE_lattice.h"
#include "BKE_modifier.h"

#include "BKE_deform.h"

/* -------------------------------------------------------------------- */
/** \name Lattice Deform API
 * \{ */

typedef struct LatticeDeformData {
  Object *object;
  float *latticedata;
  float latmat[4][4];
} LatticeDeformData;

LatticeDeformData *init_latt_deform(Object *oblatt, Object *ob)
{
  /* we make an array with all differences */
  Lattice *lt = oblatt->data;
  BPoint *bp;
  DispList *dl = oblatt->runtime.curve_cache ?
                     BKE_displist_find(&oblatt->runtime.curve_cache->disp, DL_VERTS) :
                     NULL;
  const float *co = dl ? dl->verts : NULL;
  float *fp, imat[4][4];
  float fu, fv, fw;
  int u, v, w;
  float *latticedata;
  float latmat[4][4];
  LatticeDeformData *lattice_deform_data;

  if (lt->editlatt) {
    lt = lt->editlatt->latt;
  }
  bp = lt->def;

  fp = latticedata = MEM_mallocN(sizeof(float) * 3 * lt->pntsu * lt->pntsv * lt->pntsw,
                                 "latticedata");

  /* for example with a particle system: (ob == NULL) */
  if (ob == NULL) {
    /* in deformspace, calc matrix  */
    invert_m4_m4(latmat, oblatt->obmat);

    /* back: put in deform array */
    invert_m4_m4(imat, latmat);
  }
  else {
    /* in deformspace, calc matrix */
    invert_m4_m4(imat, oblatt->obmat);
    mul_m4_m4m4(latmat, imat, ob->obmat);

    /* back: put in deform array */
    invert_m4_m4(imat, latmat);
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

  lattice_deform_data = MEM_mallocN(sizeof(LatticeDeformData), "Lattice Deform Data");
  lattice_deform_data->latticedata = latticedata;
  lattice_deform_data->object = oblatt;
  copy_m4_m4(lattice_deform_data->latmat, latmat);

  return lattice_deform_data;
}

void calc_latt_deform(LatticeDeformData *lattice_deform_data, float co[3], float weight)
{
  Object *ob = lattice_deform_data->object;
  Lattice *lt = ob->data;
  float u, v, w, tu[4], tv[4], tw[4];
  float vec[3];
  int idx_w, idx_v, idx_u;
  int ui, vi, wi, uu, vv, ww;

  /* vgroup influence */
  int defgrp_index = -1;
  float co_prev[3], weight_blend = 0.0f;
  MDeformVert *dvert = BKE_lattice_deform_verts_get(ob);
  float *__restrict latticedata = lattice_deform_data->latticedata;

  if (lt->editlatt) {
    lt = lt->editlatt->latt;
  }
  if (latticedata == NULL) {
    return;
  }

  if (lt->vgroup[0] && dvert) {
    defgrp_index = BKE_object_defgroup_name_index(ob, lt->vgroup);
    copy_v3_v3(co_prev, co);
  }

  /* co is in local coords, treat with latmat */
  mul_v3_m4v3(vec, lattice_deform_data->latmat, co);

  /* u v w coords */

  if (lt->pntsu > 1) {
    u = (vec[0] - lt->fu) / lt->du;
    ui = (int)floor(u);
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
    vi = (int)floor(v);
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
    wi = (int)floor(w);
    w -= wi;
    key_curve_position_weights(w, tw, lt->typew);
  }
  else {
    tw[0] = tw[2] = tw[3] = 0.0;
    tw[1] = 1.0;
    wi = 0;
  }

  for (ww = wi - 1; ww <= wi + 2; ww++) {
    w = tw[ww - wi + 1];

    if (w != 0.0f) {
      if (ww > 0) {
        if (ww < lt->pntsw) {
          idx_w = ww * lt->pntsu * lt->pntsv;
        }
        else {
          idx_w = (lt->pntsw - 1) * lt->pntsu * lt->pntsv;
        }
      }
      else {
        idx_w = 0;
      }

      for (vv = vi - 1; vv <= vi + 2; vv++) {
        v = w * tv[vv - vi + 1];

        if (v != 0.0f) {
          if (vv > 0) {
            if (vv < lt->pntsv) {
              idx_v = idx_w + vv * lt->pntsu;
            }
            else {
              idx_v = idx_w + (lt->pntsv - 1) * lt->pntsu;
            }
          }
          else {
            idx_v = idx_w;
          }

          for (uu = ui - 1; uu <= ui + 2; uu++) {
            u = weight * v * tu[uu - ui + 1];

            if (u != 0.0f) {
              if (uu > 0) {
                if (uu < lt->pntsu) {
                  idx_u = idx_v + uu;
                }
                else {
                  idx_u = idx_v + (lt->pntsu - 1);
                }
              }
              else {
                idx_u = idx_v;
              }

              madd_v3_v3fl(co, &latticedata[idx_u * 3], u);

              if (defgrp_index != -1) {
                weight_blend += (u * BKE_defvert_find_weight(dvert + idx_u, defgrp_index));
              }
            }
          }
        }
      }
    }
  }

  if (defgrp_index != -1) {
    interp_v3_v3v3(co, co_prev, co, weight_blend);
  }
}

void end_latt_deform(LatticeDeformData *lattice_deform_data)
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

typedef struct LatticeDeformUserdata {
  LatticeDeformData *lattice_deform_data;
  float (*vert_coords)[3];
  const MDeformVert *dvert;
  int defgrp_index;
  float fac;
  bool invert_vgroup;
} LatticeDeformUserdata;

static void lattice_deform_vert_task(void *__restrict userdata,
                                     const int index,
                                     const TaskParallelTLS *__restrict UNUSED(tls))
{
  const LatticeDeformUserdata *data = userdata;

  if (data->dvert != NULL) {
    const float weight = data->invert_vgroup ?
                             1.0f -
                                 BKE_defvert_find_weight(data->dvert + index, data->defgrp_index) :
                             BKE_defvert_find_weight(data->dvert + index, data->defgrp_index);
    if (weight > 0.0f) {
      calc_latt_deform(data->lattice_deform_data, data->vert_coords[index], weight * data->fac);
    }
  }
  else {
    calc_latt_deform(data->lattice_deform_data, data->vert_coords[index], data->fac);
  }
}

static void lattice_deform_coords_impl(Object *ob_lattice,
                                       Object *ob_target,
                                       float (*vert_coords)[3],
                                       const int vert_coords_len,
                                       const short flag,
                                       const char *defgrp_name,
                                       const float fac,
                                       const Mesh *me_target)
{
  LatticeDeformData *lattice_deform_data;
  const MDeformVert *dvert = NULL;
  int defgrp_index = -1;

  if (ob_lattice->type != OB_LATTICE) {
    return;
  }

  lattice_deform_data = init_latt_deform(ob_lattice, ob_target);

  /* Check whether to use vertex groups (only possible if ob_target is a Mesh or Lattice).
   * We want either a Mesh/Lattice with no derived data, or derived data with deformverts.
   */
  if (defgrp_name && defgrp_name[0] && ob_target && ELEM(ob_target->type, OB_MESH, OB_LATTICE)) {
    defgrp_index = BKE_object_defgroup_name_index(ob_target, defgrp_name);

    if (defgrp_index != -1) {
      /* if there's derived data without deformverts, don't use vgroups */
      if (me_target) {
        dvert = CustomData_get_layer(&me_target->vdata, CD_MDEFORMVERT);
      }
      else if (ob_target->type == OB_LATTICE) {
        dvert = ((Lattice *)ob_target->data)->dvert;
      }
      else {
        dvert = ((Mesh *)ob_target->data)->dvert;
      }
    }
  }

  LatticeDeformUserdata data = {
      .lattice_deform_data = lattice_deform_data,
      .vert_coords = vert_coords,
      .dvert = dvert,
      .defgrp_index = defgrp_index,
      .fac = fac,
      .invert_vgroup = (flag & MOD_LATTICE_INVERT_VGROUP) != 0,
  };

  TaskParallelSettings settings;
  BLI_parallel_range_settings_defaults(&settings);
  settings.min_iter_per_thread = 32;
  BLI_task_parallel_range(0, vert_coords_len, &data, lattice_deform_vert_task, &settings);

  end_latt_deform(lattice_deform_data);
}

void BKE_lattice_deform_coords(Object *ob_lattice,
                               Object *ob_target,
                               float (*vert_coords)[3],
                               int vert_coords_len,
                               short flag,
                               const char *defgrp_name,
                               float fac)
{
  lattice_deform_coords_impl(
      ob_lattice, ob_target, vert_coords, vert_coords_len, flag, defgrp_name, fac, NULL);
}

void BKE_lattice_deform_coords_with_mesh(Object *ob_lattice,
                                         Object *ob_target,
                                         float (*vert_coords)[3],
                                         const int vert_coords_len,
                                         const short flag,
                                         const char *defgrp_name,
                                         const float fac,
                                         const Mesh *me_target)
{
  lattice_deform_coords_impl(
      ob_lattice, ob_target, vert_coords, vert_coords_len, flag, defgrp_name, fac, me_target);
}

/** \} */
