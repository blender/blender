/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "BCAnimationCurve.h"

#include "DNA_action_types.h"
#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_camera_types.h"
#include "DNA_constraint_types.h"
#include "DNA_curve_types.h"
#include "DNA_light_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BIK_api.h"
#include "BKE_action.h" /* pose functions */
#include "BKE_armature.hh"
#include "BKE_constraint.h"
#include "BKE_fcurve.h"
#include "BKE_object.hh"
#include "BKE_scene.h"
#include "ED_object.hh"

#include "MEM_guardedalloc.h"

#include "RNA_access.hh"

#include "COLLADASWBaseInputElement.h"
#include "COLLADASWConstants.h"
#include "COLLADASWInputList.h"
#include "COLLADASWInstanceGeometry.h"
#include "COLLADASWLibraryAnimations.h"
#include "COLLADASWParamBase.h"
#include "COLLADASWParamTemplate.h"
#include "COLLADASWPrimitves.h"
#include "COLLADASWSampler.h"
#include "COLLADASWSource.h"
#include "COLLADASWVertices.h"

#include "BCAnimationSampler.h"
#include "EffectExporter.h"
#include "collada_internal.h"

#include "IK_solver.h"

#include <algorithm> /* std::find */
#include <map>
#include <vector>

typedef enum BC_animation_source_type {
  BC_SOURCE_TYPE_VALUE,
  BC_SOURCE_TYPE_ANGLE,
  BC_SOURCE_TYPE_TIMEFRAME,
} BC_animation_source_type;

typedef enum BC_global_rotation_type {
  BC_NO_ROTATION,
  BC_OBJECT_ROTATION,
  BC_DATA_ROTATION
} BC_global_rotation_type;

class AnimationExporter : COLLADASW::LibraryAnimations {
 private:
  COLLADASW::StreamWriter *sw;
  BCExportSettings &export_settings;

  BC_global_rotation_type get_global_rotation_type(Object *ob);

 public:
  AnimationExporter(COLLADASW::StreamWriter *sw, BCExportSettings &export_settings)
      : COLLADASW::LibraryAnimations(sw), sw(sw), export_settings(export_settings)
  {
  }

  bool exportAnimations();

  /** Called for each exported object. */
  void operator()(Object *ob);

 protected:
  void export_object_constraint_animation(Object *ob);

  void export_morph_animation(Object *ob);

  void write_bone_animation_matrix(Object *ob_arm, Bone *bone);

  void write_bone_animation(Object *ob_arm, Bone *bone);

  void sample_and_write_bone_animation(Object *ob_arm, Bone *bone, int transform_type);

  void sample_and_write_bone_animation_matrix(Object *ob_arm, Bone *bone);

  void sample_animation(float *v,
                        std::vector<float> &frames,
                        int type,
                        Bone *bone,
                        Object *ob_arm,
                        bPoseChannel *pChan);

  void sample_animation(std::vector<float[4][4]> &mats,
                        std::vector<float> &frames,
                        Bone *bone,
                        Object *ob_arm,
                        bPoseChannel *pChan);

  /* dae_bone_animation -> add_bone_animation
   * (blend this into dae_bone_animation) */
  void dae_bone_animation(std::vector<float> &fra,
                          float *v,
                          int tm_type,
                          int axis,
                          std::string ob_name,
                          std::string bone_name);

  void dae_baked_animation(std::vector<float> &fra, Object *ob_arm, Bone *bone);

  void dae_baked_object_animation(std::vector<float> &fra, Object *ob);

  float convert_time(float frame);

  float convert_angle(float angle);

  std::vector<std::vector<std::string>> anim_meta;

  /** Main entry point into Animation export (called for each exported object). */
  void exportAnimation(Object *ob, BCAnimationSampler &sampler);

  /**
   * Export all animation FCurves of an Object.
   *
   * \note This uses the keyframes as sample points,
   * and exports "baked keyframes" while keeping the tangent information
   * of the FCurves intact. This works for simple cases, but breaks
   * especially when negative scales are involved in the animation.
   * And when parent inverse matrices are involved (when exporting
   * object hierarchies)
   */
  void export_curve_animation_set(Object *ob, BCAnimationSampler &sampler, bool export_as_matrix);

  /** Export one single curve. */
  void export_curve_animation(Object *ob, BCAnimationCurve &curve);

