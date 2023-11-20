/* SPDX-FileCopyrightText: 2017 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shader_fx
 */

#include <cstdio>

#include "DNA_gpencil_legacy_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "BKE_context.hh"
#include "BKE_screen.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "RNA_access.hh"

#include "FX_shader_types.h"
#include "FX_ui_common.h"

static void init_data(ShaderFxData *fx)
{
  FlipShaderFxData *gpfx = (FlipShaderFxData *)fx;
  gpfx->flag |= FX_FLIP_HORIZONTAL;
}

static void copy_data(const ShaderFxData *md, ShaderFxData *target)
{
  BKE_shaderfx_copydata_generic(md, target);
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *row;
  uiLayout *layout = panel->layout;
  const eUI_Item_Flag toggles_flag = UI_ITEM_R_TOGGLE | UI_ITEM_R_FORCE_BLANK_DECORATE;

  PointerRNA *ptr = shaderfx_panel_get_property_pointers(panel, nullptr);

  uiLayoutSetPropSep(layout, true);

  row = uiLayoutRowWithHeading(layout, true, IFACE_("Axis"));
  uiItemR(row, ptr, "use_flip_x", toggles_flag, nullptr, ICON_NONE);
  uiItemR(row, ptr, "use_flip_y", toggles_flag, nullptr, ICON_NONE);

  shaderfx_panel_end(layout, ptr);
}

static void panel_register(ARegionType *region_type)
{
  shaderfx_panel_register(region_type, eShaderFxType_Flip, panel_draw);
}

ShaderFxTypeInfo shaderfx_Type_Flip = {
    /*name*/ N_("Flip"),
    /*struct_name*/ "FlipShaderFxData",
    /*struct_size*/ sizeof(FlipShaderFxData),
    /*type*/ eShaderFxType_GpencilType,
    /*flags*/ ShaderFxTypeFlag(0),

    /*copy_data*/ copy_data,

    /*init_data*/ init_data,
    /*free_data*/ nullptr,
    /*is_disabled*/ nullptr,
    /*update_depsgraph*/ nullptr,
    /*depends_on_time*/ nullptr,
    /*foreach_ID_link*/ nullptr,
    /*panel_register*/ panel_register,
};
