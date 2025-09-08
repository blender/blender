/* SPDX-FileCopyrightText: 2018 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shader_fx
 */

#include "BKE_context.hh"
#include "BKE_idtype.hh"
#include "BKE_screen.hh"

#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "DNA_screen_types.h"
#include "DNA_shader_fx_types.h"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "RNA_access.hh"

#include "FX_shader_types.h"
#include "FX_ui_common.h"

static void init_data(ShaderFxData *fx)
{
  ColorizeShaderFxData *gpfx = (ColorizeShaderFxData *)fx;
  ARRAY_SET_ITEMS(gpfx->low_color, 0.0f, 0.0f, 0.0f, 1.0f);
  ARRAY_SET_ITEMS(gpfx->high_color, 1.0f, 1.0f, 1.0f, 1.0f);
  gpfx->mode = eShaderFxColorizeMode_GrayScale;
  gpfx->factor = 0.5f;
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

  if (ELEM(mode, eShaderFxColorizeMode_Custom, eShaderFxColorizeMode_Duotone)) {
    const char *text = (mode == eShaderFxColorizeMode_Duotone) ? IFACE_("Low Color") :
                                                                 IFACE_("Color");
    layout->prop(ptr, "low_color", UI_ITEM_NONE, text, ICON_NONE);
  }
  if (mode == eShaderFxColorizeMode_Duotone) {
    layout->prop(ptr, "high_color", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }

  layout->prop(ptr, "factor", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  shaderfx_panel_end(layout, ptr);
}

static void panel_register(ARegionType *region_type)
{
  shaderfx_panel_register(region_type, eShaderFxType_Colorize, panel_draw);
}

static void foreach_working_space_color(ShaderFxData *fx,
                                        const IDTypeForeachColorFunctionCallback &fn)
{
  ColorizeShaderFxData *gpfx = (ColorizeShaderFxData *)fx;
  fn.single(gpfx->low_color);
  fn.single(gpfx->high_color);
}

ShaderFxTypeInfo shaderfx_Type_Colorize = {
    /*name*/ N_("Colorize"),
    /*struct_name*/ "ColorizeShaderFxData",
    /*struct_size*/ sizeof(ColorizeShaderFxData),
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
