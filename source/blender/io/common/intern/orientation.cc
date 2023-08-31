/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_main.h"
#include "DNA_scene_types.h"
#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_types.hh"

#include "BLI_math_basis_types.hh"

#include "IO_orientation.hh"

static const EnumPropertyItem io_transform_axis[] = {
    {int(blender::math::AxisSigned::X_POS), "X", 0, "X", "Positive X axis"},
    {int(blender::math::AxisSigned::Y_POS), "Y", 0, "Y", "Positive Y axis"},
    {int(blender::math::AxisSigned::Z_POS), "Z", 0, "Z", "Positive Z axis"},
    {int(blender::math::AxisSigned::X_NEG), "NEGATIVE_X", 0, "-X", "Negative X axis"},
    {int(blender::math::AxisSigned::Y_NEG), "NEGATIVE_Y", 0, "-Y", "Negative Y axis"},
    {int(blender::math::AxisSigned::Z_NEG), "NEGATIVE_Z", 0, "-Z", "Negative Z axis"},
    {0, nullptr, 0, nullptr, nullptr}};

static void io_ui_forward_axis_update(Main * /*main*/, Scene * /*scene*/, PointerRNA *ptr)
{
  /* Both forward and up axes cannot be along the same direction. */

  int forward = RNA_enum_get(ptr, "forward_axis");
  int up = RNA_enum_get(ptr, "up_axis");
  if ((forward % 3) == (up % 3)) {
    RNA_enum_set(ptr, "up_axis", (up + 1) % 6);
  }
}

static void io_ui_up_axis_update(Main * /*main*/, Scene * /*scene*/, PointerRNA *ptr)
{
  int forward = RNA_enum_get(ptr, "forward_axis");
  int up = RNA_enum_get(ptr, "up_axis");
  if ((forward % 3) == (up % 3)) {
    RNA_enum_set(ptr, "forward_axis", (forward + 1) % 6);
  }
}

void io_ui_axes_register(StructRNA &srna)
{
  PropertyRNA *forward = RNA_def_enum(&srna,
                                      "forward_axis",
                                      io_transform_axis,
                                      int(blender::math::AxisSigned::Y_POS),
                                      "Forward Axis",
                                      "");
  RNA_def_property_update_runtime(forward, io_ui_forward_axis_update);
  PropertyRNA *up = RNA_def_enum(
      &srna, "up_axis", io_transform_axis, int(blender::math::AxisSigned::Z_POS), "Up Axis", "");
  RNA_def_property_update_runtime(up, io_ui_up_axis_update);
}
