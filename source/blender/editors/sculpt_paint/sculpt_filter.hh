/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */

#pragma once

#include <array>

#include "BLI_array.hh"
#include "BLI_index_mask.hh"
#include "BLI_math_matrix.hh"
#include "BLI_math_vector.hh"
#include "BLI_vector.hh"

#include "ED_view3d.hh"

struct wmOperatorType;

namespace blender {
namespace bke::pbvh {
class Node;
}
namespace ed::sculpt_paint {
enum class TransformDisplacementMode;
namespace auto_mask {
struct Cache;
}
namespace cloth {
struct SimulationData;
}
namespace undo {
enum class Type : int8_t;
}
}  // namespace ed::sculpt_paint
}  // namespace blender

namespace blender::ed::sculpt_paint::filter {

enum class FilterOrientation {
  Local = 0,
  World = 1,
  View = 2,
};

struct Cache {
  std::array<bool, 3> enabled_axis;
  int random_seed;

  /* Used for alternating between filter operations in filters that need to apply different ones to
   * achieve certain effects. */
  int iteration_count;

  /* Stores the displacement produced by the laplacian step of HC smooth. */
  Array<float3> surface_smooth_laplacian_disp;
  float surface_smooth_shape_preservation;
  float surface_smooth_current_vertex;

  /* Sharpen mesh filter. */
  float sharpen_smooth_ratio;
  float sharpen_intensify_detail_strength;
  int sharpen_curvature_smooth_iterations;
  Array<float> sharpen_factor;
  Array<float3> detail_directions;

  /* Filter orientation. */
  FilterOrientation orientation;
  float4x4 obmat;
  float4x4 obmat_inv;
  float4x4 viewmat;
  float4x4 viewmat_inv;

  /* Displacement eraser. */
  Array<float3> limit_surface_co;

  /* unmasked nodes */
  IndexMaskMemory node_mask_memory;
  IndexMask node_mask;

  /* Cloth filter. */
  std::unique_ptr<cloth::SimulationData> cloth_sim;
  float3 cloth_sim_pinch_point;

  /* mask expand iteration caches */
  int mask_update_current_it;
  int mask_update_last_it;
  Array<int> mask_update_it;
  Array<float> normal_factor;
  Array<float> edge_factor;
  Array<float> prev_mask;
  float3 mask_expand_initial_co;

  int new_face_set;
  Array<int> prev_face_set;

  int active_face_set;

  TransformDisplacementMode transform_displacement_mode;

  std::unique_ptr<auto_mask::Cache> automasking;
  float3 initial_normal;
  float3 view_normal;

  /* Pre-smoothed colors used by sharpening. Colors are HSL. */
  Array<float4> pre_smoothed_color;

  ViewContext vc;
  float start_filter_strength;

  ~Cache();
};

void cache_init(bContext *C,
                Object &ob,
                const Sculpt &sd,
                undo::Type undo_type,
                const float mval_fl[2],
                float area_normal_radius,
                float start_strength);
void register_operator_props(wmOperatorType *ot);

/* Filter orientation utils. */
float3x3 to_orientation_space(const filter::Cache &filter_cache);
float3x3 to_object_space(const filter::Cache &filter_cache);
void zero_disabled_axis_components(const filter::Cache &filter_cache, MutableSpan<float3> vectors);
}  // namespace blender::ed::sculpt_paint::filter
