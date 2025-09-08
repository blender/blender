/* SPDX-FileCopyrightText: 2018 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shader_fx
 */

#include "DNA_screen_types.h"

#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "BKE_context.hh"
#include "BKE_idtype.hh"
#include "BKE_modifier.hh"
#include "BKE_screen.hh"
#include "BKE_shader_fx.h"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "RNA_access.hh"

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

  layout->use_property_split_set(true);

  layout->prop(ptr, "mode", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  layout->prop(ptr, "threshold", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  if (mode == eShaderFxGlowMode_Color) {
    layout->prop(ptr, "select_color", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }

  layout->prop(ptr, "glow_color", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  layout->separator();

  layout->prop(ptr, "blend_mode", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout->prop(ptr, "opacity", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout->prop(ptr, "size", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout->prop(ptr, "rotation", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout->prop(ptr, "samples", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout->prop(ptr, "use_glow_under", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  shaderfx_panel_end(layout, ptr);
}

static void panel_register(ARegionType *region_type)
{
  shaderfx_panel_register(region_type, eShaderFxType_Glow, panel_draw);
}

static void foreach_working_space_color(ShaderFxData *fx,
                                        const IDTypeForeachColorFunctionCallback &fn)
{
  GlowShaderFxData *gpfx = (GlowShaderFxData *)fx;
  fn.single(gpfx->glow_color);
  fn.single(gpfx->select_color);
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
    /*foreach_working_space_color*/ foreach_working_space_color,
    /*panel_register*/ panel_register,
};
