/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_array.hh"
#include "BLI_bit_span.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_offset_indices.hh"
#include "BLI_set.hh"
#include "BLI_span.hh"
#include "BLI_vector.hh"

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

struct BMVert;
struct Brush;
struct Mesh;
struct Object;
struct PBVH;
struct PBVHNode;
struct Sculpt;
struct SculptSession;

namespace blender::ed::sculpt_paint {
struct StrokeCache;

namespace auto_mask {
struct Cache;
};

void scale_translations(MutableSpan<float3> translations, Span<float> factors);
void scale_translations(MutableSpan<float3> translations, float factor);
void scale_factors(MutableSpan<float> factors, float strength);

/**
 * Note on the various positions arrays:
 * - positions_orig: Positions owned by the original mesh. Not the same as `positions_eval` if
 *   there are deform modifiers.
 * - positions_eval: Positions after procedural deformation, used to build the PBVH. Translations
 *   are built for these values, then applied to `positions_orig`.
 */

/** Fill the output array with all positions in the grids referenced by the indices. */
void gather_grids_positions(const SubdivCCG &subdiv_ccg,
                            Span<int> grids,
                            MutableSpan<float3> positions);

void gather_bmesh_positions(const Set<BMVert *, 0> &verts, MutableSpan<float3> positions);

/**
 * Calculate initial influence factors based on vertex visibility.
 */
void fill_factor_from_hide(const Mesh &mesh, Span<int> vert_indices, MutableSpan<float> r_factors);
void fill_factor_from_hide(const SubdivCCG &subdiv_ccg,
                           Span<int> grids,
                           MutableSpan<float> r_factors);
void fill_factor_from_hide(const Set<BMVert *, 0> &verts, MutableSpan<float> r_factors);

/**
 * Calculate initial influence factors based on vertex visibility and masking.
 */
void fill_factor_from_hide_and_mask(const Mesh &mesh,
                                    Span<int> vert_indices,
                                    MutableSpan<float> r_factors);
void fill_factor_from_hide_and_mask(const SubdivCCG &subdiv_ccg,
                                    Span<int> grids,
                                    MutableSpan<float> r_factors);
void fill_factor_from_hide_and_mask(const BMesh &bm,
                                    const Set<BMVert *, 0> &verts,
                                    MutableSpan<float> r_factors);

/**
 * Disable brush influence when vertex normals point away from the view.
 */
void calc_front_face(const float3 &view_normal,
                     Span<float3> vert_normals,
                     Span<int> vert_indices,
                     MutableSpan<float> factors);
void calc_front_face(const float3 &view_normal,
                     const SubdivCCG &subdiv_ccg,
                     Span<int> grids,
                     MutableSpan<float> factors);
void calc_front_face(const float3 &view_normal,
                     const Set<BMVert *, 0> &verts,
                     const MutableSpan<float> factors);

/**
 * When the 3D view's clipping planes are enabled, brushes shouldn't have any effect on vertices
 * outside of the planes, because they're not visible. This function disables the factors for those
 * vertices.
 */
void filter_region_clip_factors(const SculptSession &ss,
                                Span<float3> vert_positions,
                                Span<int> verts,
                                MutableSpan<float> factors);
void filter_region_clip_factors(const SculptSession &ss,
                                Span<float3> positions,
                                MutableSpan<float> factors);

/**
 * Calculate distances based on the distance from the brush cursor and various other settings.
 * Also ignore vertices that are too far from the cursor.
 */
void calc_distance_falloff(const SculptSession &ss,
                           Span<float3> vert_positions,
                           Span<int> vert_indices,
                           eBrushFalloffShape falloff_shape,
                           MutableSpan<float> r_distances,
                           MutableSpan<float> factors);
void calc_distance_falloff(const SculptSession &ss,
                           Span<float3> positions,
                           const eBrushFalloffShape falloff_shape,
                           MutableSpan<float> r_distances,
                           MutableSpan<float> factors);

/**
 * Calculate distances based on a "square" brush tip falloff and ignore vertices that are too far
 * away.
 */
void calc_cube_distance_falloff(SculptSession &ss,
                                const Brush &brush,
                                const float4x4 &mat,
                                Span<float3> positions,
                                Span<int> verts,
                                MutableSpan<float> r_distances,
                                MutableSpan<float> factors);
void calc_cube_distance_falloff(SculptSession &ss,
                                const Brush &brush,
                                const float4x4 &mat,
                                const Span<float3> positions,
                                const MutableSpan<float> r_distances,
                                const MutableSpan<float> factors);

/**
 * Scale the distances based on the brush radius and the cached "hardness" setting, which increases
 * the strength of the effect for vertices torwards the outside of the radius.
 */
void apply_hardness_to_distances(const StrokeCache &cache, MutableSpan<float> distances);

/**
 * Modify the factors based on distances to the brush cursor, using various brush settings.
 */
void calc_brush_strength_factors(const StrokeCache &cache,
                                 const Brush &brush,
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
void calc_brush_texture_factors(SculptSession &ss,
                                const Brush &brush,
                                Span<float3> positions,
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
void calc_grids_factors(const Object &object,
                        const Cache &cache,
                        const PBVHNode &node,
                        Span<int> grids,
                        MutableSpan<float> factors);
void calc_vert_factors(const Object &object,
                       const Cache &cache,
                       const PBVHNode &node,
                       const Set<BMVert *, 0> &verts,
                       MutableSpan<float> factors);

}  // namespace auto_mask

/**
 * Many brushes end up calculating translations from the original positions. Instead of applying
 * these directly to the modified values, it's helpful to process them separately to easily
 * calculate various effects like clipping. After they are processed, this function can be used to
 * simply add them to the final vertex positions.
 */
void apply_translations(Span<float3> translations, Span<int> verts, MutableSpan<float3> positions);
void apply_translations(Span<float3> translations, Span<int> grids, SubdivCCG &subdiv_ccg);
void apply_translations(Span<float3> translations, const Set<BMVert *, 0> &verts);

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
void clip_and_lock_translations(const Sculpt &sd,
                                const SculptSession &ss,
                                Span<float3> positions,
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

/**
 * Write the new translated positions to the original mesh, taking into account inverse
 * deformation from modifiers, axis locking, and clipping. Flush the deformation to shape keys as
 * well.
 */
void write_translations(const Sculpt &sd,
                        Object &object,
                        Span<float3> positions_eval,
                        Span<int> verts,
                        MutableSpan<float3> translations,
                        MutableSpan<float3> positions_orig);

/**
 * Creates OffsetIndices based on each node's unique vertex count, allowing for easy slicing of a
 * new array.
 */
OffsetIndices<int> create_node_vert_offsets(Span<PBVHNode *> nodes, Array<int> &node_data);

/**
 * Find vertices connected to the indexed vertices across faces.
 *
 * Does not handle boundary vertices differently, so this method is generally inappropriate for
 * functions that are related to coordinates. See #calc_vert_neighbors_interior
 *
 * \note A vector allocated per element is typically not a good strategy for performance because
 * of each vector's 24 byte overhead, non-contiguous memory, and the possibility of further heap
 * allocations. However, it's done here for now for two reasons:
 *  1. In typical quad meshes there are just 4 neighbors, which fit in the inline buffer.
 *  2. We want to avoid using edges, and the remaining topology map we have access to is the
 *     vertex to face map. That requires deduplication when building the neighbors, which
 *     requires some intermediate data structure like a vector anyway.
 */
void calc_vert_neighbors(OffsetIndices<int> faces,
                         Span<int> corner_verts,
                         GroupedSpan<int> vert_to_face,
                         Span<bool> hide_poly,
                         Span<int> verts,
                         MutableSpan<Vector<int>> result);

/**
 * Find vertices connected to the indexed vertices across faces. For boundary vertices (stored in
 * the \a boundary_verts argument), only include other boundary vertices. Also skip connectivity
 * across hidden faces and skip neighbors of corner vertices.
 *
 * \note See #calc_vert_neighbors for information on why we use a Vector per element.
 */
void calc_vert_neighbors_interior(OffsetIndices<int> faces,
                                  Span<int> corner_verts,
                                  GroupedSpan<int> vert_to_face,
                                  BitSpan boundary_verts,
                                  Span<bool> hide_poly,
                                  Span<int> verts,
                                  MutableSpan<Vector<int>> result);

/** Find the translation from each vertex position to the closest point on the plane. */
void calc_translations_to_plane(Span<float3> vert_positions,
                                Span<int> verts,
                                const float4 &plane,
                                MutableSpan<float3> translations);
void calc_translations_to_plane(Span<float3> positions,
                                const float4 &plane,
                                MutableSpan<float3> translations);

/** Ignore points that fall below the "plane trim" threshold for the brush. */
void filter_plane_trim_limit_factors(const Brush &brush,
                                     const StrokeCache &cache,
                                     Span<float3> translations,
                                     MutableSpan<float> factors);

/** Ignore points below the plane. */
void filter_below_plane_factors(Span<float3> vert_positions,
                                Span<int> verts,
                                const float4 &plane,
                                MutableSpan<float> factors);
void filter_below_plane_factors(Span<float3> positions,
                                const float4 &plane,
                                MutableSpan<float> factors);

/* Ignore points above the plane. */
void filter_above_plane_factors(Span<float3> vert_positions,
                                Span<int> verts,
                                const float4 &plane,
                                MutableSpan<float> factors);
void filter_above_plane_factors(Span<float3> positions,
                                const float4 &plane,
                                MutableSpan<float> factors);

}  // namespace blender::ed::sculpt_paint
