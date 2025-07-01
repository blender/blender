/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

#include <limits>
#include <optional>

#include "BLI_index_mask_fwd.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_vector_set.hh"

#include "DNA_windowmanager_enums.h"

struct ARegion;
struct bContext;
struct PointCloud;
struct rcti;
struct UndoType;
struct wmKeyConfig;
struct wmOperator;
struct wmOperatorType;
namespace blender::bke {
enum class AttrType : int16_t;
struct GSpanAttributeWriter;
}  // namespace blender::bke
namespace blender {
class GMutableSpan;
}  // namespace blender
enum eSelectOp : int8_t;

namespace blender::ed::pointcloud {

void operatortypes_pointcloud();
void operatormacros_pointcloud();
void keymap_pointcloud(wmKeyConfig *keyconf);
void undosys_type_register(UndoType *ut);

VectorSet<PointCloud *> get_unique_editable_pointclouds(const bContext &C);

/* -------------------------------------------------------------------- */
/** \name Selection
 *
 * Selection on point cloud are stored per-point. It
 * can be stored with a float or boolean data-type. The boolean data-type is faster, smaller, and
 * corresponds better to edit-mode selections, but the float data type is useful for soft selection
 * (like masking) in sculpt mode.
 *
 * The attribute API is used to do the necessary type and domain conversions when necessary, and
 * can handle most interaction with the selection attribute, but these functions implement some
 * helpful utilities on top of that.
 * \{ */

void fill_selection_true(GMutableSpan span);
void fill_selection_false(GMutableSpan selection, const IndexMask &mask);
void fill_selection_true(GMutableSpan selection, const IndexMask &mask);

/**
 * Return true if any element is selected, on either domain with either type.
 */
bool has_anything_selected(const PointCloud &pointcloud);

/**
 * (De)select all the points.
 *
 * \param action: One of #SEL_TOGGLE, #SEL_SELECT, #SEL_DESELECT, or #SEL_INVERT.
 * See `ED_select_utils.hh`.
 */
void select_all(PointCloud &pointcloud, int action);

/**
 * If the selection_id attribute doesn't exist, create it with the requested type (bool or float).
 */
bke::GSpanAttributeWriter ensure_selection_attribute(PointCloud &pointcloud,
                                                     bke::AttrType create_type);

bool select_box(PointCloud &pointcloud,
                const ARegion &region,
                const float4x4 &projection,
                const rcti &rect,
                const eSelectOp sel_op);

bool select_lasso(PointCloud &pointcloud,
                  const ARegion &region,
                  const float4x4 &projection,
                  const Span<int2> lasso_coords,
                  const eSelectOp sel_op);

bool select_circle(PointCloud &pointcloud,
                   const ARegion &region,
                   const float4x4 &projection,
                   const int2 coord,
                   const float radius,
                   const eSelectOp sel_op);

struct FindClosestData {
  int index = -1;
  float distance_sq = std::numeric_limits<float>::max();
};

std::optional<FindClosestData> find_closest_point_to_screen_co(
    const ARegion &region,
    const Span<float3> positions,
    const float4x4 &projection,
    const IndexMask &points_mask,
    const float2 mouse_pos,
    const float radius,
    const FindClosestData &initial_closest);

IndexMask retrieve_selected_points(const PointCloud &pointcloud, IndexMaskMemory &memory);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Editing
 * \{ */

/**
 * Remove selected points based on the ".selection" attribute.
 * \returns true if any point was removed.
 */
bool remove_selection(PointCloud &pointcloud);
PointCloud *copy_selection(const PointCloud &src, const IndexMask &mask);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Poll Functions
 * \{ */

bool editable_pointcloud_in_edit_mode_poll(bContext *C);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Operators
 * \{ */

void POINTCLOUD_OT_attribute_set(wmOperatorType *ot);
void POINTCLOUD_OT_duplicate(wmOperatorType *ot);
void POINTCLOUD_OT_separate(wmOperatorType *ot);

wmOperatorStatus join_objects_exec(bContext *C, wmOperator *op);

/** \} */

}  // namespace blender::ed::pointcloud
