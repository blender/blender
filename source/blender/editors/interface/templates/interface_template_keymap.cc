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

#include "UI_interface.hh"
#include "interface_intern.hh"

static void keymap_item_modified(bContext * /*C*/, void *kmi_p, void * /*unused*/)
{
  wmKeyMapItem *kmi = (wmKeyMapItem *)kmi_p;
  WM_keyconfig_update_tag(nullptr, kmi);
  U.runtime.is_dirty = true;
}

static void template_keymap_item_properties(uiLayout *layout, const char *title, PointerRNA *ptr)
{
  uiItemS(layout);

  if (title) {
    uiItemL(layout, title, ICON_NONE);
  }

  uiLayout *flow = uiLayoutColumnFlow(layout, 2, false);

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

    uiLayout *box = uiLayoutBox(flow);
    uiLayoutSetActive(box, is_set);
    uiLayout *row = uiLayoutRow(box, false);

    /* property value */
    uiItemFullR(row, ptr, prop, -1, 0, UI_ITEM_NONE, std::nullopt, ICON_NONE);

    if (is_set) {
      /* unset operator */
      uiBlock *block = uiLayoutGetBlock(row);
      UI_block_emboss_set(block, UI_EMBOSS_NONE);
      but = uiDefIconButO(block,
                          UI_BTYPE_BUT,
                          "UI_OT_unset_property_button",
                          WM_OP_EXEC_DEFAULT,
                          ICON_X,
                          0,
                          0,
                          UI_UNIT_X,
                          UI_UNIT_Y,
                          nullptr);
      but->rnapoin = *ptr;
      but->rnaprop = prop;
      UI_block_emboss_set(block, UI_EMBOSS);
    }
  }
  RNA_STRUCT_END;
}

void uiTemplateKeymapItemProperties(uiLayout *layout, PointerRNA *ptr)
{
  PointerRNA propptr = RNA_pointer_get(ptr, "properties");

  if (propptr.data) {
    uiBut *but = static_cast<uiBut *>(uiLayoutGetBlock(layout)->buttons.last);

    WM_operator_properties_sanitize(&propptr, false);
    template_keymap_item_properties(layout, nullptr, &propptr);

    /* attach callbacks to compensate for missing properties update,
     * we don't know which keymap (item) is being modified there */
    for (; but; but = but->next) {
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
