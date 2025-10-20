/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include <fmt/format.h>

#include "BKE_unit.hh"

#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"

#include "BLT_translation.hh"

#include "RNA_access.hh"
#include "RNA_enum_types.hh"

#include "interface_intern.hh"

using blender::StringRef;
using blender::StringRefNull;

/* Format translation/rotation value as a string based on Blender unit settings. */
static std::string format_unit_value(float value, PropertySubType subtype, uiLayout *layout)
{
  const UnitSettings *unit = layout->block()->unit;
  const int unit_type = RNA_SUBTYPE_UNIT(subtype);

  /* Change negative zero to regular zero, without altering anything else. */
  value += +0.0f;
  double value_scaled = BKE_unit_value_scale(*unit, unit_type, value);

  char new_str[UI_MAX_DRAW_STR];
  BKE_unit_value_as_string(new_str,
                           sizeof(new_str),
                           value_scaled,
                           RNA_TRANSLATION_PREC_DEFAULT,
                           RNA_SUBTYPE_UNIT_VALUE(unit_type),
                           *unit,
                           true);
  return std::string(new_str);
}

/* Format unitless value as a string. */
static std::string format_coefficient(float value)
{
  /* Change negative zero to regular zero, without altering anything else. */
  value += +0.0f;
  /* Same precision that we use in `Object.scale`. */
  const int RNA_SCALE_PREC_DEFAULT = 3;
  return fmt::format("{:.{}f}", value, RNA_SCALE_PREC_DEFAULT);
}

/* Static variable to store rotation mode button state at runtime.
 * Defaults to XYZ Euler. */
static int rotation_mode_index = ROT_MODE_EUL;

static void rotation_mode_menu_callback(bContext *, uiLayout *layout, void *)
{
  for (size_t i = 0; i < RNA_enum_items_count(rna_enum_object_rotation_mode_items); i++) {
    const EnumPropertyItem &mode_info = rna_enum_object_rotation_mode_items[i];
    const int yco = -1.5f * UI_UNIT_Y;
    const int width = 9 * UI_UNIT_X;
    uiBut *but = uiDefButI(layout->block(),
                           ButType::Row,
                           0,
                           IFACE_(mode_info.name),
                           0,
                           yco,
                           width / 2,
                           UI_UNIT_Y,
                           &rotation_mode_index,
                           i,
                           i,
                           TIP_(mode_info.description));
    UI_but_flag_disable(but, UI_BUT_UNDO);
    if (i == rotation_mode_index) {
      UI_but_flag_enable(but, UI_SELECT_DRAW);
    }
  }
}

