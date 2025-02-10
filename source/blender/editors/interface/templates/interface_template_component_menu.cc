/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include "BLI_string.h"
#include "BLI_string_ref.hh"

#include "RNA_access.hh"

#include "UI_interface.hh"
#include "interface_intern.hh"

using blender::StringRef;
using blender::StringRefNull;

struct ComponentMenuArgs {
  PointerRNA ptr;
  char propname[64]; /* XXX arbitrary */
};
/* NOTE: this is a block-menu, needs 0 events, otherwise the menu closes */
static uiBlock *component_menu(bContext *C, ARegion *region, void *args_v)
{
  ComponentMenuArgs *args = (ComponentMenuArgs *)args_v;

  uiBlock *block = UI_block_begin(C, region, __func__, UI_EMBOSS);
  UI_block_flag_enable(block, UI_BLOCK_KEEP_OPEN);

  uiLayout *layout = uiLayoutColumn(UI_block_layout(block,
                                                    UI_LAYOUT_VERTICAL,
                                                    UI_LAYOUT_PANEL,
                                                    0,
                                                    0,
                                                    UI_UNIT_X * 6,
                                                    UI_UNIT_Y,
                                                    0,
                                                    UI_style_get()),
                                    false);

  uiItemR(layout, &args->ptr, args->propname, UI_ITEM_R_EXPAND, "", ICON_NONE);

  UI_block_bounds_set_normal(block, 0.3f * U.widget_unit);
  UI_block_direction_set(block, UI_DIR_DOWN);

  return block;
}
void uiTemplateComponentMenu(uiLayout *layout,
                             PointerRNA *ptr,
                             const StringRefNull propname,
                             const StringRef name)
{
  ComponentMenuArgs *args = MEM_new<ComponentMenuArgs>(__func__);

  args->ptr = *ptr;
  STRNCPY(args->propname, propname.c_str());

  uiBlock *block = uiLayoutGetBlock(layout);
  UI_block_align_begin(block);

  uiBut *but = uiDefBlockButN(block,
                              component_menu,
                              args,
                              name,
                              0,
                              0,
                              UI_UNIT_X * 6,
                              UI_UNIT_Y,
                              "",
                              but_func_argN_free<ComponentMenuArgs>,
                              but_func_argN_copy<ComponentMenuArgs>);
  /* set rna directly, uiDefBlockButN doesn't do this */
  but->rnapoin = *ptr;
  but->rnaprop = RNA_struct_find_property(ptr, propname.c_str());
  but->rnaindex = 0;

  UI_block_align_end(block);
}
