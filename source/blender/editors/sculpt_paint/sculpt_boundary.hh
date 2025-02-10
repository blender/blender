/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */
#pragma once

#include <memory>

#include "BLI_array.hh"
#include "BLI_bit_span.hh"
#include "BLI_map.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_offset_indices.hh"
#include "BLI_span.hh"
#include "BLI_vector.hh"

struct Brush;
struct BMVert;
struct Depsgraph;
struct Object;
struct SculptBoundaryPreview;
struct SculptSession;
struct SubdivCCG;
struct SubdivCCGCoord;
namespace blender::bke::pbvh {
class Node;
}

namespace blender::ed::sculpt_paint::boundary {

struct SculptBoundary {
  /* Vertex indices of the active boundary. */
  Vector<int> verts;

  /* Distance from a vertex in the boundary to initial vertex indexed by vertex index, taking into
   * account the length of all edges between them. Any vertex that is not in the boundary will have
   * a distance of 0. */
  Map<int, float> distance;

  /* Data for drawing the preview. */
  Vector<std::pair<float3, float3>> edges;

  /* Initial vertex index in the boundary which is closest to the current sculpt active vertex. */
  int initial_vert_i;

  /* Vertex that at max_propagation_steps from the boundary and closest to the original active
   * vertex that was used to initialize the boundary. This is used as a reference to check how much
   * the deformation will go into the mesh and to calculate the strength of the brushes. */
  float3 pivot_position;

  /* Stores the initial positions of the pivot and boundary initial vertex as they may be deformed
   * during the brush action. This allows to use them as a reference positions and vectors for some
   * brush effects. */
  float3 initial_vert_position;

  /* Maximum number of topology steps that were calculated from the boundary. */
  int max_propagation_steps;

  /* Indexed by vertex index, contains the topology information needed for boundary deformations.
   */
  struct {
    /* Vertex index from where the topology propagation reached this vertex. */
    Array<int> original_vertex_i;

    /* How many steps were needed to reach this vertex from the boundary. */
    Array<int> propagation_steps_num;

    /* Strength that is used to deform this vertex. */
    Array<float> strength_factor;
  } edit_info;

  /* Bend Deform type. */
  struct {
    Array<float3> pivot_rotation_axis;
    Array<float3> pivot_positions;
  } bend;

  /* Slide Deform type. */
  struct {
    Array<float3> directions;
  } slide;

  /* Twist Deform type. */
  struct {
    float3 rotation_axis;
    float3 pivot_position;
  } twist;
};

/**
 * Populates boundary information for a mesh.
 *
 * \see SculptVertexInfo
 */
void ensure_boundary_info(Object &object);

/**
 * Determine if a vertex is a boundary vertex.
 *
 * Requires #ensure_boundary_info to have been called.
 */
bool vert_is_boundary(GroupedSpan<int> vert_to_face_map,
                      Span<bool> hide_poly,
                      BitSpan boundary,
                      int vert);
bool vert_is_boundary(OffsetIndices<int> faces,
                      Span<int> corner_verts,
                      BitSpan boundary,
                      const SubdivCCG &subdiv_ccg,
                      SubdivCCGCoord vert);
bool vert_is_boundary(BMVert *vert);

/**
 * Main function to get #SculptBoundary data both for brush deformation and viewport preview.
 * Can return NULL if there is no boundary from the given vertex using the given radius.
 */
std::unique_ptr<SculptBoundary> data_init(const Depsgraph &depsgraph,
                                          Object &object,
                                          const Brush *brush,
                                          int initial_vert,
                                          float radius);
std::unique_ptr<SculptBoundary> data_init_mesh(const Depsgraph &depsgraph,
                                               Object &object,
                                               const Brush *brush,
                                               int initial_vert,
                                               float radius);
std::unique_ptr<SculptBoundary> data_init_grids(Object &object,
                                                const Brush *brush,
                                                SubdivCCGCoord initial_vert,
                                                float radius);
std::unique_ptr<SculptBoundary> data_init_bmesh(Object &object,
                                                const Brush *brush,
                                                BMVert *initial_vert,
                                                float radius);
std::unique_ptr<SculptBoundaryPreview> preview_data_init(const Depsgraph &depsgraph,
                                                         Object &object,
                                                         const Brush *brush,
                                                         float radius);

void edges_preview_draw(uint gpuattr,
                        SculptSession &ss,
                        const float outline_col[3],
                        float outline_alpha);
void pivot_line_preview_draw(uint gpuattr, SculptSession &ss);

}  // namespace blender::ed::sculpt_paint::boundary
