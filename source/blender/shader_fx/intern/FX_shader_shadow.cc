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

#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "BKE_context.h"
#include "BKE_lib_query.h"
#include "BKE_modifier.h"
#include "BKE_screen.h"
#include "BKE_shader_fx.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"

#include "FX_shader_types.h"
#include "FX_ui_common.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

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

static bool is_disabled(ShaderFxData *fx, int /*user_render_params*/)
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

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, ptr, "shadow_color", 0, nullptr, ICON_NONE);

  /* Add the X, Y labels manually because size is a #PROP_PIXEL. */
  col = uiLayoutColumn(layout, true);
  PropertyRNA *prop = RNA_struct_find_property(ptr, "offset");
  uiItemFullR(col, ptr, prop, 0, 0, 0, IFACE_("Offset X"), ICON_NONE);
  uiItemFullR(col, ptr, prop, 1, 0, 0, IFACE_("Y"), ICON_NONE);

  uiItemR(layout, ptr, "scale", 0, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "rotation", 0, nullptr, ICON_NONE);

  row = uiLayoutRowWithHeading(layout, true, IFACE_("Object Pivot"));
  uiItemR(row, ptr, "use_object", 0, "", ICON_NONE);
  uiItemR(row, ptr, "object", 0, "", ICON_NONE);

  shaderfx_panel_end(layout, ptr);
}

static void blur_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *col;
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = shaderfx_panel_get_property_pointers(panel, nullptr);

  uiLayoutSetPropSep(layout, true);

  /* Add the X, Y labels manually because size is a #PROP_PIXEL. */
  col = uiLayoutColumn(layout, true);
  PropertyRNA *prop = RNA_struct_find_property(ptr, "blur");
  uiItemFullR(col, ptr, prop, 0, 0, 0, IFACE_("Blur X"), ICON_NONE);
  uiItemFullR(col, ptr, prop, 1, 0, 0, IFACE_("Y"), ICON_NONE);

  uiItemR(layout, ptr, "samples", 0, nullptr, ICON_NONE);
}

static void wave_header_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = shaderfx_panel_get_property_pointers(panel, nullptr);

  uiItemR(layout, ptr, "use_wave", 0, IFACE_("Wave Effect"), ICON_NONE);
}

static void wave_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = shaderfx_panel_get_property_pointers(panel, nullptr);

  uiLayoutSetPropSep(layout, true);

  uiLayoutSetActive(layout, RNA_boolean_get(ptr, "use_wave"));

  uiItemR(layout, ptr, "orientation", UI_ITEM_R_EXPAND, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "amplitude", 0, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "period", 0, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "phase", 0, nullptr, ICON_NONE);
}

static void panel_register(ARegionType *region_type)
{
  PanelType *panel_type = shaderfx_panel_register(region_type, eShaderFxType_Shadow, panel_draw);
  shaderfx_subpanel_register(region_type, "blur", "Blur", nullptr, blur_panel_draw, panel_type);
  shaderfx_subpanel_register(
      region_type, "wave", "", wave_header_draw, wave_panel_draw, panel_type);
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
    /*panel_register*/ panel_register,
};
