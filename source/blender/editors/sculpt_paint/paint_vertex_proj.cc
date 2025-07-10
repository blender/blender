/* SPDX-FileCopyrightText: 2013 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 *
 * Utility functions for getting vertex locations while painting
 * (since they may be instanced multiple times in an evaluated mesh)
 */

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_object_types.h"

#include "BKE_mesh_iterators.hh"
#include "BKE_object.hh"

#include "BLI_listbase.h"
#include "BLI_math_vector.h"

#include "DEG_depsgraph_query.hh"

#include "ED_view3d.hh"

#include "paint_intern.hh" /* own include */

/* Opaque Structs for internal use */

/* stored while painting */
struct VertProjHandle {
  blender::Array<blender::float3> vert_positions;
  blender::Array<blender::float3> vert_normals;

  bool use_update;

  /* use for update */
  blender::Array<float> dists_sq;

  Object *ob;
  Scene *scene;
};

/* only for passing to the callbacks */
struct VertProjUpdate {
  VertProjHandle *vp_handle;

  /* runtime */
  ARegion *region;
  const float *mval_fl;
};

/* -------------------------------------------------------------------- */
/* Internal Init */

static void vpaint_proj_dm_map_cosnos_init__map_cb(void *user_data,
                                                   int index,
                                                   const float co[3],
                                                   const float no[3])
{
  VertProjHandle *vp_handle = static_cast<VertProjHandle *>(user_data);

  /* check if we've been here before (normal should not be 0) */
  if (!blender::math::is_zero(vp_handle->vert_normals[index])) {
    /* remember that multiple dm verts share the same source vert */
    vp_handle->use_update = true;
    return;
  }

  vp_handle->vert_positions[index] = co;
  vp_handle->vert_normals[index] = no;
}

static void vpaint_proj_dm_map_cosnos_init(Depsgraph &depsgraph,
                                           Scene & /*scene*/,
                                           Object &ob,
                                           VertProjHandle &vp_handle)
{
  const Object *ob_eval = DEG_get_evaluated(&depsgraph, &ob);
  const Mesh *mesh_eval = BKE_object_get_evaluated_mesh(ob_eval);

  vp_handle.vert_normals.fill(blender::float3(0));
  BKE_mesh_foreach_mapped_vert(
      mesh_eval, vpaint_proj_dm_map_cosnos_init__map_cb, &vp_handle, MESH_FOREACH_USE_NORMAL);
}

/* -------------------------------------------------------------------- */
/* Internal Update */

/* Same as init but take mouse location into account */

static void vpaint_proj_dm_map_cosnos_update__map_cb(void *user_data,
                                                     int index,
                                                     const float co[3],
                                                     const float no[3])
{
  VertProjUpdate *vp_update = static_cast<VertProjUpdate *>(user_data);
  VertProjHandle *vp_handle = vp_update->vp_handle;

  /* find closest vertex */
  {
    /* first find distance to this vertex */
    float co_ss[2]; /* screenspace */

    if (ED_view3d_project_float_object(
            vp_update->region, co, co_ss, V3D_PROJ_TEST_CLIP_BB | V3D_PROJ_TEST_CLIP_NEAR) ==
        V3D_PROJ_RET_OK)
    {
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

  vp_handle->vert_positions[index] = co;
  vp_handle->vert_normals[index] = no;
}

static void vpaint_proj_dm_map_cosnos_update(Depsgraph *depsgraph,
                                             VertProjHandle *vp_handle,
                                             ARegion *region,
                                             const float mval_fl[2])
{
  VertProjUpdate vp_update = {vp_handle, region, mval_fl};

  Object &ob = *vp_handle->ob;

  const Object *ob_eval = DEG_get_evaluated(depsgraph, &ob);
  const Mesh *mesh_eval = BKE_object_get_evaluated_mesh(ob_eval);

  /* quick sanity check - we shouldn't have to run this if there are no modifiers */
  BLI_assert(BLI_listbase_is_empty(&ob.modifiers) == false);

  vp_handle->dists_sq.fill(FLT_MAX);
  BKE_mesh_foreach_mapped_vert(
      mesh_eval, vpaint_proj_dm_map_cosnos_update__map_cb, &vp_update, MESH_FOREACH_USE_NORMAL);
}

/* -------------------------------------------------------------------- */
/* Public Functions */

VertProjHandle *ED_vpaint_proj_handle_create(Depsgraph &depsgraph,
                                             Scene &scene,
                                             Object &ob,
                                             blender::Span<blender::float3> &r_vert_positions,
                                             blender::Span<blender::float3> &r_vert_normals)
{
  VertProjHandle *vp_handle = MEM_new<VertProjHandle>(__func__);
  Mesh *mesh = static_cast<Mesh *>(ob.data);

  /* setup the handle */
  vp_handle->vert_positions.reinitialize(mesh->verts_num);
  vp_handle->vert_normals.reinitialize(mesh->verts_num);
  vp_handle->use_update = false;

  /* sets 'use_update' if needed */
  vpaint_proj_dm_map_cosnos_init(depsgraph, scene, ob, *vp_handle);

  if (vp_handle->use_update) {
    vp_handle->dists_sq.reinitialize(mesh->verts_num);
    vp_handle->ob = &ob;
    vp_handle->scene = &scene;
  }
  else {
    vp_handle->ob = nullptr;
    vp_handle->scene = nullptr;
  }

  r_vert_positions = vp_handle->vert_positions;
  r_vert_normals = vp_handle->vert_normals;
  return vp_handle;
}

void ED_vpaint_proj_handle_update(Depsgraph *depsgraph,
                                  VertProjHandle *vp_handle,
                                  ARegion *region,
                                  const float mval_fl[2])
{
  if (vp_handle->use_update) {
    vpaint_proj_dm_map_cosnos_update(depsgraph, vp_handle, region, mval_fl);
  }
}

void ED_vpaint_proj_handle_free(VertProjHandle *vp_handle)
{
  MEM_delete(vp_handle);
}
