/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include "BLI_string_ref.hh"

#include "RNA_access.hh"

#include "UI_interface.hh"
#include "interface_intern.hh"

using blender::StringRefNull;

struct IconViewMenuArgs {
  PointerRNA ptr;
  PropertyRNA *prop;
  bool show_labels;
  float icon_scale;
};

/* ID Search browse menu, open */
static uiBlock *ui_icon_view_menu_cb(bContext *C, ARegion *region, void *arg_litem)
{
  static IconViewMenuArgs args;

  /* arg_litem is malloced, can be freed by parent button */
  args = *((IconViewMenuArgs *)arg_litem);
  const int w = UI_UNIT_X * (args.icon_scale);
  const int h = UI_UNIT_X * (args.icon_scale + args.show_labels);

  uiBlock *block = UI_block_begin(C, region, "_popup", UI_EMBOSS_PULLDOWN);
  UI_block_flag_enable(block, UI_BLOCK_LOOP);
  UI_block_theme_style_set(block, UI_BLOCK_THEME_STYLE_POPUP);

  bool free;
  const EnumPropertyItem *item;
  RNA_property_enum_items(C, &args.ptr, args.prop, &item, nullptr, &free);

  for (int a = 0; item[a].identifier; a++) {
    const int x = (a % 8) * w;
    const int y = -(a / 8) * h;

    const int icon = item[a].icon;
    const int value = item[a].value;
    uiBut *but;
    if (args.show_labels) {
      but = uiDefIconTextButR_prop(block,
                                   UI_BTYPE_ROW,
                                   0,
                                   icon,
                                   item[a].name,
                                   x,
                                   y,
                                   w,
                                   h,
                                   &args.ptr,
                                   args.prop,
                                   -1,
                                   0,
                                   value,
                                   nullptr);
    }
    else {
      but = uiDefIconButR_prop(
          block, UI_BTYPE_ROW, 0, icon, x, y, w, h, &args.ptr, args.prop, -1, 0, value, nullptr);
    }
    ui_def_but_icon(but, icon, UI_HAS_ICON | UI_BUT_ICON_PREVIEW);
  }

  UI_block_bounds_set_normal(block, 0.3f * U.widget_unit);
  UI_block_direction_set(block, UI_DIR_DOWN);

  if (free) {
    MEM_freeN((void *)item);
  }

  return block;
}

void uiTemplateIcon(uiLayout *layout, int icon_value, float icon_scale)
{
  uiBlock *block = uiLayoutAbsoluteBlock(layout);
  uiBut *but = uiDefIconBut(block,
                            UI_BTYPE_LABEL,
                            0,
                            ICON_X,
                            0,
                            0,
                            UI_UNIT_X * icon_scale,
                            UI_UNIT_Y * icon_scale,
                            nullptr,
                            0.0,
                            0.0,
                            "");
  ui_def_but_icon(but, icon_value, UI_HAS_ICON | UI_BUT_ICON_PREVIEW);
}

void uiTemplateIconView(uiLayout *layout,
                        PointerRNA *ptr,
                        const StringRefNull propname,
                        bool show_labels,
                        float icon_scale,
                        float icon_scale_popup)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, propname.c_str());

  if (!prop || RNA_property_type(prop) != PROP_ENUM) {
    RNA_warning("property of type Enum not found: %s.%s",
                RNA_struct_identifier(ptr->type),
                propname.c_str());
    return;
  }

  uiBlock *block = uiLayoutAbsoluteBlock(layout);

  int tot_items;
  bool free_items;
  const EnumPropertyItem *items;
  RNA_property_enum_items(
      static_cast<bContext *>(block->evil_C), ptr, prop, &items, &tot_items, &free_items);
  const int value = RNA_property_enum_get(ptr, prop);
  int icon = ICON_NONE;
  RNA_enum_icon_from_value(items, value, &icon);

  uiBut *but;
  if (RNA_property_editable(ptr, prop)) {
    IconViewMenuArgs *cb_args = MEM_new<IconViewMenuArgs>(__func__);
    cb_args->ptr = *ptr;
    cb_args->prop = prop;
    cb_args->show_labels = show_labels;
    cb_args->icon_scale = icon_scale_popup;

    but = uiDefBlockButN(block,
                         ui_icon_view_menu_cb,
                         cb_args,
                         "",
                         0,
                         0,
                         UI_UNIT_X * icon_scale,
                         UI_UNIT_Y * icon_scale,
                         "",
                         but_func_argN_free<IconViewMenuArgs>,
                         but_func_argN_copy<IconViewMenuArgs>);
  }
  else {
    but = uiDefIconBut(block,
                       UI_BTYPE_LABEL,
                       0,
                       ICON_X,
                       0,
                       0,
                       UI_UNIT_X * icon_scale,
                       UI_UNIT_Y * icon_scale,
                       nullptr,
                       0.0,
                       0.0,
                       "");
  }

  ui_def_but_icon(but, icon, UI_HAS_ICON | UI_BUT_ICON_PREVIEW);

  if (free_items) {
    MEM_freeN((void *)items);
  }
}
