/* SPDX-FileCopyrightText: 2025-2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include "interface_intern.hh"

#include <optional>
#include <string>

#include "BLI_set.hh"
#include "BLI_string.h"
#include "BLI_vector.hh"

#include "BKE_context.hh"
#include "BKE_screen.hh"

#include "RNA_access.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "IMB_colormanagement.hh"

#include "BLT_translation.hh"

namespace blender::ui {

/* -------------------------------------------------------------------- */
/** \name Color Space Menu
 * \{ */

const char *COLORSPACE_MENU_ID = "UI_MT_color_space_select";

static std::optional<StringRefNull> colorspace_rna_details_from_context(const bContext *C,
                                                                        PointerRNA *ptr,
                                                                        PropertyRNA **prop)
{
  std::optional<StringRefNull> parent_path = CTX_data_string_get(C, "colorspace_parent_path");
  if (!parent_path) {
    /* This is the first time called, so we use the active button to get the RNA properties */
    int unused_index = 0;
    context_active_but_prop_get(C, ptr, prop, &unused_index);
    return std::nullopt;
  }

  /* Use the stored properties in the bContext */
  std::optional<StringRefNull> prop_name = CTX_data_string_get(C, "colorspace_pointer_prop");
  *ptr = CTX_data_pointer_get(C, "colorspace_pointer_rna");
  *prop = RNA_struct_find_property(ptr, prop_name->c_str());
  return parent_path;
}

static void colorspaces_menu_add_button(ui::Layout &layout,
                                        PointerRNA ptr,
                                        PropertyRNA *prop,
                                        const char *identifier,
                                        const char *name,
                                        const char *description)
{
  Block *block = layout.block();
  Button *but = uiDefIconTextBut(block,
                                 ButtonType::But,
                                 ICON_BLANK1,
                                 name,
                                 0,
                                 0,
                                 layout.width(),
                                 UI_UNIT_Y,
                                 nullptr,
                                 std::nullopt);

  const std::string prop_name = RNA_property_identifier(prop);
  const int index = IMB_colormanagement_colorspace_get_named_index(identifier);

  button_func_set(but, [ptr, prop_name, index](bContext &C) mutable {
    PropertyRNA *prop = RNA_struct_find_property(&ptr, prop_name.c_str());
    if (prop) {
      RNA_property_enum_set(&ptr, prop, index);
      RNA_property_update(&C, &ptr, prop);
    }
  });

  if (description && description[0]) {
    button_func_quick_tooltip_set(
        but, [tip = std::string(description)](const Button *) { return tip; });
  }
}

static void colorspaces_menu_draw(const bContext *C, Menu *menu)
{
  /* Get access to the RNA object who we are drawing */
  PointerRNA ptr;
  PropertyRNA *prop = nullptr;
  std::optional<StringRefNull> parent_path = colorspace_rna_details_from_context(C, &ptr, &prop);

  /* In case menu is invoked from invalid context. */
  if (ptr.data == nullptr || prop == nullptr) {
    return;
  }

  /* Loop through items in enum */
  const EnumPropertyItem *item_array = nullptr;
  int item_len = 0;
  bool free_items = false;
  RNA_property_enum_items(nullptr, &ptr, prop, &item_array, &item_len, &free_items);

  Set<StringRef> subdirs_set;
  Vector<const ColorSpace *> colorspaces_at_this_level;

  for (int i = 0; i < item_len; ++i) {
    const ColorSpace *cs = IMB_colormanagement_space_get_named(item_array[i].identifier);
    if (!cs) {
      continue;
    }
    const char *family_cstr = IMB_colormanagement_colorspace_get_family(cs);
    StringRef remaining(family_cstr ? family_cstr : "");

    if (parent_path) {
      if (!remaining.startswith(*parent_path)) {
        continue;
      }
      /* Strip the parent_path prefix. */
      remaining = remaining.drop_known_prefix(*parent_path);
    }

    if (remaining.is_empty()) {
      colorspaces_at_this_level.append(cs);
      continue;
    }

    /* Skip sibling items with similar prefix. */
    int64_t pos = remaining.find_first_not_of('/');
    if (pos == 0) {
      if (parent_path) {
        continue;
      }
    }
    else if (pos != StringRef::not_found) {
      /* New subdir, so erase leading slash. */
      remaining = remaining.drop_prefix(pos);
    }

    /* Found a new subdir. Remove any paths further down and add it. */
    pos = remaining.find_first_of('/');
    if (pos != StringRef::not_found) {
      remaining = remaining.substr(0, pos);
    }
    if (!remaining.is_empty()) {
      subdirs_set.add(remaining);
    }
  }

  Vector<StringRef> subdirs(subdirs_set.begin(), subdirs_set.end());
  std::ranges::sort(subdirs);

  std::ranges::sort(colorspaces_at_this_level, [](const ColorSpace *a, const ColorSpace *b) {
    return StringRef(IMB_colormanagement_colorspace_get_name(a)) <
           StringRef(IMB_colormanagement_colorspace_get_name(b));
  });

  Layout &col = menu->layout->column(false);

  /* Set context variables so they can be re-used in submenus. */
  const std::string root = (parent_path) ? std::string(parent_path->c_str()) + "/" : "";
  col.context_string_set("colorspace_parent_path", root);
  col.context_ptr_set("colorspace_pointer_rna", &ptr);
  col.context_string_set("colorspace_pointer_prop", RNA_property_identifier(prop));

  for (const StringRef &dir : subdirs) {
    std::string path = root + std::string(dir);
    col.context_string_set("colorspace_parent_path", path);
    col.menu(COLORSPACE_MENU_ID, dir, ICON_NONE);
  }

  if (!subdirs.is_empty() && !colorspaces_at_this_level.is_empty()) {
    col.separator();
  }

  for (const ColorSpace *cs : colorspaces_at_this_level) {
    colorspaces_menu_add_button(col,
                                ptr,
                                prop,
                                IMB_colormanagement_colorspace_get_name(cs),
                                IMB_colormanagement_colorspace_get_name(cs),
                                IMB_colormanagement_colorspace_get_description(cs));
  }

  if (!parent_path) {
    colorspaces_menu_add_button(col,
                                ptr,
                                prop,
                                "scene_linear",
                                IFACE_("Working Space"),
                                TIP_("Working color space of the current file"));
  }

  if (free_items) {
    MEM_delete(const_cast<EnumPropertyItem *>(item_array));
  }
}

MenuType *UI_MT_color_space_select()
{
  MenuType *type = MEM_new_zeroed<MenuType>(__func__);
  STRNCPY(type->idname, COLORSPACE_MENU_ID);
  type->draw = colorspaces_menu_draw;
  type->flag = MenuTypeFlag::ContextDependent;
  return type;
}

/** \} */

}  // namespace blender::ui
