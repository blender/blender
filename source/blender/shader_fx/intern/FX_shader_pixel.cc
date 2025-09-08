/* SPDX-FileCopyrightText: 2017 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shader_fx
 */

#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "BKE_context.hh"
#include "BKE_screen.hh"

#include "DNA_screen_types.h"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "RNA_access.hh"

#include "FX_shader_types.h"
#include "FX_ui_common.h"

static void init_data(ShaderFxData *fx)
{
  PixelShaderFxData *gpfx = (PixelShaderFxData *)fx;
  ARRAY_SET_ITEMS(gpfx->size, 5, 5);
  ARRAY_SET_ITEMS(gpfx->rgba, 0.0f, 0.0f, 0.0f, 0.9f);
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

  /* Add the X, Y labels manually because size is a #PROP_PIXEL. */
  col = &layout->column(true);
  PropertyRNA *prop = RNA_struct_find_property(ptr, "size");
  col->prop(ptr, prop, 0, 0, UI_ITEM_NONE, IFACE_("Size X"), ICON_NONE);
  col->prop(ptr, prop, 1, 0, UI_ITEM_NONE, IFACE_("Y"), ICON_NONE);

  layout->prop(ptr, "use_antialiasing", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  shaderfx_panel_end(layout, ptr);
}

static void panel_register(ARegionType *region_type)
{
  shaderfx_panel_register(region_type, eShaderFxType_Pixel, panel_draw);
}

ShaderFxTypeInfo shaderfx_Type_Pixel = {
    /*name*/ N_("Pixelate"),
    /*struct_name*/ "PixelShaderFxData",
    /*struct_size*/ sizeof(PixelShaderFxData),
    /*type*/ eShaderFxType_GpencilType,
    /*flags*/ ShaderFxTypeFlag(0),

    /*copy_data*/ copy_data,

    /*init_data*/ init_data,
    /*free_data*/ nullptr,
    /*is_disabled*/ nullptr,
    /*update_depsgraph*/ nullptr,
    /*depends_on_time*/ nullptr,
    /*foreach_ID_link*/ nullptr,
    /*foreach_working_space_color*/ nullptr,
    /*panel_register*/ panel_register,
};
