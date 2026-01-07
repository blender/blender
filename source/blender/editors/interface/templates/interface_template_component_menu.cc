/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include "BLI_string.h"
#include "BLI_string_ref.hh"

#include "RNA_access.hh"

#include "UI_interface_layout.hh"
#include "interface_intern.hh"

namespace blender::ui {

struct ComponentMenuArgs {
  PointerRNA ptr;
  char propname[64]; /* XXX arbitrary */
};
/* NOTE: this is a block-menu, needs 0 events, otherwise the menu closes */
static Block *component_menu(bContext *C, ARegion *region, void *args_v)
{
  ComponentMenuArgs *args = static_cast<ComponentMenuArgs *>(args_v);

  Block *block = block_begin(C, region, __func__, EmbossType::Emboss);
  block_flag_enable(block, BLOCK_KEEP_OPEN);

  Layout &layout = block_layout(block,
                                LayoutDirection::Vertical,
                                LayoutType::Panel,
                                0,
                                0,
                                UI_UNIT_X * 6,
                                UI_UNIT_Y,
                                0,
                                style_get())
                       .column(false);

  layout.prop(&args->ptr, args->propname, ITEM_R_EXPAND, "", ICON_NONE);

  block_bounds_set_normal(block, 0.3f * U.widget_unit);
  block_direction_set(block, UI_DIR_DOWN);

  return block;
}
void template_component_menu(Layout *layout,
                             PointerRNA *ptr,
                             const StringRefNull propname,
                             const StringRef name)
{
  ComponentMenuArgs *args = MEM_new<ComponentMenuArgs>(__func__);

  args->ptr = *ptr;
  STRNCPY(args->propname, propname.c_str());

  Block *block = layout->block();
  block_align_begin(block);

  Button *but = uiDefBlockButN(block,
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

  block_align_end(block);
}

}  // namespace blender::ui
