/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */

#pragma once

#include "BLI_array.hh"
#include "BLI_offset_indices.hh"
#include "BLI_set.hh"
#include "BLI_span.hh"

#include "BKE_attribute.hh"

struct BMVert;
struct Mesh;
struct Object;
struct SubdivCCG;
struct SubdivCCGCoord;

namespace blender::ed::sculpt_paint::face_set {

int active_face_set_get(const Object &object);
int vert_face_set_get(GroupedSpan<int> vert_to_face_map, Span<int> face_sets, int vert);
int vert_face_set_get(const SubdivCCG &subdiv_ccg, Span<int> face_sets, int grid);
int vert_face_set_get(int face_set_offset, const BMVert &vert);

bool vert_has_face_set(GroupedSpan<int> vert_to_face_map,
                       Span<int> face_sets,
                       int vert,
                       int face_set);
bool vert_has_face_set(const SubdivCCG &subdiv_ccg, Span<int> face_sets, int grid, int face_set);
bool vert_has_face_set(int face_set_offset, const BMVert &vert, int face_set);
bool vert_has_unique_face_set(GroupedSpan<int> vert_to_face_map, Span<int> face_sets, int vert);
bool vert_has_unique_face_set(OffsetIndices<int> faces,
                              Span<int> corner_verts,
                              GroupedSpan<int> vert_to_face_map,
                              Span<int> face_sets,
                              const SubdivCCG &subdiv_ccg,
                              SubdivCCGCoord coord);
bool vert_has_unique_face_set(int face_set_offset, const BMVert &vert);

/**
 * Creates the sculpt face set attribute on the mesh if it doesn't exist.
 *
 * \see face_set::ensure_face_sets_mesh if further writing to the attribute is desired.
 */
bool create_face_sets_mesh(Object &object);

/**
 * Ensures that the sculpt face set attribute exists on the mesh.
 *
 * \see face_set::create_face_sets_mesh to avoid having to remember to call .finish()
 */
bke::SpanAttributeWriter<int> ensure_face_sets_mesh(Mesh &mesh);
int ensure_face_sets_bmesh(Object &object);
Array<int> duplicate_face_sets(const Mesh &mesh);
Set<int> gather_hidden_face_sets(Span<bool> hide_poly, Span<int> face_sets);

void filter_verts_with_unique_face_sets_mesh(GroupedSpan<int> vert_to_face_map,
                                             Span<int> face_sets,
                                             bool unique,
                                             Span<int> verts,
                                             MutableSpan<float> factors);
void filter_verts_with_unique_face_sets_grids(OffsetIndices<int> faces,
                                              Span<int> corner_verts,
                                              GroupedSpan<int> vert_to_face_map,
                                              Span<int> face_sets,
                                              const SubdivCCG &subdiv_ccg,
                                              bool unique,
                                              Span<int> grids,
                                              MutableSpan<float> factors);
void filter_verts_with_unique_face_sets_bmesh(int face_set_offset,
                                              bool unique,
                                              const Set<BMVert *, 0> &verts,
                                              MutableSpan<float> factors);

}  // namespace blender::ed::sculpt_paint::face_set
