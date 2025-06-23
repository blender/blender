/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include "BLI_string_ref.hh"

#include "BLT_translation.hh"

#include "WM_keymap.hh"

#include "UI_interface_layout.hh"
#include "interface_intern.hh"

using blender::StringRefNull;

static const wmKeyMapItem *keymap_item_from_enum_item(const wmKeyMap *keymap,
                                                      const EnumPropertyItem *item)
{
  if (item == nullptr) {
    return nullptr;
  }

  for (wmKeyMapItem *kmi = static_cast<wmKeyMapItem *>(keymap->items.first); kmi; kmi = kmi->next)
  {
    if (kmi->propvalue == item->value) {
      return kmi;
    }
  }

  return nullptr;
}

static bool keymap_item_can_collapse(const wmKeyMapItem *kmi_a, const wmKeyMapItem *kmi_b)
{
  return (kmi_a->shift == kmi_b->shift && kmi_a->ctrl == kmi_b->ctrl && kmi_a->alt == kmi_b->alt &&
          kmi_a->oskey == kmi_b->oskey && kmi_a->hyper == kmi_b->hyper);
}

int uiTemplateStatusBarModalItem(uiLayout *layout,
                                 const wmKeyMap *keymap,
                                 const EnumPropertyItem *item)
{
  const wmKeyMapItem *kmi = keymap_item_from_enum_item(keymap, item);
  if (kmi == nullptr) {
    return 0;
  }

  if ((kmi->val == KM_RELEASE) && ISKEYBOARD(kmi->type)) {
    /* Assume release events just disable something which was toggled on. */
    return 1;
  }

  /* Try to merge some known triplet items to save horizontal space. */
  const EnumPropertyItem *item_y = (item[1].identifier) ? item + 1 : nullptr;
  const EnumPropertyItem *item_z = (item_y && item[2].identifier) ? item + 2 : nullptr;
  const wmKeyMapItem *kmi_y = keymap_item_from_enum_item(keymap, item_y);
  const wmKeyMapItem *kmi_z = keymap_item_from_enum_item(keymap, item_z);

  if (kmi_y && kmi_z && keymap_item_can_collapse(kmi, kmi_y) &&
      keymap_item_can_collapse(kmi_y, kmi_z))
  {
    const char *xyz_label = nullptr;

    if (STREQ(item->identifier, "AXIS_X") && STREQ(item_y->identifier, "AXIS_Y") &&
        STREQ(item_z->identifier, "AXIS_Z"))
    {
      xyz_label = IFACE_("Axis");
    }
    else if (STREQ(item->identifier, "PLANE_X") && STREQ(item_y->identifier, "PLANE_Y") &&
             STREQ(item_z->identifier, "PLANE_Z"))
    {
      xyz_label = IFACE_("Plane");
    }

    if (STREQ(item->identifier, "PROPORTIONAL_SIZE_UP") &&
        STREQ(item_y->identifier, "PROPORTIONAL_SIZE_DOWN") &&
        STREQ(item_z->identifier, "PROPORTIONAL_SIZE"))
    {
      xyz_label = IFACE_("Proportional Size");
    }

    if (xyz_label) {
      int icon_mod[KM_MOD_NUM] = {0};
#ifdef WITH_HEADLESS
      int icon = 0;
#else
      int icon = UI_icon_from_keymap_item(kmi, icon_mod);
#endif
      for (int j = 0; j < ARRAY_SIZE(icon_mod) && icon_mod[j]; j++) {
        layout->label("", icon_mod[j]);
        const float offset = ui_event_icon_offset(icon_mod[j]);
        if (offset != 0.0f) {
          layout->separator(offset);
        }
      }
      layout->label("", icon);
      layout->separator(ui_event_icon_offset(icon));

#ifndef WITH_HEADLESS
      icon = UI_icon_from_keymap_item(kmi_y, icon_mod);
#endif
      layout->label("", icon);
      layout->separator(ui_event_icon_offset(icon));

#ifndef WITH_HEADLESS
      icon = UI_icon_from_keymap_item(kmi_z, icon_mod);
#endif
      layout->label("", icon);
      layout->separator(ui_event_icon_offset(icon));
      layout->separator(0.2f);
      layout->label(xyz_label, ICON_NONE);
      layout->separator(0.6f);
      return 3;
    }
  }

  /* Try to merge some known paired items to save horizontal space. */
  if (kmi_y && keymap_item_can_collapse(kmi, kmi_y)) {
    const char *ab_label = nullptr;

    if (STREQ(item->identifier, "PROPORTIONAL_SIZE_UP") &&
        STREQ(item_y->identifier, "PROPORTIONAL_SIZE_DOWN"))
    {
      ab_label = IFACE_("Proportional Size");
    }

    if (ab_label) {
      int icon_mod[KM_MOD_NUM] = {0};
#ifdef WITH_HEADLESS
      int icon = 0;
#else
      int icon = UI_icon_from_keymap_item(kmi, icon_mod);
#endif
      for (int j = 0; j < ARRAY_SIZE(icon_mod) && icon_mod[j]; j++) {
        layout->label("", icon_mod[j]);
        const float offset = ui_event_icon_offset(icon_mod[j]);
        if (offset != 0.0f) {
          layout->separator(offset);
        }
      }
      layout->label("", icon);
      layout->separator(ui_event_icon_offset(icon));

#ifndef WITH_HEADLESS
      icon = UI_icon_from_keymap_item(kmi_y, icon_mod);
#endif
      layout->label("", icon);
      layout->separator(ui_event_icon_offset(icon));
      layout->separator(0.2f);
      layout->label(ab_label, ICON_NONE);
      layout->separator(0.6f);
      return 2;
    }
  }

  /* Single item without merging. */
  return uiTemplateEventFromKeymapItem(layout, item->name, kmi, false) ? 1 : 0;
}

bool uiTemplateEventFromKeymapItem(uiLayout *layout,
                                   const StringRefNull text,
                                   const wmKeyMapItem *kmi,
                                   bool text_fallback)
{
  bool ok = false;

  int icon_mod[KM_MOD_NUM];
#ifdef WITH_HEADLESS
  int icon = 0;
#else
  const int icon = UI_icon_from_keymap_item(kmi, icon_mod);
#endif
  if (icon != 0) {
    for (int j = 0; j < ARRAY_SIZE(icon_mod) && icon_mod[j]; j++) {
      layout->label("", icon_mod[j]);
      const float offset = ui_event_icon_offset(icon_mod[j]);
      if (offset != 0.0f) {
        layout->separator(offset);
      }
    }

    /* Icon and text separately is closer together with aligned layout. */

    layout->label("", icon);
    if (icon >= ICON_MOUSE_LMB && icon <= ICON_MOUSE_MMB_SCROLL) {
      /* Negative space after narrow mice icons. */
      layout->separator(-0.68f);
    }

    const float offset = ui_event_icon_offset(icon);
    if (offset != 0.0f) {
      layout->separator(offset);
    }

    layout->separator(0.2f);
    layout->label(CTX_IFACE_(BLT_I18NCONTEXT_ID_WINDOWMANAGER, text.c_str()), ICON_NONE);
    layout->separator(0.6f);
    ok = true;
  }
  else if (text_fallback) {
    const char *event_text = WM_key_event_string(kmi->type, true);
    layout->label(event_text, ICON_NONE);
    layout->label(CTX_IFACE_(BLT_I18NCONTEXT_ID_WINDOWMANAGER, text.c_str()), ICON_NONE);
    layout->separator(0.6f);
    ok = true;
  }
  return ok;
}
