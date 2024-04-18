/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 *
 * The primary purpose of this API is to avoid unnecessary mesh conversion for the final
 * output of a modified mesh.
 *
 * This API handles the case when the modifier stack outputs a mesh which does not have
 * #Mesh data (#Mesh::faces(), corner verts, corner edges, edges, etc).
 * Currently this is used so the resulting mesh can have #BMEditMesh data,
 * postponing the converting until it's needed or avoiding conversion entirely
 * which can be an expensive operation.
 * Once converted, the meshes type changes to #ME_WRAPPER_TYPE_MDATA,
 * although the edit mesh is not cleared.
 *
 * This API exposes functions that abstract over the different kinds of internal data,
 * as well as supporting converting the mesh into regular mesh.
 */

#include "MEM_guardedalloc.h"

#include "DNA_modifier_types.h"
#include "DNA_object_types.h"

#include "BLI_ghash.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_math_vector.hh"
#include "BLI_task.hh"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "BKE_DerivedMesh.hh"
#include "BKE_editmesh.hh"
#include "BKE_editmesh_cache.hh"
#include "BKE_lib_id.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_runtime.hh"
#include "BKE_mesh_wrapper.hh"
#include "BKE_modifier.hh"
#include "BKE_object.hh"
#include "BKE_subdiv.hh"
#include "BKE_subdiv_mesh.hh"
#include "BKE_subdiv_modifier.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

using blender::float3;
using blender::Span;

Mesh *BKE_mesh_wrapper_from_editmesh(std::shared_ptr<BMEditMesh> em,
                                     const CustomData_MeshMasks *cd_mask_extra,
                                     const Mesh *me_settings)
{
  Mesh *mesh = static_cast<Mesh *>(BKE_id_new_nomain(ID_ME, nullptr));
  BKE_mesh_copy_parameters_for_eval(mesh, me_settings);
  BKE_mesh_runtime_ensure_edit_data(mesh);

  mesh->runtime->wrapper_type = ME_WRAPPER_TYPE_BMESH;
  if (cd_mask_extra) {
    mesh->runtime->cd_mask_extra = *cd_mask_extra;
  }

  /* Use edit-mesh directly where possible. */
  mesh->runtime->is_original_bmesh = true;

  mesh->runtime->edit_mesh = std::move(em);

  /* Make sure we crash if these are ever used. */
#ifndef NDEBUG
  mesh->verts_num = INT_MAX;
  mesh->edges_num = INT_MAX;
  mesh->faces_num = INT_MAX;
  mesh->corners_num = INT_MAX;
#else
  mesh->verts_num = 0;
  mesh->edges_num = 0;
  mesh->faces_num = 0;
  mesh->corners_num = 0;
#endif

  return mesh;
}

void BKE_mesh_wrapper_ensure_mdata(Mesh *mesh)
{
  std::lock_guard lock{mesh->runtime->eval_mutex};
  if (mesh->runtime->wrapper_type == ME_WRAPPER_TYPE_MDATA) {
    return;
  }

  /* Must isolate multithreaded tasks while holding a mutex lock. */
  blender::threading::isolate_task([&]() {
    switch (static_cast<eMeshWrapperType>(mesh->runtime->wrapper_type)) {
      case ME_WRAPPER_TYPE_MDATA:
      case ME_WRAPPER_TYPE_SUBD: {
        break; /* Quiet warning. */
      }
      case ME_WRAPPER_TYPE_BMESH: {
        mesh->verts_num = 0;
        mesh->edges_num = 0;
        mesh->faces_num = 0;
        mesh->corners_num = 0;

        BLI_assert(mesh->runtime->edit_mesh != nullptr);
        BLI_assert(mesh->runtime->edit_data != nullptr);

        BMEditMesh *em = mesh->runtime->edit_mesh.get();
        BM_mesh_bm_to_me_for_eval(*em->bm, *mesh, &mesh->runtime->cd_mask_extra);

        /* Adding original index layers here assumes that all BMesh Mesh wrappers are created from
         * original edit mode meshes (the only case where adding original indices makes sense).
         * If that assumption is broken, the layers might be incorrect because they might not
         * actually be "original".
         *
         * There is also a performance aspect, where this also assumes that original indices are
         * always needed when converting a BMesh to a mesh with the mesh wrapper system. That might
         * be wrong, but it's not harmful. */
        BKE_mesh_ensure_default_orig_index_customdata_no_check(mesh);

        blender::bke::EditMeshData &edit_data = *mesh->runtime->edit_data;
        if (!edit_data.vert_positions.is_empty()) {
          mesh->vert_positions_for_write().copy_from(edit_data.vert_positions);
          mesh->runtime->is_original_bmesh = false;
        }

        mesh->runtime->edit_data.reset();
        break;
      }
    }

    /* Keep type assignment last, so that read-only access only uses the mdata code paths after all
     * the underlying data has been initialized. */
    mesh->runtime->wrapper_type = ME_WRAPPER_TYPE_MDATA;
  });
}