static void draw_matrix_template(uiLayout &layout, PointerRNA &ptr, PropertyRNA &prop)
{
  /* Matrix template UI is mirroring Object's Transform UI for better UX. */
  uiLayout *row, *col;
  uiLayout *layout_ = &layout.box();

  float m4[4][4];
  RNA_property_float_get_array(&ptr, &prop, &m4[0][0]);

  /* Show a warning as a matrix with a shear cannot be represented fully
   * by a decomposition.
   * Use the 3x3 matrix, as shear in the 4x4 homogeneous matrix
   * is expected due to the translation component. */
  float m3[3][3];
  copy_m3_m4(m3, m4);
  if (!is_orthogonal_m3(m3)) {
    layout_->label(RPT_("Matrix has a shear"), ICON_ERROR);
  }

  float loc[3], quat[4], size[3];
  mat4_decompose(loc, quat, size, m4);

  /* Translation. */
  col = &layout_->column(true);
  col->use_property_split_set(true);

  row = &col->row(true);
  uiItemL_respect_property_split(row, IFACE_("Location X"), ICON_NONE);
  row->label(format_unit_value(loc[0], PROP_TRANSLATION, layout_), ICON_NONE);

  row = &col->row(true);
  uiItemL_respect_property_split(row, IFACE_("Y"), ICON_NONE);
  row->label(format_unit_value(loc[1], PROP_TRANSLATION, layout_), ICON_NONE);

  row = &col->row(true);
  uiItemL_respect_property_split(row, IFACE_("Z"), ICON_NONE);
  row->label(format_unit_value(loc[2], PROP_TRANSLATION, layout_), ICON_NONE);

  /* Rotation. */
  float eul[3], axis[3];
  float angle;
  const EnumPropertyItem &mode_info = rna_enum_object_rotation_mode_items[rotation_mode_index];
  col = &layout_->column(true);
  col->use_property_split_set(true);

  if (mode_info.value == ROT_MODE_QUAT) {
    row = &col->row(true);
    uiItemL_respect_property_split(row, IFACE_("Rotation W"), ICON_NONE);
    row->label(format_coefficient(quat[0]), ICON_NONE);

    row = &col->row(true);
    uiItemL_respect_property_split(row, IFACE_("X"), ICON_NONE);
    row->label(format_coefficient(quat[1]), ICON_NONE);

    row = &col->row(true);
    uiItemL_respect_property_split(row, IFACE_("Y"), ICON_NONE);
    row->label(format_coefficient(quat[2]), ICON_NONE);

    row = &col->row(true);
    uiItemL_respect_property_split(row, IFACE_("Z"), ICON_NONE);
    row->label(format_coefficient(quat[3]), ICON_NONE);
  }
  else if (mode_info.value == ROT_MODE_AXISANGLE) {
    quat_to_axis_angle(axis, &angle, quat);

    row = &col->row(true);
    uiItemL_respect_property_split(row, IFACE_("Rotation W"), ICON_NONE);
    row->label(format_unit_value(angle, PROP_ANGLE, layout_), ICON_NONE);

    row = &col->row(true);
    uiItemL_respect_property_split(row, IFACE_("X"), ICON_NONE);
    row->label(format_coefficient(axis[0]), ICON_NONE);

    row = &col->row(true);
    uiItemL_respect_property_split(row, IFACE_("Y"), ICON_NONE);
    row->label(format_coefficient(axis[1]), ICON_NONE);

    row = &col->row(true);
    uiItemL_respect_property_split(row, IFACE_("Z"), ICON_NONE);
    row->label(format_coefficient(axis[2]), ICON_NONE);
  }
  else { /* Euler modes. */
    quat_to_eulO(eul, mode_info.value, quat);

    row = &col->row(true);
    uiItemL_respect_property_split(row, IFACE_("Rotation X"), ICON_NONE);
    row->label(format_unit_value(eul[0], PROP_ANGLE, layout_), ICON_NONE);

    row = &col->row(true);
    uiItemL_respect_property_split(row, IFACE_("Y"), ICON_NONE);
    row->label(format_unit_value(eul[1], PROP_ANGLE, layout_), ICON_NONE);

    row = &col->row(true);
    uiItemL_respect_property_split(row, IFACE_("Z"), ICON_NONE);
    row->label(format_unit_value(eul[2], PROP_ANGLE, layout_), ICON_NONE);
  }

  /* Mirror RNA enum property dropdown UI - with menu triangle an dropdown items. */
  row = &layout_->row(true);
  uiItemL_respect_property_split(row, IFACE_("Mode"), ICON_NONE);
  uiBlock *block = row->block();
  uiBut *but = uiDefMenuBut(block,
                            rotation_mode_menu_callback,
                            nullptr,
                            IFACE_(mode_info.name),
                            0,
                            0,
                            UI_UNIT_X * 10,
                            UI_UNIT_Y,
                            TIP_("Rotation mode.\n\nOnly affects the way "
                                 "rotation is displayed, rotation itself is unaffected."));
  UI_but_type_set_menu_from_pulldown(but);

  /* Scale. */
  col = &layout_->column(true);
  col->use_property_split_set(true);

  row = &col->row(true);
  uiItemL_respect_property_split(row, IFACE_("Scale X"), ICON_NONE);
  row->label(format_coefficient(size[0]), ICON_NONE);

  row = &col->row(true);
  uiItemL_respect_property_split(row, IFACE_("Y"), ICON_NONE);
  row->label(format_coefficient(size[1]), ICON_NONE);

  row = &col->row(true);
  uiItemL_respect_property_split(row, IFACE_("Z"), ICON_NONE);
  row->label(format_coefficient(size[2]), ICON_NONE);
}

void uiTemplateMatrix(uiLayout *layout, PointerRNA *ptr, const StringRefNull propname)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, propname.c_str());

  if (!prop || RNA_property_type(prop) != PROP_FLOAT ||
      RNA_property_subtype(prop) != PROP_MATRIX || RNA_property_array_length(ptr, prop) != 16)
  {
    RNA_warning("4x4 Matrix property not found: %s.%s",
                RNA_struct_identifier(ptr->type),
                propname.c_str());
    return;
  }
  draw_matrix_template(*layout, *ptr, *prop);
}
