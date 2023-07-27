/* SPDX-FileCopyrightText: 2018 Blender Foundation
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

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "BKE_context.h"
#include "BKE_modifier.h"
#include "BKE_screen.h"
#include "BKE_shader_fx.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"

#include "FX_shader_types.h"
#include "FX_ui_common.h"

static void init_data(ShaderFxData *md)
{
  GlowShaderFxData *gpfx = (GlowShaderFxData *)md;
  ARRAY_SET_ITEMS(gpfx->glow_color, 0.75f, 1.0f, 1.0f, 1.0f);
  ARRAY_SET_ITEMS(gpfx->select_color, 0.0f, 0.0f, 0.0f);
  copy_v2_fl(gpfx->blur, 50.0f);
  gpfx->threshold = 0.1f;
  gpfx->samples = 8;
}

static void copy_data(const ShaderFxData *md, ShaderFxData *target)
{
  BKE_shaderfx_copydata_generic(md, target);
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = shaderfx_panel_get_property_pointers(panel, nullptr);

  int mode = RNA_enum_get(ptr, "mode");

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, ptr, "mode", 0, nullptr, ICON_NONE);

  uiItemR(layout, ptr, "threshold", 0, nullptr, ICON_NONE);
  if (mode == eShaderFxGlowMode_Color) {
    uiItemR(layout, ptr, "select_color", 0, nullptr, ICON_NONE);
  }

  uiItemR(layout, ptr, "glow_color", 0, nullptr, ICON_NONE);

  uiItemS(layout);

  uiItemR(layout, ptr, "blend_mode", 0, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "opacity", 0, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "size", 0, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "rotation", 0, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "samples", 0, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "use_glow_under", 0, nullptr, ICON_NONE);

  shaderfx_panel_end(layout, ptr);
}

static void panel_register(ARegionType *region_type)
{
  shaderfx_panel_register(region_type, eShaderFxType_Glow, panel_draw);
}

ShaderFxTypeInfo shaderfx_Type_Glow = {
    /*name*/ N_("Glow"),
    /*struct_name*/ "GlowShaderFxData",
    /*struct_size*/ sizeof(GlowShaderFxData),
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
