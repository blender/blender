/* SPDX-FileCopyrightText: 2018 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shader_fx
 */

#include "BLT_translation.hh"

#include "BLI_math_base.h"

#include "BKE_context.hh"
#include "BKE_lib_query.hh"
#include "BKE_modifier.hh"
#include "BKE_screen.hh"
#include "BKE_shader_fx.h"

#include "DNA_screen_types.h"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "RNA_access.hh"

#include "FX_shader_types.h"
#include "FX_ui_common.h"

#include "DEG_depsgraph_build.hh"

static void init_data(ShaderFxData *md)
{
  SwirlShaderFxData *gpmd = (SwirlShaderFxData *)md;
  gpmd->radius = 100;
  gpmd->angle = M_PI_2;
}

static void copy_data(const ShaderFxData *md, ShaderFxData *target)
{
  BKE_shaderfx_copydata_generic(md, target);
}

static void update_depsgraph(ShaderFxData *fx, const ModifierUpdateDepsgraphContext *ctx)
{
  SwirlShaderFxData *fxd = (SwirlShaderFxData *)fx;
  if (fxd->object != nullptr) {
    DEG_add_object_relation(ctx->node, fxd->object, DEG_OB_COMP_TRANSFORM, "Swirl ShaderFx");
  }
  DEG_add_object_relation(ctx->node, ctx->object, DEG_OB_COMP_TRANSFORM, "Swirl ShaderFx");
}

static bool is_disabled(ShaderFxData *fx, bool /*use_render_params*/)
{
  SwirlShaderFxData *fxd = (SwirlShaderFxData *)fx;

  return !fxd->object;
}

static void foreach_ID_link(ShaderFxData *fx, Object *ob, IDWalkFunc walk, void *user_data)
{
  SwirlShaderFxData *fxd = (SwirlShaderFxData *)fx;

  walk(user_data, ob, (ID **)&fxd->object, IDWALK_CB_NOP);
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = shaderfx_panel_get_property_pointers(panel, nullptr);

  layout->use_property_split_set(true);

  layout->prop(ptr, "object", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout->prop(ptr, "radius", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout->prop(ptr, "angle", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  shaderfx_panel_end(layout, ptr);
}

static void panel_register(ARegionType *region_type)
{
  shaderfx_panel_register(region_type, eShaderFxType_Swirl, panel_draw);
}

ShaderFxTypeInfo shaderfx_Type_Swirl = {
    /*name*/ N_("Swirl"),
    /*struct_name*/ "SwirlShaderFxData",
    /*struct_size*/ sizeof(SwirlShaderFxData),
    /*type*/ eShaderFxType_GpencilType,
    /*flags*/ ShaderFxTypeFlag(0),

    /*copy_data*/ copy_data,

    /*init_data*/ init_data,
    /*free_data*/ nullptr,
    /*is_disabled*/ is_disabled,
    /*update_depsgraph*/ update_depsgraph,
    /*depends_on_time*/ nullptr,
    /*foreach_ID_link*/ foreach_ID_link,
    /*foreach_working_space_color*/ nullptr,
    /*panel_register*/ panel_register,
};
