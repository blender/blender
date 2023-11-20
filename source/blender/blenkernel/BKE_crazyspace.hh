/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#pragma once

#include "BLI_array.hh"
#include "BLI_math_matrix.hh"
#include "BLI_span.hh"

struct BMEditMesh;
struct Depsgraph;
struct Mesh;
struct Object;
struct ReportList;
struct Scene;

namespace blender::bke::crazyspace {

/**
 * Contains information about how points have been deformed during evaluation.
 * This allows mapping edits on evaluated data back to original data in some cases.
 */
struct GeometryDeformation {
  /**
   * Positions of the deformed points. This may also point to the original position if no
   * deformation data is available.
   */
  Span<float3> positions;
  /**
   * Matrices that transform point translations on original data into corresponding translations in
   * evaluated data. This may be empty if not available.
   */
  Span<float3x3> deform_mats;

  float3 translation_from_deformed_to_original(const int position_i,
                                               const float3 &translation) const
  {
    if (this->deform_mats.is_empty()) {
      return translation;
    }
    const float3x3 &deform_mat = this->deform_mats[position_i];
    return math::transform_point(math::invert(deform_mat), translation);
  }
};

/**
 * During evaluation of the object, deformation data may have been generated for this object. This
 * function either retrieves the deformation data from the evaluated object, or falls back to
 * returning the original data.
 */
GeometryDeformation get_evaluated_curves_deformation(const Object *ob_eval, const Object &ob_orig);
GeometryDeformation get_evaluated_curves_deformation(const Depsgraph &depsgraph,
                                                     const Object &ob_orig);
GeometryDeformation get_evaluated_grease_pencil_drawing_deformation(const Object *ob_eval,
                                                                    const Object &ob_orig,
                                                                    int layer_index,
                                                                    int frame);
GeometryDeformation get_evaluated_grease_pencil_drawing_deformation(const Depsgraph &depsgraph,
                                                                    const Object &ob_orig,
                                                                    int layer_index,
                                                                    int frame);

}  // namespace blender::bke::crazyspace

/**
 * Disable subdivision-surface temporal, get mapped coordinates, and enable it.
 */
blender::Array<blender::float3> BKE_crazyspace_get_mapped_editverts(Depsgraph *depsgraph,
                                                                    Object *obedit);
void BKE_crazyspace_set_quats_editmesh(BMEditMesh *em,
                                       blender::Span<blender::float3> origcos,
                                       blender::Span<blender::float3> mappedcos,
                                       float (*quats)[4],
                                       bool use_select);
void BKE_crazyspace_set_quats_mesh(Mesh *me,
                                   blender::Span<blender::float3> origcos,
                                   blender::Span<blender::float3> mappedcos,
                                   float (*quats)[4]);
/**
 * Returns an array of deform matrices for crazy-space correction,
 * and the number of modifiers left.
 */
int BKE_crazyspace_get_first_deform_matrices_editbmesh(
    Depsgraph *depsgraph,
    Scene *,
    Object *,
    BMEditMesh *em,
    blender::Array<blender::float3x3, 0> &deformmats,
    blender::Array<blender::float3, 0> &deformcos);
int BKE_sculpt_get_first_deform_matrices(Depsgraph *depsgraph,
                                         Scene *scene,
                                         Object *ob,
                                         blender::Array<blender::float3x3, 0> &deformmats,
                                         blender::Array<blender::float3, 0> &deformcos);
void BKE_crazyspace_build_sculpt(Depsgraph *depsgraph,
                                 Scene *scene,
                                 Object *ob,
                                 blender::Array<blender::float3x3, 0> &deformmats,
                                 blender::Array<blender::float3, 0> &deformcos);

/* -------------------------------------------------------------------- */
/** \name Crazy-Space API
 * \{ */

void BKE_crazyspace_api_eval(Depsgraph *depsgraph,
                             Scene *scene,
                             Object *object,
                             ReportList *reports);

void BKE_crazyspace_api_displacement_to_deformed(Object *object,
                                                 ReportList *reports,
                                                 int vertex_index,
                                                 const float displacement[3],
                                                 float r_displacement_deformed[3]);

void BKE_crazyspace_api_displacement_to_original(Object *object,
                                                 ReportList *reports,
                                                 int vertex_index,
                                                 const float displacement_deformed[3],
                                                 float r_displacement[3]);

void BKE_crazyspace_api_eval_clear(Object *object);

/** \} */