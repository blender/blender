/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */

#pragma once

#include "BLI_generic_span.hh"
#include "BLI_math_vector.hh"
#include "BLI_offset_indices.hh"
#include "BLI_set.hh"

#include "BKE_subdiv_ccg.hh"

struct BMVert;
struct Object;
struct SubdivCCG;
struct SubdivCCGCoord;
namespace blender::bke {
enum class AttrDomain : int8_t;
}

namespace blender::ed::sculpt_paint::smooth {

/**
 * For bmesh: Average surrounding verts based on an orthogonality measure.
 * Naturally converges to a quad-like structure.
 */
void bmesh_four_neighbor_average(float avg[3], const float3 &direction, const BMVert *v);

void neighbor_color_average(OffsetIndices<int> faces,
                            Span<int> corner_verts,
                            GroupedSpan<int> vert_to_face_map,
                            GSpan color_attribute,
                            bke::AttrDomain color_domain,
                            Span<Vector<int>> vert_neighbors,
                            MutableSpan<float4> smooth_colors);

void neighbor_position_average_interior_grids(OffsetIndices<int> faces,
                                              Span<int> corner_verts,
                                              BitSpan boundary_verts,
                                              const SubdivCCG &subdiv_ccg,
                                              Span<int> grids,
                                              MutableSpan<float3> new_positions);

void neighbor_position_average_bmesh(const Set<BMVert *, 0> &verts,
                                     MutableSpan<float3> new_positions);
void neighbor_position_average_interior_bmesh(const Set<BMVert *, 0> &verts,
                                              MutableSpan<float3> new_positions);

template<typename T>
void neighbor_data_average_mesh(Span<T> src, Span<Vector<int>> vert_neighbors, MutableSpan<T> dst);
template<typename T>
void neighbor_data_average_mesh_check_loose(Span<T> src,
                                            Span<int> verts,
                                            Span<Vector<int>> vert_neighbors,
                                            MutableSpan<T> dst);

template<typename T>
void average_data_grids(const SubdivCCG &subdiv_ccg,
                        Span<T> src,
                        Span<int> grids,
                        MutableSpan<T> dst);

template<typename T>
void average_data_bmesh(Span<T> src, const Set<BMVert *, 0> &verts, MutableSpan<T> dst);

/* Average the data in the argument span across vertex neighbors. */
void blur_geometry_data_array(const Object &object, int iterations, MutableSpan<float> data);

/* Surface Smooth Brush. */

void surface_smooth_laplacian_step(Span<float3> positions,
                                   Span<float3> orig_positions,
                                   Span<float3> average_positions,
                                   float alpha,
                                   MutableSpan<float3> laplacian_disp,
                                   MutableSpan<float3> translations);
void surface_smooth_displace_step(Span<float3> laplacian_disp,
                                  Span<float3> average_laplacian_disp,
                                  float beta,
                                  MutableSpan<float3> translations);

void calc_relaxed_translations_faces(Span<float3> vert_positions,
                                     Span<float3> vert_normals,
                                     OffsetIndices<int> faces,
                                     Span<int> corner_verts,
                                     GroupedSpan<int> vert_to_face_map,
                                     BitSpan boundary_verts,
                                     Span<int> face_sets,
                                     Span<bool> hide_poly,
                                     bool filter_boundary_face_sets,
                                     Span<int> verts,
                                     Span<float> factors,
                                     Vector<Vector<int>> &neighbors,
                                     MutableSpan<float3> translations);
void calc_relaxed_translations_grids(const SubdivCCG &subdiv_ccg,
                                     OffsetIndices<int> faces,
                                     Span<int> corner_verts,
                                     Span<int> face_sets,
                                     GroupedSpan<int> vert_to_face_map,
                                     BitSpan boundary_verts,
                                     Span<int> grids,
                                     bool filter_boundary_face_sets,
                                     Span<float> factors,
                                     Vector<Vector<SubdivCCGCoord>> &neighbors,
                                     MutableSpan<float3> translations);
void calc_relaxed_translations_bmesh(const Set<BMVert *, 0> &verts,
                                     Span<float3> positions,
                                     const int face_set_offset,
                                     bool filter_boundary_face_sets,
                                     Span<float> factors,
                                     Vector<Vector<BMVert *>> &neighbors,
                                     MutableSpan<float3> translations);

}  // namespace blender::ed::sculpt_paint::smooth
