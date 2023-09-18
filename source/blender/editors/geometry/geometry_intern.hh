/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgeometry
 */

#pragma once

struct wmOperatorType;

namespace blender::ed::geometry {

/* *** geometry_attributes.cc *** */
void GEOMETRY_OT_attribute_add(wmOperatorType *ot);
void GEOMETRY_OT_attribute_remove(wmOperatorType *ot);
void GEOMETRY_OT_color_attribute_add(wmOperatorType *ot);
void GEOMETRY_OT_color_attribute_remove(wmOperatorType *ot);
void GEOMETRY_OT_color_attribute_render_set(wmOperatorType *ot);
void GEOMETRY_OT_color_attribute_duplicate(wmOperatorType *ot);
void GEOMETRY_OT_attribute_convert(wmOperatorType *ot);
void GEOMETRY_OT_color_attribute_convert(wmOperatorType *ot);

void GEOMETRY_OT_execute_node_group(wmOperatorType *ot);

}  // namespace blender::ed::geometry
