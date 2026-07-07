/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgeometry
 */

#pragma once

namespace blender {

struct wmOperatorType;

namespace ed::geometry {

/* *** geometry_attributes.cc *** */
void GEOMETRY_OT_attribute_add(wmOperatorType *ot);
void GEOMETRY_OT_attribute_remove(wmOperatorType *ot);
void GEOMETRY_OT_color_attribute_add(wmOperatorType *ot);
void GEOMETRY_OT_color_attribute_remove(wmOperatorType *ot);
void GEOMETRY_OT_color_attribute_render_set(wmOperatorType *ot);
void GEOMETRY_OT_color_attribute_duplicate(wmOperatorType *ot);
void GEOMETRY_OT_attribute_convert(wmOperatorType *ot);
void GEOMETRY_OT_color_attribute_convert(wmOperatorType *ot);
void GEOMETRY_OT_geometry_randomization(wmOperatorType *ot);

}  // namespace ed::geometry
}  // namespace blender
