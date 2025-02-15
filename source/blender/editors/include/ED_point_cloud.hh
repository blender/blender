/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

#include "BLI_index_mask_fwd.hh"

#include "DNA_customdata_types.h"

struct bContext;
struct PointCloud;
struct wmKeyConfig;
namespace blender::bke {
struct GSpanAttributeWriter;
}  // namespace blender::bke
namespace blender {
class GMutableSpan;
}  // namespace blender

namespace blender::ed::point_cloud {

void operatortypes_point_cloud();
void keymap_point_cloud(wmKeyConfig *keyconf);

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

void fill_selection_false(GMutableSpan selection, const IndexMask &mask);
void fill_selection_true(GMutableSpan selection, const IndexMask &mask);

/**
 * Return true if any element is selected, on either domain with either type.
 */
bool has_anything_selected(const PointCloud &point_cloud);

/**
 * (De)select all the points.
 *
 * \param action: One of #SEL_TOGGLE, #SEL_SELECT, #SEL_DESELECT, or #SEL_INVERT.
 * See `ED_select_utils.hh`.
 */
void select_all(PointCloud &point_cloud, int action);

/**
 * If the selection_id attribute doesn't exist, create it with the requested type (bool or float).
 */
bke::GSpanAttributeWriter ensure_selection_attribute(PointCloud &point_cloud,
                                                     eCustomDataType create_type);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Poll Functions
 * \{ */

bool editable_point_cloud_in_edit_mode_poll(bContext *C);

/** \} */

}  // namespace blender::ed::point_cloud
