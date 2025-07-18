/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include "BKE_context.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "WM_api.hh"

#include "UI_interface_layout.hh"
#include "interface_intern.hh"

static void keymap_item_modified(bContext * /*C*/, void *kmi_p, void * /*unused*/)
{
  wmKeyMapItem *kmi = (wmKeyMapItem *)kmi_p;
  WM_keyconfig_update_tag(nullptr, kmi);
  U.runtime.is_dirty = true;
}

static void template_keymap_item_properties(uiLayout *layout, const char *title, PointerRNA *ptr)
{
  layout->separator();

  if (title) {
    layout->label(title, ICON_NONE);
  }

  uiLayout *flow = &layout->column_flow(2, false);

  RNA_STRUCT_BEGIN_SKIP_RNA_TYPE (ptr, prop) {
    const bool is_set = RNA_property_is_set(ptr, prop);
    uiBut *but;

    /* recurse for nested properties */
    if (RNA_property_type(prop) == PROP_POINTER) {
      PointerRNA propptr = RNA_property_pointer_get(ptr, prop);

      if (propptr.data && RNA_struct_is_a(propptr.type, &RNA_OperatorProperties)) {
        const char *name = RNA_property_ui_name(prop);
        template_keymap_item_properties(layout, name, &propptr);
        continue;
      }
    }

    uiLayout *box = &flow->box();
    box->active_set(is_set);
    uiLayout *row = &box->row(false);

    /* property value */
    row->prop(ptr, prop, -1, 0, UI_ITEM_NONE, std::nullopt, ICON_NONE);

    if (is_set) {
      /* unset operator */
      uiBlock *block = row->block();
      UI_block_emboss_set(block, blender::ui::EmbossType::None);
      but = uiDefIconButO(block,
                          ButType::But,
                          "UI_OT_unset_property_button",
                          blender::wm::OpCallContext::ExecDefault,
                          ICON_X,
                          0,
                          0,
                          UI_UNIT_X,
                          UI_UNIT_Y,
                          std::nullopt);
      but->rnapoin = *ptr;
      but->rnaprop = prop;
      UI_block_emboss_set(block, blender::ui::EmbossType::Emboss);
    }
  }
  RNA_STRUCT_END;
}

void uiTemplateKeymapItemProperties(uiLayout *layout, PointerRNA *ptr)
{
  PointerRNA propptr = RNA_pointer_get(ptr, "properties");

  if (propptr.data) {
    uiBlock *block = layout->block();
    int i = layout->block()->buttons.size() - 1;

    WM_operator_properties_sanitize(&propptr, false);
    template_keymap_item_properties(layout, nullptr, &propptr);
    if (i < 0) {
      return;
    }
    /* attach callbacks to compensate for missing properties update,
     * we don't know which keymap (item) is being modified there */
    for (; i < block->buttons.size(); i++) {
      uiBut *but = block->buttons[i].get();
      /* operator buttons may store props for use (file selector, #36492) */
      if (but->rnaprop) {
        UI_but_func_set(but, keymap_item_modified, ptr->data, nullptr);

        /* Otherwise the keymap will be re-generated which we're trying to edit,
         * see: #47685 */
        UI_but_flag_enable(but, UI_BUT_UPDATE_DELAY);
      }
    }
  }
}