  /** Export animation as matrix data. */
  void export_matrix_animation(Object *ob, BCAnimationSampler &sampler);

  /** Write bone animations in transform matrix sources (step through the bone hierarchy). */
  void export_bone_animations_recursive(Object *ob_arm, Bone *bone, BCAnimationSampler &sampler);

  /** Export for one bone. */
  void export_bone_animation(Object *ob, Bone *bone, BCFrames &frames, BCMatrixSampleMap &samples);

  /** Call to the low level collada exporter. */
  void export_collada_curve_animation(std::string id,
                                      std::string name,
                                      std::string target,
                                      std::string axis,
                                      BCAnimationCurve &curve,
                                      BC_global_rotation_type global_rotation_type);

  /** Call to the low level collada exporter. */
  void export_collada_matrix_animation(std::string id,
                                       std::string name,
                                       std::string target,
                                       BCFrames &frames,
                                       BCMatrixSampleMap &samples,
                                       BC_global_rotation_type global_rotation_type,
                                       Matrix &parentinv);

  /**
   * In some special cases the exported Curve needs to be replaced
   * by a modified curve (for collada purposes)
   * This method checks if a conversion is necessary and if applicable
   * returns a pointer to the modified BCAnimationCurve.
   * IMPORTANT: the modified curve must be deleted by the caller when no longer needed
   * if no conversion is needed this method returns a NULL;
   */
  BCAnimationCurve *get_modified_export_curve(Object *ob,
                                              BCAnimationCurve &curve,
                                              BCAnimationCurveMap &curves);

  /* Helper functions. */

  void openAnimationWithClip(std::string id, std::string name);
  bool open_animation_container(bool has_container, Object *ob);
  void close_animation_container(bool has_container);

  /** Input and Output sources (single valued). */
  std::string collada_source_from_values(BC_animation_source_type source_type,
                                         COLLADASW::InputSemantic::Semantics semantic,
                                         std::vector<float> &values,
                                         const std::string &anim_id,
                                         const std::string axis_name);

  /** Output sources (matrix data). * Create a collada matrix source for a set of samples. */
  std::string collada_source_from_values(BCMatrixSampleMap &samples,
                                         const std::string &anim_id,
                                         BC_global_rotation_type global_rotation_type,
                                         Matrix &parentinv);

  /** Interpolation sources. */
  std::string collada_linear_interpolation_source(int tot, const std::string &anim_id);

  /* source ID = animation_name + semantic_suffix */

  std::string get_semantic_suffix(COLLADASW::InputSemantic::Semantics semantic);

  void add_source_parameters(COLLADASW::SourceBase::ParameterNameList &param,
                             COLLADASW::InputSemantic::Semantics semantic,
                             bool is_rot,
                             const std::string axis,
                             bool transform);

  int get_point_in_curve(BCBezTriple &bezt,
                         COLLADASW::InputSemantic::Semantics semantic,
                         bool is_angle,
                         float *values);
  int get_point_in_curve(const BCAnimationCurve &curve,
                         float sample_frame,
                         COLLADASW::InputSemantic::Semantics semantic,
                         bool is_angle,
                         float *values);

  std::string collada_tangent_from_curve(COLLADASW::InputSemantic::Semantics semantic,
                                         BCAnimationCurve &curve,
                                         const std::string &anim_id,
                                         const std::string axis_name);

  std::string collada_interpolation_source(const BCAnimationCurve &curve,
                                           const std::string &anim_id,
                                           std::string axis_name,
                                           bool *has_tangents);

  std::string get_axis_name(std::string channel, int id);
  std::string get_collada_name(std::string channel_type) const;
  /**
   * Assign sid of the animated parameter or transform for rotation,
   * axis name is always appended and the value of append_axis is ignored.
   */
  std::string get_collada_sid(const BCAnimationCurve &curve, const std::string axis_name);

  /* ===================================== */
  /* Currently unused or not (yet?) needed */
  /* ===================================== */

  bool is_bone_deform_group(Bone *bone);

#if 0
  BC_animation_transform_type _get_transform_type(const std::string path);
  void get_eul_source_for_quat(std::vector<float> &cache, Object *ob);
#endif

#ifdef WITH_MORPH_ANIMATION
  void export_morph_animation(Object *ob, BCAnimationSampler &sampler);
#endif
};
