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
 * The Original Code is Copyright (C) 2013 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup edsculpt
 *
 * Utility functions for getting vertex locations while painting
 * (since they may be instanced multiple times in an evaluated mesh)
 */

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math.h"

#include "DNA_mesh_types.h"
#include "DNA_object_types.h"

#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_mesh_iterators.h"
#include "BKE_mesh_runtime.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "ED_screen.h"
#include "ED_view3d.h"

#include "paint_intern.h" /* own include */

/* Opaque Structs for internal use */

/* stored while painting */
struct VertProjHandle {
  CoNo *vcosnos;

  bool use_update;

  /* use for update */
  float *dists_sq;

  Object *ob;
  Scene *scene;
};

/* only for passing to the callbacks */
struct VertProjUpdate {
  struct VertProjHandle *vp_handle;

  /* runtime */
  ARegion *region;
  const float *mval_fl;
};

/* -------------------------------------------------------------------- */
/* Internal Init */

static void vpaint_proj_dm_map_cosnos_init__map_cb(void *userData,
                                                   int index,
                                                   const float co[3],
                                                   const float no[3])
{
  struct VertProjHandle *vp_handle = userData;
  CoNo *co_no = &vp_handle->vcosnos[index];

  /* check if we've been here before (normal should not be 0) */
  if (!is_zero_v3(co_no->no)) {
    /* remember that multiple dm verts share the same source vert */
    vp_handle->use_update = true;
    return;
  }

  copy_v3_v3(co_no->co, co);
  copy_v3_v3(co_no->no, no);
}

static void vpaint_proj_dm_map_cosnos_init(struct Depsgraph *depsgraph,
                                           Scene *UNUSED(scene),
                                           Object *ob,
                                           struct VertProjHandle *vp_handle)
{
  Scene *scene_eval = DEG_get_evaluated_scene(depsgraph);
  Object *ob_eval = DEG_get_evaluated_object(depsgraph, ob);
  Mesh *me = ob->data;

  CustomData_MeshMasks cddata_masks = CD_MASK_BAREMESH_ORIGINDEX;
  Mesh *me_eval = mesh_get_eval_final(depsgraph, scene_eval, ob_eval, &cddata_masks);

  memset(vp_handle->vcosnos, 0, sizeof(*vp_handle->vcosnos) * me->totvert);
  BKE_mesh_foreach_mapped_vert(
      me_eval, vpaint_proj_dm_map_cosnos_init__map_cb, vp_handle, MESH_FOREACH_USE_NORMAL);
}

/* -------------------------------------------------------------------- */
/* Internal Update */

/* Same as init but take mouse location into account */

static void vpaint_proj_dm_map_cosnos_update__map_cb(void *userData,
                                                     int index,
                                                     const float co[3],
                                                     const float no[3])
{
  struct VertProjUpdate *vp_update = userData;
  struct VertProjHandle *vp_handle = vp_update->vp_handle;

  CoNo *co_no = &vp_handle->vcosnos[index];

  /* find closest vertex */
  {
    /* first find distance to this vertex */
    float co_ss[2]; /* screenspace */

    if (ED_view3d_project_float_object(
            vp_update->region, co, co_ss, V3D_PROJ_TEST_CLIP_BB | V3D_PROJ_TEST_CLIP_NEAR) ==
        V3D_PROJ_RET_OK) {
      const float dist_sq = len_squared_v2v2(vp_update->mval_fl, co_ss);
      if (dist_sq > vp_handle->dists_sq[index]) {
        /* bail out! */
        return;
      }

      vp_handle->dists_sq[index] = dist_sq;
    }
    else if (vp_handle->dists_sq[index] != FLT_MAX) {
      /* already initialized & couldn't project this 'co' */
      return;
    }
  }
  /* continue with regular functionality */

  copy_v3_v3(co_no->co, co);
  copy_v3_v3(co_no->no, no);
}

static void vpaint_proj_dm_map_cosnos_update(struct Depsgraph *depsgraph,
                                             struct VertProjHandle *vp_handle,
                                             ARegion *region,
                                             const float mval_fl[2])
{
  struct VertProjUpdate vp_update = {vp_handle, region, mval_fl};

  Object *ob = vp_handle->ob;
  Scene *scene_eval = DEG_get_evaluated_scene(depsgraph);
  Object *ob_eval = DEG_get_evaluated_object(depsgraph, ob);
  Mesh *me = ob->data;

  CustomData_MeshMasks cddata_masks = CD_MASK_BAREMESH_ORIGINDEX;
  Mesh *me_eval = mesh_get_eval_final(depsgraph, scene_eval, ob_eval, &cddata_masks);

  /* quick sanity check - we shouldn't have to run this if there are no modifiers */
  BLI_assert(BLI_listbase_is_empty(&ob->modifiers) == false);

  copy_vn_fl(vp_handle->dists_sq, me->totvert, FLT_MAX);
  BKE_mesh_foreach_mapped_vert(
      me_eval, vpaint_proj_dm_map_cosnos_update__map_cb, &vp_update, MESH_FOREACH_USE_NORMAL);
}

/* -------------------------------------------------------------------- */
/* Public Functions */

struct VertProjHandle *ED_vpaint_proj_handle_create(struct Depsgraph *depsgraph,
                                                    Scene *scene,
                                                    Object *ob,
                                                    CoNo **r_vcosnos)
{
  struct VertProjHandle *vp_handle = MEM_mallocN(sizeof(struct VertProjHandle), __func__);
  Mesh *me = ob->data;

  /* setup the handle */
  vp_handle->vcosnos = MEM_mallocN(sizeof(CoNo) * me->totvert, "vertexcosnos map");
  vp_handle->use_update = false;

  /* sets 'use_update' if needed */
  vpaint_proj_dm_map_cosnos_init(depsgraph, scene, ob, vp_handle);

  if (vp_handle->use_update) {
    vp_handle->dists_sq = MEM_mallocN(sizeof(float) * me->totvert, __func__);

    vp_handle->ob = ob;
    vp_handle->scene = scene;
  }
  else {
    vp_handle->dists_sq = NULL;

    vp_handle->ob = NULL;
    vp_handle->scene = NULL;
  }

  *r_vcosnos = vp_handle->vcosnos;
  return vp_handle;
}

void ED_vpaint_proj_handle_update(struct Depsgraph *depsgraph,
                                  struct VertProjHandle *vp_handle,
                                  ARegion *region,
                                  const float mval_fl[2])
{
  if (vp_handle->use_update) {
    vpaint_proj_dm_map_cosnos_update(depsgraph, vp_handle, region, mval_fl);
  }
}

void ED_vpaint_proj_handle_free(struct VertProjHandle *vp_handle)
{
  if (vp_handle->use_update) {
    MEM_freeN(vp_handle->dists_sq);
  }

  MEM_freeN(vp_handle->vcosnos);
  MEM_freeN(vp_handle);
}