/* -------------------------------------------------------------------- */
/** \name Mesh Coordinate Access
 * \{ */

Span<float3> BKE_mesh_wrapper_vert_coords(const Mesh *mesh)
{
  switch (mesh->runtime->wrapper_type) {
    case ME_WRAPPER_TYPE_BMESH:
      return mesh->runtime->edit_data->vert_positions;
    case ME_WRAPPER_TYPE_MDATA:
    case ME_WRAPPER_TYPE_SUBD:
      return mesh->vert_positions();
  }
  BLI_assert_unreachable();
  return {};
}

Span<float3> BKE_mesh_wrapper_face_normals(Mesh *mesh)
{
  switch (mesh->runtime->wrapper_type) {
    case ME_WRAPPER_TYPE_BMESH:
      return BKE_editmesh_cache_ensure_face_normals(*mesh->runtime->edit_mesh,
                                                    *mesh->runtime->edit_data);
    case ME_WRAPPER_TYPE_MDATA:
    case ME_WRAPPER_TYPE_SUBD:
      return mesh->face_normals();
  }
  BLI_assert_unreachable();
  return {};
}

void BKE_mesh_wrapper_tag_positions_changed(Mesh *mesh)
{
  switch (mesh->runtime->wrapper_type) {
    case ME_WRAPPER_TYPE_BMESH:
      if (blender::bke::EditMeshData *edit_data = mesh->runtime->edit_data.get()) {
        edit_data->vert_normals = {};
        edit_data->face_centers = {};
        edit_data->face_normals = {};
      }
      break;
    case ME_WRAPPER_TYPE_MDATA:
    case ME_WRAPPER_TYPE_SUBD:
      mesh->tag_positions_changed();
      break;
  }
}

void BKE_mesh_wrapper_vert_coords_copy(const Mesh *mesh, blender::MutableSpan<float3> positions)
{
  switch (mesh->runtime->wrapper_type) {
    case ME_WRAPPER_TYPE_BMESH: {
      BMesh *bm = mesh->runtime->edit_mesh->bm;
      const blender::bke::EditMeshData &edit_data = *mesh->runtime->edit_data;
      if (!edit_data.vert_positions.is_empty()) {
        positions.copy_from(edit_data.vert_positions);
      }
      else {
        BMIter iter;
        BMVert *v;
        int i;
        BM_ITER_MESH_INDEX (v, &iter, bm, BM_VERTS_OF_MESH, i) {
          copy_v3_v3(positions[i], v->co);
        }
      }
      return;
    }
    case ME_WRAPPER_TYPE_MDATA:
    case ME_WRAPPER_TYPE_SUBD: {
      positions.copy_from(mesh->vert_positions());
      return;
    }
  }
  BLI_assert_unreachable();
}

