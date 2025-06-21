/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_index_mask.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_math_vector_types.hh"

#include <optional>

struct Brush;
struct Depsgraph;
struct Scene;
struct Sculpt;
namespace blender::ed::sculpt_paint {
struct StrokeCache;
};
struct SculptSession;
struct Object;
namespace blender::bke::pbvh {
class Node;
}

namespace blender::ed::sculpt_paint::brushes {

/** Represents the result of one or more BVH queries to find a brush's affected nodes. */
struct CursorSampleResult {
  IndexMask node_mask;

  /* For planar brushes, the plane center and normal are calculated based on the original cursor
   * position and needed for further calculations when performing brush strokes.
   */
  std::optional<float3> plane_center;
  std::optional<float3> plane_normal;
};

void do_clay_brush(const Depsgraph &depsgraph,
                   const Sculpt &sd,
                   Object &ob,
                   const IndexMask &node_mask);
/**
 * Basic principles of the clay strips brush:
 * * Calculate a brush plane from an initial node mask
 * * Use this center position and normal to create a brush-local matrix
 * * Use this matrix and the plane to calculate and use cube distances for
 * * the affected area
 */
void do_clay_strips_brush(const Depsgraph &depsgraph,
                          const Sculpt &sd,
                          Object &ob,
                          const IndexMask &node_mask,
                          const float3 &plane_normal,
                          const float3 &plane_center);
namespace clay_strips {
float4x4 calc_local_matrix(const Brush &brush,
                           const StrokeCache &cache,
                           const float3 &plane_normal,
                           const float3 &plane_center,
                           const bool flip);
CursorSampleResult calc_node_mask(const Depsgraph &depsgraph,
                                  Object &ob,
                                  const Brush &brush,
                                  IndexMaskMemory &memory);
}  // namespace clay_strips
void do_clay_thumb_brush(const Depsgraph &depsgraph,
                         const Sculpt &sd,
                         Object &ob,
                         const IndexMask &node_mask);
float clay_thumb_get_stabilized_pressure(const StrokeCache &cache);

void do_crease_brush(const Depsgraph &depsgraph,
                     const Sculpt &sd,
                     Object &ob,
                     const IndexMask &node_mask);
void do_blob_brush(const Depsgraph &depsgraph,
                   const Sculpt &sd,
                   Object &ob,
                   const IndexMask &node_mask);
void do_bmesh_topology_rake_brush(const Depsgraph &depsgraph,
                                  const Sculpt &sd,
                                  Object &ob,
                                  const IndexMask &node_mask,
                                  float strength);
void do_displacement_eraser_brush(const Depsgraph &depsgraph,
                                  const Sculpt &sd,
                                  Object &ob,
                                  const IndexMask &node_mask);
void do_displacement_smear_brush(const Depsgraph &depsgraph,
                                 const Sculpt &sd,
                                 Object &ob,
                                 const IndexMask &node_mask);
void do_draw_face_sets_brush(const Depsgraph &depsgraph,
                             const Sculpt &sd,
                             Object &object,
                             const IndexMask &node_mask);
/** A simple normal-direction displacement. */
void do_draw_brush(const Depsgraph &depsgraph,
                   const Sculpt &sd,
                   Object &object,
                   const IndexMask &node_mask);
/** A simple normal-direction displacement based on image texture RGB/XYZ values. */
void do_draw_vector_displacement_brush(const Depsgraph &depsgraph,
                                       const Sculpt &sd,
                                       Object &object,
                                       const IndexMask &node_mask);
void do_draw_sharp_brush(const Depsgraph &depsgraph,
                         const Sculpt &sd,
                         Object &object,
                         const IndexMask &node_mask);
void do_elastic_deform_brush(const Depsgraph &depsgraph,
                             const Sculpt &sd,
                             Object &object,
                             const IndexMask &node_mask);
void do_enhance_details_brush(const Depsgraph &depsgraph,
                              const Sculpt &sd,
                              Object &object,
                              const IndexMask &node_mask);
void do_plane_brush(const Depsgraph &depsgraph,
                    const Sculpt &sd,
                    Object &object,
                    const IndexMask &node_mask,
                    const float3 &plane_normal,
                    const float3 &plane_center);

namespace plane {
CursorSampleResult calc_node_mask(const Depsgraph &depsgraph,
                                  Object &ob,
                                  const Brush &brush,
                                  IndexMaskMemory &memory);
}

void do_grab_brush(const Depsgraph &depsgraph,
                   const Sculpt &sd,
                   Object &ob,
                   const IndexMask &node_mask);
void do_gravity_brush(const Depsgraph &depsgraph,
                      const Sculpt &sd,
                      Object &ob,
                      const IndexMask &node_mask);
void do_inflate_brush(const Depsgraph &depsgraph,
                      const Sculpt &sd,
                      Object &ob,
                      const IndexMask &node_mask);
void do_layer_brush(const Depsgraph &depsgraph,
                    const Sculpt &sd,
                    Object &object,
                    const IndexMask &node_mask);
/** A brush that modifies mask values instead of position. */
void do_mask_brush(const Depsgraph &depsgraph,
                   const Sculpt &sd,
                   Object &object,
                   const IndexMask &node_mask);
void do_multiplane_scrape_brush(const Depsgraph &depsgraph,
                                const Sculpt &sd,
                                Object &object,
                                const IndexMask &node_mask);
void multiplane_scrape_preview_draw(uint gpuattr,
                                    const Brush &brush,
                                    const SculptSession &ss,
                                    const float outline_col[3],
                                    float outline_alpha);

void do_pinch_brush(const Depsgraph &depsgraph,
                    const Sculpt &sd,
                    Object &object,
                    const IndexMask &node_mask);
void do_nudge_brush(const Depsgraph &depsgraph,
                    const Sculpt &sd,
                    Object &object,
                    const IndexMask &node_mask);
void do_relax_face_sets_brush(const Depsgraph &depsgraph,
                              const Sculpt &sd,
                              Object &object,
                              const IndexMask &node_mask);
void do_rotate_brush(const Depsgraph &depsgraph,
                     const Sculpt &sd,
                     Object &object,
                     const IndexMask &node_mask);
/** Smooth positions with neighboring vertices. */
void do_smooth_brush(const Depsgraph &depsgraph,
                     const Sculpt &sd,
                     Object &object,
                     const IndexMask &node_mask,
                     float brush_strength);
/** Smooth mask values with neighboring vertices. */
void do_smooth_mask_brush(const Depsgraph &depsgraph,
                          const Sculpt &sd,
                          Object &object,
                          const IndexMask &node_mask,
                          float brush_strength);
void do_snake_hook_brush(const Depsgraph &depsgraph,
                         const Sculpt &sd,
                         Object &object,
                         const IndexMask &node_mask);
void do_surface_smooth_brush(const Depsgraph &depsgraph,
                             const Sculpt &sd,
                             Object &object,
                             const IndexMask &node_mask);
void do_thumb_brush(const Depsgraph &depsgraph,
                    const Sculpt &sd,
                    Object &object,
                    const IndexMask &node_mask);
void do_topology_slide_brush(const Depsgraph &depsgraph,
                             const Sculpt &sd,
                             Object &object,
                             const IndexMask &node_mask);
void do_topology_relax_brush(const Depsgraph &depsgraph,
                             const Sculpt &sd,
                             Object &object,
                             const IndexMask &node_mask);

}  // namespace blender::ed::sculpt_paint::brushes
