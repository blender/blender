/* SPDX-FileCopyrightText: 2018 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shader_fx
 */

#include "DNA_screen_types.h"

#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "BKE_context.hh"
#include "BKE_idtype.hh"
#include "BKE_lib_query.hh"
#include "BKE_modifier.hh"
#include "BKE_screen.hh"
#include "BKE_shader_fx.h"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "RNA_access.hh"

#include "FX_shader_types.h"
#include "FX_ui_common.h"

#include "DEG_depsgraph_build.hh"

static void init_data(ShaderFxData *md)
{
  ShadowShaderFxData *gpfx = (ShadowShaderFxData *)md;
  gpfx->rotation = 0.0f;
  ARRAY_SET_ITEMS(gpfx->offset, 15, 20);
  ARRAY_SET_ITEMS(gpfx->scale, 1.0f, 1.0f);
  ARRAY_SET_ITEMS(gpfx->shadow_rgba, 0.0f, 0.0f, 0.0f, 0.8f);

  gpfx->amplitude = 10.0f;
  gpfx->period = 20.0f;
  gpfx->phase = 0.0f;
  gpfx->orientation = 1;

  ARRAY_SET_ITEMS(gpfx->blur, 5, 5);
  gpfx->samples = 2;

  gpfx->object = nullptr;
}

static void copy_data(const ShaderFxData *md, ShaderFxData *target)
{
  BKE_shaderfx_copydata_generic(md, target);
}

static void update_depsgraph(ShaderFxData *fx, const ModifierUpdateDepsgraphContext *ctx)
{
  ShadowShaderFxData *fxd = (ShadowShaderFxData *)fx;
  if (fxd->object != nullptr) {
    DEG_add_object_relation(ctx->node, fxd->object, DEG_OB_COMP_TRANSFORM, "Shadow ShaderFx");
  }
  DEG_add_object_relation(ctx->node, ctx->object, DEG_OB_COMP_TRANSFORM, "Shadow ShaderFx");
}

static bool is_disabled(ShaderFxData *fx, bool /*use_render_params*/)
{
  ShadowShaderFxData *fxd = (ShadowShaderFxData *)fx;

  return (!fxd->object) && (fxd->flag & FX_SHADOW_USE_OBJECT);
}

static void foreach_ID_link(ShaderFxData *fx, Object *ob, IDWalkFunc walk, void *user_data)
{
  ShadowShaderFxData *fxd = (ShadowShaderFxData *)fx;

  walk(user_data, ob, (ID **)&fxd->object, IDWALK_CB_NOP);
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *row, *col;
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = shaderfx_panel_get_property_pointers(panel, nullptr);

  layout->use_property_split_set(true);

  layout->prop(ptr, "shadow_color", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  /* Add the X, Y labels manually because size is a #PROP_PIXEL. */
  col = &layout->column(true);
  PropertyRNA *prop = RNA_struct_find_property(ptr, "offset");
  col->prop(ptr, prop, 0, 0, UI_ITEM_NONE, IFACE_("Offset X"), ICON_NONE);
  col->prop(ptr, prop, 1, 0, UI_ITEM_NONE, IFACE_("Y"), ICON_NONE);

  layout->prop(ptr, "scale", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout->prop(ptr, "rotation", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  row = &layout->row(true, IFACE_("Object Pivot"));
  row->prop(ptr, "use_object", UI_ITEM_NONE, "", ICON_NONE);
  row->prop(ptr, "object", UI_ITEM_NONE, "", ICON_NONE);

  shaderfx_panel_end(layout, ptr);
}

static void blur_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *col;
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = shaderfx_panel_get_property_pointers(panel, nullptr);

  layout->use_property_split_set(true);

  /* Add the X, Y labels manually because size is a #PROP_PIXEL. */
  col = &layout->column(true);
  PropertyRNA *prop = RNA_struct_find_property(ptr, "blur");
  col->prop(ptr, prop, 0, 0, UI_ITEM_NONE, IFACE_("Blur X"), ICON_NONE);
  col->prop(ptr, prop, 1, 0, UI_ITEM_NONE, IFACE_("Y"), ICON_NONE);

  layout->prop(ptr, "samples", UI_ITEM_NONE, std::nullopt, ICON_NONE);
}

static void wave_header_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = shaderfx_panel_get_property_pointers(panel, nullptr);

  layout->prop(ptr, "use_wave", UI_ITEM_NONE, IFACE_("Wave Effect"), ICON_NONE);
}

static void wave_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = shaderfx_panel_get_property_pointers(panel, nullptr);

  layout->use_property_split_set(true);

  layout->active_set(RNA_boolean_get(ptr, "use_wave"));

  layout->prop(ptr, "orientation", UI_ITEM_R_EXPAND, std::nullopt, ICON_NONE);
  layout->prop(ptr, "amplitude", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout->prop(ptr, "period", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout->prop(ptr, "phase", UI_ITEM_NONE, std::nullopt, ICON_NONE);
}

static void panel_register(ARegionType *region_type)
{
  PanelType *panel_type = shaderfx_panel_register(region_type, eShaderFxType_Shadow, panel_draw);
  shaderfx_subpanel_register(region_type, "blur", "Blur", nullptr, blur_panel_draw, panel_type);
  shaderfx_subpanel_register(
      region_type, "wave", "", wave_header_draw, wave_panel_draw, panel_type);
}

static void foreach_working_space_color(ShaderFxData *fx,
                                        const IDTypeForeachColorFunctionCallback &fn)
{
  ShadowShaderFxData *fxd = (ShadowShaderFxData *)fx;
  fn.single(fxd->shadow_rgba);
}

ShaderFxTypeInfo shaderfx_Type_Shadow = {
    /*name*/ N_("Shadow"),
    /*struct_name*/ "ShadowShaderFxData",
    /*struct_size*/ sizeof(ShadowShaderFxData),
    /*type*/ eShaderFxType_GpencilType,
    /*flags*/ ShaderFxTypeFlag(0),

    /*copy_data*/ copy_data,

    /*init_data*/ init_data,
    /*free_data*/ nullptr,
    /*is_disabled*/ is_disabled,
    /*update_depsgraph*/ update_depsgraph,
    /*depends_on_time*/ nullptr,
    /*foreach_ID_link*/ foreach_ID_link,
    /*foreach_working_space_color*/ foreach_working_space_color,
    /*panel_register*/ panel_register,
};
