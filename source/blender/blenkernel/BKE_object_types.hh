/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

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
  /**
   * The custom data layer mask that was last used
   * to calculate data_eval and mesh_deform_eval.
   */
  CustomData_MeshMasks last_data_mask;

  /** Did last modifier stack generation need mapping support? */
  char last_need_mapping;

  /** Only used for drawing the parent/child help-line. */
  float parent_display_origin[3];

  /**
   * Selection id of this object. It might differ between an evaluated and its original object,
   * when the object is being instanced.
   */
  int select_id;

  /**
   * Denotes whether the evaluated data is owned by this object or is referenced and owned by
   * somebody else.
   */
  char is_data_eval_owned;

  /** Start time of the mode transfer overlay animation. */
  double overlay_mode_transfer_start_time;

  /** Axis aligned bound-box (in local-space). */
  BoundBox *bb;

  /**
   * Original data pointer, before object->data was changed to point
   * to data_eval.
   * Is assigned by dependency graph's copy-on-write evaluation.
   */
  ID *data_orig;
  /**
   * Object data structure created during object evaluation. It has all modifiers applied.
   * The type is determined by the type of the original object.
   */
  ID *data_eval;

  /**
   * Objects can evaluate to a geometry set instead of a single ID. In those cases, the evaluated
   * geometry set will be stored here. An ID of the correct type is still stored in #data_eval.
   * #geometry_set_eval might reference the ID pointed to by #data_eval as well, but does not own
   * the data.
   */
  GeometrySet *geometry_set_eval;

  /**
   * Mesh structure created during object evaluation.
   * It has deformation only modifiers applied on it.
   */
  Mesh *mesh_deform_eval;

  /* Evaluated mesh cage in edit mode. */
  Mesh *editmesh_eval_cage;

  /** Cached cage bounding box of `editmesh_eval_cage` for selection. */
  BoundBox *editmesh_bb_cage;

  /**
   * Original grease pencil bGPdata pointer, before object->data was changed to point
   * to gpd_eval.
   * Is assigned by dependency graph's copy-on-write evaluation.
   */
  bGPdata *gpd_orig;
  /**
   * bGPdata structure created during object evaluation.
   * It has all modifiers applied.
   */
  bGPdata *gpd_eval;

  /**
   * This is a mesh representation of corresponding object.
   * It created when Python calls `object.to_mesh()`.
   */
  Mesh *object_as_temp_mesh;

  /**
   * Backup of the object's pose (might be a subset, i.e. not contain all bones).
   *
   * Created by `BKE_pose_backup_create_on_object()`. This memory is owned by the Object.
   * It is freed along with the object, or when `BKE_pose_backup_clear()` is called.
   */
  PoseBackup *pose_backup;

  /**
   * This is a curve representation of corresponding object.
   * It created when Python calls `object.to_curve()`.
   */
  ::Curve *object_as_temp_curve;

  /** Runtime evaluated curve-specific data, not stored in the file. */
  CurveCache *curve_cache;

  unsigned short local_collections_bits;

  float (*crazyspace_deform_imats)[3][3];
  float (*crazyspace_deform_cos)[3];
  int crazyspace_verts_num;
};

}  // namespace blender::bke