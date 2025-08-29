/* SPDX-FileCopyrightText: 2018 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shader_fx
 */

#include "DNA_screen_types.h"
#include "DNA_shader_fx_types.h"

#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "BKE_context.hh"
#include "BKE_idtype.hh"
#include "BKE_screen.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "RNA_access.hh"

#include "FX_shader_types.h"
#include "FX_ui_common.h"

static void init_data(ShaderFxData *fx)
{
  RimShaderFxData *gpfx = (RimShaderFxData *)fx;
  ARRAY_SET_ITEMS(gpfx->offset, 50, -100);
  ARRAY_SET_ITEMS(gpfx->rim_rgb, 1.0f, 1.0f, 0.5f);
  ARRAY_SET_ITEMS(gpfx->mask_rgb, 0.0f, 0.0f, 0.0f);
  gpfx->mode = eShaderFxRimMode_Overlay;

  ARRAY_SET_ITEMS(gpfx->blur, 0, 0);
  gpfx->samples = 2;
}

static void copy_data(const ShaderFxData *md, ShaderFxData *target)
{
  BKE_shaderfx_copydata_generic(md, target);
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *col;
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = shaderfx_panel_get_property_pointers(panel, nullptr);

  layout->use_property_split_set(true);

  layout->prop(ptr, "rim_color", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout->prop(ptr, "mask_color", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout->prop(ptr, "mode", UI_ITEM_NONE, IFACE_("Blend Mode"), ICON_NONE);

  /* Add the X, Y labels manually because offset is a #PROP_PIXEL. */
  col = &layout->column(true);
  PropertyRNA *prop = RNA_struct_find_property(ptr, "offset");
  col->prop(ptr, prop, 0, 0, UI_ITEM_NONE, IFACE_("Offset X"), ICON_NONE);
  col->prop(ptr, prop, 1, 0, UI_ITEM_NONE, IFACE_("Y"), ICON_NONE);

  shaderfx_panel_end(layout, ptr);
}

static void blur_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *col;
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = shaderfx_panel_get_property_pointers(panel, nullptr);

  layout->use_property_split_set(true);

  /* Add the X, Y labels manually because blur is a #PROP_PIXEL. */
  col = &layout->column(true);
  PropertyRNA *prop = RNA_struct_find_property(ptr, "blur");
  col->prop(ptr, prop, 0, 0, UI_ITEM_NONE, IFACE_("Blur X"), ICON_NONE);
  col->prop(ptr, prop, 1, 0, UI_ITEM_NONE, IFACE_("Y"), ICON_NONE);

  layout->prop(ptr, "samples", UI_ITEM_NONE, std::nullopt, ICON_NONE);
}

static void panel_register(ARegionType *region_type)
{
  PanelType *panel_type = shaderfx_panel_register(region_type, eShaderFxType_Rim, panel_draw);
  shaderfx_subpanel_register(region_type, "blur", "Blur", nullptr, blur_panel_draw, panel_type);
}

static void foreach_working_space_color(ShaderFxData *fx,
                                        const IDTypeForeachColorFunctionCallback &fn)
{
  RimShaderFxData *gpfx = (RimShaderFxData *)fx;
  fn.single(gpfx->rim_rgb);
  fn.single(gpfx->mask_rgb);
}

ShaderFxTypeInfo shaderfx_Type_Rim = {
    /*name*/ N_("Rim"),
    /*struct_name*/ "RimShaderFxData",
    /*struct_size*/ sizeof(RimShaderFxData),
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