void BKE_mesh_wrapper_vert_coords_copy_with_mat4(const Mesh *mesh,
                                                 float (*vert_coords)[3],
                                                 int vert_coords_len,
                                                 const float mat[4][4])
{
  switch (mesh->runtime->wrapper_type) {
    case ME_WRAPPER_TYPE_BMESH: {
      BMesh *bm = mesh->runtime->edit_mesh->bm;
      BLI_assert(vert_coords_len == bm->totvert);
      const blender::bke::EditMeshData &edit_data = *mesh->runtime->edit_data;
      if (!edit_data.vert_positions.is_empty()) {
        for (int i = 0; i < vert_coords_len; i++) {
          mul_v3_m4v3(vert_coords[i], mat, edit_data.vert_positions[i]);
        }
      }
      else {
        BMIter iter;
        BMVert *v;
        int i;
        BM_ITER_MESH_INDEX (v, &iter, bm, BM_VERTS_OF_MESH, i) {
          mul_v3_m4v3(vert_coords[i], mat, v->co);
        }
      }
      return;
    }
    case ME_WRAPPER_TYPE_MDATA:
    case ME_WRAPPER_TYPE_SUBD: {
      BLI_assert(vert_coords_len == mesh->verts_num);
      const Span<float3> positions = mesh->vert_positions();
      for (int i = 0; i < vert_coords_len; i++) {
        mul_v3_m4v3(vert_coords[i], mat, positions[i]);
      }
      return;
    }
  }
  BLI_assert_unreachable();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mesh Array Length Access
 * \{ */

int BKE_mesh_wrapper_vert_len(const Mesh *mesh)
{
  switch (mesh->runtime->wrapper_type) {
    case ME_WRAPPER_TYPE_BMESH:
      return mesh->runtime->edit_mesh->bm->totvert;
    case ME_WRAPPER_TYPE_MDATA:
    case ME_WRAPPER_TYPE_SUBD:
      return mesh->verts_num;
  }
  BLI_assert_unreachable();
  return -1;
}

int BKE_mesh_wrapper_edge_len(const Mesh *mesh)
{
  switch (mesh->runtime->wrapper_type) {
    case ME_WRAPPER_TYPE_BMESH:
      return mesh->runtime->edit_mesh->bm->totedge;
    case ME_WRAPPER_TYPE_MDATA:
    case ME_WRAPPER_TYPE_SUBD:
      return mesh->edges_num;
  }
  BLI_assert_unreachable();
  return -1;
}

int BKE_mesh_wrapper_loop_len(const Mesh *mesh)
{
  switch (mesh->runtime->wrapper_type) {
    case ME_WRAPPER_TYPE_BMESH:
      return mesh->runtime->edit_mesh->bm->totloop;
    case ME_WRAPPER_TYPE_MDATA:
    case ME_WRAPPER_TYPE_SUBD:
      return mesh->corners_num;
  }
  BLI_assert_unreachable();
  return -1;
}

int BKE_mesh_wrapper_face_len(const Mesh *mesh)
{
  switch (mesh->runtime->wrapper_type) {
    case ME_WRAPPER_TYPE_BMESH:
      return mesh->runtime->edit_mesh->bm->totface;
    case ME_WRAPPER_TYPE_MDATA:
    case ME_WRAPPER_TYPE_SUBD:
      return mesh->faces_num;
  }
  BLI_assert_unreachable();
  return -1;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name CPU Subdivision Evaluation
 * \{ */

static Mesh *mesh_wrapper_ensure_subdivision(Mesh *mesh)
{
  SubsurfRuntimeData *runtime_data = (SubsurfRuntimeData *)mesh->runtime->subsurf_runtime_data;
  if (runtime_data == nullptr || runtime_data->settings.level == 0) {
    return mesh;
  }

  /* Initialize the settings before ensuring the descriptor as this is checked to decide whether
   * subdivision is needed at all, and checking the descriptor status might involve checking if the
   * data is out-of-date, which is a very expensive operation. */
  SubdivToMeshSettings mesh_settings;
  mesh_settings.resolution = runtime_data->resolution;
  mesh_settings.use_optimal_display = runtime_data->use_optimal_display;

  if (mesh_settings.resolution < 3) {
    return mesh;
  }

  Subdiv *subdiv = BKE_subsurf_modifier_subdiv_descriptor_ensure(runtime_data, mesh, false);
  if (subdiv == nullptr) {
    /* Happens on bad topology, but also on empty input mesh. */
    return mesh;
  }
  const bool use_clnors = runtime_data->use_loop_normals;
  if (use_clnors) {
    /* If custom normals are present and the option is turned on calculate the split
     * normals and clear flag so the normals get interpolated to the result mesh. */
    void *data = CustomData_add_layer(
        &mesh->corner_data, CD_NORMAL, CD_CONSTRUCT, mesh->corners_num);
    memcpy(data, mesh->corner_normals().data(), mesh->corner_normals().size_in_bytes());
  }

  Mesh *subdiv_mesh = BKE_subdiv_to_mesh(subdiv, &mesh_settings, mesh);

  if (use_clnors) {
    BKE_mesh_set_custom_normals(subdiv_mesh,
                                static_cast<float(*)[3]>(CustomData_get_layer_for_write(
                                    &subdiv_mesh->corner_data, CD_NORMAL, mesh->corners_num)));
    CustomData_free_layers(&subdiv_mesh->corner_data, CD_NORMAL, mesh->corners_num);
  }

  if (!ELEM(subdiv, runtime_data->subdiv_cpu, runtime_data->subdiv_gpu)) {
    BKE_subdiv_free(subdiv);
  }

  if (subdiv_mesh != mesh) {
    if (mesh->runtime->mesh_eval != nullptr) {
      BKE_id_free(nullptr, mesh->runtime->mesh_eval);
    }
    mesh->runtime->mesh_eval = subdiv_mesh;
    mesh->runtime->wrapper_type = ME_WRAPPER_TYPE_SUBD;
  }

  return mesh->runtime->mesh_eval;
}

Mesh *BKE_mesh_wrapper_ensure_subdivision(Mesh *mesh)
{
  std::lock_guard lock{mesh->runtime->eval_mutex};

  if (mesh->runtime->wrapper_type == ME_WRAPPER_TYPE_SUBD) {
    return mesh->runtime->mesh_eval;
  }

  Mesh *result;

  /* Must isolate multithreaded tasks while holding a mutex lock. */
  blender::threading::isolate_task([&]() { result = mesh_wrapper_ensure_subdivision(mesh); });

  return result;
}

/** \} */
