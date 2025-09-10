/* SPDX-FileCopyrightText: 2017 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shader_fx
 */

#include "BLI_math_vector.h"

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
  BlurShaderFxData *gpfx = (BlurShaderFxData *)fx;
  copy_v2_fl(gpfx->radius, 50.0f);
  gpfx->samples = 8;
  gpfx->rotation = 0.0f;
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

  layout->prop(ptr, "samples", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  layout->prop(ptr, "use_dof_mode", UI_ITEM_NONE, IFACE_("Use Depth of Field"), ICON_NONE);
  col = &layout->column(false);
  col->active_set(!RNA_boolean_get(ptr, "use_dof_mode"));
  col->prop(ptr, "size", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  col->prop(ptr, "rotation", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  shaderfx_panel_end(layout, ptr);
}

static void panel_register(ARegionType *region_type)
{
  shaderfx_panel_register(region_type, eShaderFxType_Blur, panel_draw);
}

ShaderFxTypeInfo shaderfx_Type_Blur = {
    /*name*/ N_("Blur"),
    /*struct_name*/ "BlurShaderFxData",
    /*struct_size*/ sizeof(BlurShaderFxData),
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
