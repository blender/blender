/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_math_matrix_types.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_span.hh"

#include "DNA_brush_enums.h"

/**
 * This file contains common operations useful for the implementation of various different brush
 * tools. The design goals of the API are to always operate on more than one data element at a
 * time, to avoid unnecessary branching for constants, favor cache-friendly access patterns, enable
 * use of SIMD, and provide opportunities to avoid work where possible.
 *
 * API function arguments should favor passing raw data references rather than general catch-all
 * storage structs in order to clarify the scope of each function, structure the work around the
 * required data, and limit redundant data storage.
 *
 * Many functions calculate "factors" which describe how strong the brush influence should be
 * between 0 and 1. Most functions multiply with the existing factor value rather than assigning a
 * new value from scratch.
 */

struct Brush;
struct Mesh;
struct Object;
struct PBVHNode;
struct Sculpt;
struct SculptSession;

namespace blender::ed::sculpt_paint {

namespace auto_mask {
struct Cache;
};

/**
 * Note on the various positions arrays:
 * - positions_orig: Positions owned by the original mesh. Not the same as `positions_eval` if
 *   there are deform modifiers.
 * - positions_eval: Positions after procedural deformation, used to build the PBVH. Translations
 *   are built for these values, then applied to `positions_orig`.
 */

/**
 * Calculate initial influence factors based on vertex visibility and masking.
 */
void fill_factor_from_hide_and_mask(const Mesh &mesh,
                                    Span<int> vert_indices,
                                    MutableSpan<float> r_factors);

/**
 * Disable brush influence when vertex normals point away from the view.
 */
void calc_front_face(const float3 &view_normal,
                     Span<float3> vert_normals,
                     Span<int> vert_indices,
                     MutableSpan<float> factors);

/**
 * Modify influence factors based on the distance from the brush cursor and various other settings.
 * Also fill an array of distances from the brush cursor for "in bounds" vertices.
 */
void calc_distance_falloff(SculptSession &ss,
                           Span<float3> vert_positions,
                           Span<int> vert_indices,
                           eBrushFalloffShape falloff_shape,
                           MutableSpan<float> r_distances,
                           MutableSpan<float> factors);

/**
 * Modify the factors based on distances to the brush cursor, using various brush settings.
 */
void calc_brush_strength_factors(const SculptSession &ss,
                                 const Brush &brush,
                                 Span<int> vert_indices,
                                 Span<float> distances,
                                 MutableSpan<float> factors);

/**
 * Modify brush influence factors to include sampled texture values.
 */
void calc_brush_texture_factors(SculptSession &ss,
                                const Brush &brush,
                                Span<float3> vert_positions,
                                Span<int> vert_indices,
                                MutableSpan<float> factors);

namespace auto_mask {

/**
 * Calculate all auto-masking influence on each vertex.
 */
void calc_vert_factors(const Object &object,
                       const Cache &cache,
                       const PBVHNode &node,
                       Span<int> verts,
                       MutableSpan<float> factors);

}  // namespace auto_mask

/**
 * Many brushes end up calculating translations from the original positions. Instead of applying
 * these directly to the modified values, it's helpful to process them separately to easily
 * calculate various effects like clipping. After they are processed, this function can be used to
 * simply add them to the final vertex positions.
 */
void apply_translations(Span<float3> translations, Span<int> verts, MutableSpan<float3> positions);

/**
 * Rotate translations to account for rotations from procedural deformation.
 *
 * \todo Don't invert `deform_imats` on object evaluation. Instead just invert them on-demand in
 * brush implementations. This would be better because only the inversions required for affected
 * vertices would be necessary.
 */
void apply_crazyspace_to_translations(Span<float3x3> deform_imats,
                                      Span<int> verts,
                                      MutableSpan<float3> translations);

/**
 * Modify translations based on sculpt mode axis locking and mirroring clipping.
 */
void clip_and_lock_translations(const Sculpt &sd,
                                const SculptSession &ss,
                                Span<float3> positions,
                                Span<int> verts,
                                MutableSpan<float3> translations);

/**
 * Applying final positions to shape keys is non-trivial because the mesh positions and the active
 * shape key positions must be kept in sync, and shape keys dependent on the active key must also
 * be modified.
 */
void apply_translations_to_shape_keys(Object &object,
                                      Span<int> verts,
                                      Span<float3> translations,
                                      MutableSpan<float3> positions_mesh);

/**
 * Currently the PBVH owns its own copy of deformed positions that needs to be updated to stay in
 * sync with brush deformations.
 * \todo This should be removed one the PBVH no longer stores this copy of deformed positions.
 */
void apply_translations_to_pbvh(PBVH &pbvh, Span<int> verts, Span<float3> positions_orig);

}  // namespace blender::ed::sculpt_paint
