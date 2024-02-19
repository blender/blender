/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <optional>

#include "BLI_array.hh"
#include "BLI_bounds_types.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_math_vector_types.hh"

#include "DNA_customdata_types.h" /* #CustomData_MeshMasks. */

struct BoundBox;
struct bGPdata;
struct Curve;
struct CurveCache;
struct ID;
struct Mesh;
struct PoseBackup;

namespace blender::bke {

struct GeometrySet;

struct ObjectRuntime {
  /** Final transformation matrices with constraints & animsys applied. */
  float4x4 object_to_world;
  float4x4 world_to_object;
  /**
   * The custom data layer mask that was last used
   * to calculate data_eval and mesh_deform_eval.
   */
  CustomData_MeshMasks last_data_mask = {};

  /** Did last modifier stack generation need mapping support? */
  char last_need_mapping = false;

  /** Only used for drawing the parent/child help-line. */
  float3 parent_display_origin;

  /**
   * Selection id of this object. It might differ between an evaluated and its original object,
   * when the object is being instanced.
   */
  int select_id = -1;

  /**
   * Denotes whether the evaluated data is owned by this object or is referenced and owned by
   * somebody else.
   */
  char is_data_eval_owned = false;

  /** Start time of the mode transfer overlay animation. */
  double overlay_mode_transfer_start_time = 0.0f;

  /**
   * The bounding box of the object's evaluated geometry in the active dependency graph. The bounds
   * are copied back to the original object for the RNA API and for display in the interface.
   *
   * Only set on original objects.
   */
  std::optional<Bounds<float3>> bounds_eval;

  /**
   * Original data pointer, before object->data was changed to point
   * to data_eval.
   * Is assigned by dependency graph's copy-on-write evaluation.
   */
  ID *data_orig = nullptr;
  /**
   * Object data structure created during object evaluation. It has all modifiers applied.
   * The type is determined by the type of the original object.
   */
  ID *data_eval = nullptr;

  /**
   * Objects can evaluate to a geometry set instead of a single ID. In those cases, the evaluated
   * geometry set will be stored here. An ID of the correct type is still stored in #data_eval.
   * #geometry_set_eval might reference the ID pointed to by #data_eval as well, but does not own
   * the data.
   */
  GeometrySet *geometry_set_eval = nullptr;

  /**
   * Mesh structure created during object evaluation.
   * It has deformation only modifiers applied on it.
   */
  Mesh *mesh_deform_eval = nullptr;

  /**
   * Evaluated mesh cage in edit mode.
   *
   * \note When the mesh's `runtime->deformed_only` is true, the meshes vertex positions
   * and other geometry arrays will be aligned the edit-mesh. Otherwise the #CD_ORIGINDEX
   * custom-data should be used to map the cage geometry back to the original indices, see
   * #eModifierTypeFlag_SupportsMapping.
   */
  Mesh *editmesh_eval_cage = nullptr;

  /**
   * Original grease pencil bGPdata pointer, before object->data was changed to point
   * to gpd_eval.
   * Is assigned by dependency graph's copy-on-write evaluation.
   */
  bGPdata *gpd_orig = nullptr;
  /**
   * bGPdata structure created during object evaluation.
   * It has all modifiers applied.
   */
  bGPdata *gpd_eval = nullptr;

  /**
   * This is a mesh representation of corresponding object.
   * It created when Python calls `object.to_mesh()`.
   */
  Mesh *object_as_temp_mesh = nullptr;

  /**
   * Backup of the object's pose (might be a subset, i.e. not contain all bones).
   *
   * Created by `BKE_pose_backup_create_on_object()`. This memory is owned by the Object.
   * It is freed along with the object, or when `BKE_pose_backup_clear()` is called.
   */
  PoseBackup *pose_backup = nullptr;

  /**
   * This is a curve representation of corresponding object.
   * It created when Python calls `object.to_curve()`.
   */
  ::Curve *object_as_temp_curve = nullptr;

  /** Runtime evaluated curve-specific data, not stored in the file. */
  CurveCache *curve_cache = nullptr;

  unsigned short local_collections_bits = 0;

  Array<float3x3, 0> crazyspace_deform_imats;
  Array<float3, 0> crazyspace_deform_cos;

  /* The Depsgraph::update_count when this object was last updated. */
  uint64_t last_update_transform = 0;
  uint64_t last_update_geometry = 0;
  uint64_t last_update_shading = 0;
};

}  // namespace blender::bke
