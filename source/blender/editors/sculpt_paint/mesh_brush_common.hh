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
 * - positions_sculpt: The positions affected by brush strokes (maybe indirectly). Owned by the
 *   PBVH or mesh.
 * - positions_mesh: Positions owned by the original mesh. Not the same as `positions_sculpt` if
 *   there are deform modifiers.
 * - positions_eval: Positions after procedural deformation, used to build the PBVH. Translations
 *   are built for these values, then applied to `positions_sculpt`.
 *
 * Only two of these arrays are actually necessary. The third comes from the fact that the PBVH
 * currently stores its own copy of positions when there are deformations. If that was removed, the
 * situation would be clearer.
 *
 * \todo Get rid of one of the arrays mentioned above to avoid the situation with evaluated
 * positions, original positions, and then a third copy that's just there because of historical
 * reasons. This would involve removing access to positions and normals from the PBVH structure,
 * which should only be concerned with splitting geometry into spacially contiguous chunks.
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
 *
 * \todo Remove call to `undo::push_node` deep inside this function so the `object` argument can be
 * const. That may (hopefully) require pulling out the undo node push into the code for each brush.
 * That should help clarify the code path for brushes, and various optimizations will depend on
 * brush implementations doing their own undo pushes.
 */
void calc_vert_factors(Object &object,
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
 * Retrieve the final mutable positions array to be modified.
 *
 * \note See the comment at the top of this file for context.
 */
MutableSpan<float3> mesh_brush_positions_for_write(SculptSession &ss, Mesh &mesh);

/**
 * Applying final positions to shape keys is non-trivial because the mesh positions and the active
 * shape key positions must be kept in sync, and shape keys dependent on the active key must also
 * be modified.
 */
void flush_positions_to_shape_keys(Object &object,
                                   Span<int> verts,
                                   Span<float3> positions,
                                   MutableSpan<float3> positions_mesh);

}  // namespace blender::ed::sculpt_paint
