/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */

#pragma once

#include "BLI_index_mask_fwd.hh"

struct bContext;
struct Depsgraph;
struct Object;
struct wmOperatorType;
namespace blender::bke::pbvh {
class Node;
}

namespace blender::ed::sculpt_paint::hide {
void sync_all_from_faces(Object &object);
void mesh_show_all(const Depsgraph &depsgraph, Object &object, const IndexMask &node_mask);
void grids_show_all(Depsgraph &depsgraph, Object &object, const IndexMask &node_mask);
void tag_update_visibility(const bContext &C);

void PAINT_OT_hide_show_masked(wmOperatorType *ot);
void PAINT_OT_hide_show_all(wmOperatorType *ot);
void PAINT_OT_hide_show(wmOperatorType *ot);
void PAINT_OT_hide_show_lasso_gesture(wmOperatorType *ot);
void PAINT_OT_hide_show_line_gesture(wmOperatorType *ot);
void PAINT_OT_hide_show_polyline_gesture(wmOperatorType *ot);

void PAINT_OT_visibility_invert(wmOperatorType *ot);
void PAINT_OT_visibility_filter(wmOperatorType *ot);
}  // namespace blender::ed::sculpt_paint::hide
