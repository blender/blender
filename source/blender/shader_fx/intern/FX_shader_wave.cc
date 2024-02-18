/* SPDX-FileCopyrightText: 2018 Blender Authors
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

#include "BKE_context.hh"
#include "BKE_screen.hh"

#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "RNA_access.hh"

#include "FX_shader_types.h"
#include "FX_ui_common.h"

static void init_data(ShaderFxData *fx)
{
  WaveShaderFxData *gpfx = (WaveShaderFxData *)fx;
  gpfx->amplitude = 10.0f;
  gpfx->period = 20.0f;
  gpfx->phase = 0.0f;
  gpfx->orientation = 1;
}

static void copy_data(const ShaderFxData *md, ShaderFxData *target)
{
  BKE_shaderfx_copydata_generic(md, target);
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = shaderfx_panel_get_property_pointers(panel, nullptr);

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, ptr, "orientation", UI_ITEM_R_EXPAND, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "amplitude", UI_ITEM_NONE, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "period", UI_ITEM_NONE, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "phase", UI_ITEM_NONE, nullptr, ICON_NONE);

  shaderfx_panel_end(layout, ptr);
}

static void panel_register(ARegionType *region_type)
{
  shaderfx_panel_register(region_type, eShaderFxType_Wave, panel_draw);
}

ShaderFxTypeInfo shaderfx_Type_Wave = {
    /*name*/ N_("WaveDistortion"),
    /*struct_name*/ "WaveShaderFxData",
    /*struct_size*/ sizeof(WaveShaderFxData),
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
