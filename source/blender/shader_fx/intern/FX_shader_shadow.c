/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2018 Blender Foundation. */

/** \file
 * \ingroup shader_fx
 */

#include <stdio.h>

#include "DNA_gpencil_types.h"
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

static void initData(ShaderFxData *md)
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

  gpfx->object = NULL;
}

static void copyData(const ShaderFxData *md, ShaderFxData *target)
{
  BKE_shaderfx_copydata_generic(md, target);
}

static void updateDepsgraph(ShaderFxData *fx, const ModifierUpdateDepsgraphContext *ctx)
{
  ShadowShaderFxData *fxd = (ShadowShaderFxData *)fx;
  if (fxd->object != NULL) {
    DEG_add_object_relation(ctx->node, fxd->object, DEG_OB_COMP_TRANSFORM, "Shadow ShaderFx");
  }
  DEG_add_object_relation(ctx->node, ctx->object, DEG_OB_COMP_TRANSFORM, "Shadow ShaderFx");
}

static bool isDisabled(ShaderFxData *fx, int UNUSED(userRenderParams))
{
  ShadowShaderFxData *fxd = (ShadowShaderFxData *)fx;

  return (!fxd->object) && (fxd->flag & FX_SHADOW_USE_OBJECT);
}

static void foreachIDLink(ShaderFxData *fx, Object *ob, IDWalkFunc walk, void *userData)
{
  ShadowShaderFxData *fxd = (ShadowShaderFxData *)fx;

  walk(userData, ob, (ID **)&fxd->object, IDWALK_CB_NOP);
}

static void panel_draw(const bContext *UNUSED(C), Panel *panel)
{
  uiLayout *row, *col;
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = shaderfx_panel_get_property_pointers(panel, NULL);

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, ptr, "shadow_color", 0, NULL, ICON_NONE);

  /* Add the X, Y labels manually because size is a #PROP_PIXEL. */
  col = uiLayoutColumn(layout, true);
  PropertyRNA *prop = RNA_struct_find_property(ptr, "offset");
  uiItemFullR(col, ptr, prop, 0, 0, 0, IFACE_("Offset X"), ICON_NONE);
  uiItemFullR(col, ptr, prop, 1, 0, 0, IFACE_("Y"), ICON_NONE);

  uiItemR(layout, ptr, "scale", 0, NULL, ICON_NONE);
  uiItemR(layout, ptr, "rotation", 0, NULL, ICON_NONE);

  row = uiLayoutRowWithHeading(layout, true, IFACE_("Object Pivot"));
  uiItemR(row, ptr, "use_object", 0, "", ICON_NONE);
  uiItemR(row, ptr, "object", 0, "", ICON_NONE);

  shaderfx_panel_end(layout, ptr);
}

static void blur_panel_draw(const bContext *UNUSED(C), Panel *panel)
{
  uiLayout *col;
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = shaderfx_panel_get_property_pointers(panel, NULL);

  uiLayoutSetPropSep(layout, true);

  /* Add the X, Y labels manually because size is a #PROP_PIXEL. */
  col = uiLayoutColumn(layout, true);
  PropertyRNA *prop = RNA_struct_find_property(ptr, "blur");
  uiItemFullR(col, ptr, prop, 0, 0, 0, IFACE_("Blur X"), ICON_NONE);
  uiItemFullR(col, ptr, prop, 1, 0, 0, IFACE_("Y"), ICON_NONE);

  uiItemR(layout, ptr, "samples", 0, NULL, ICON_NONE);
}

static void wave_header_draw(const bContext *UNUSED(C), Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = shaderfx_panel_get_property_pointers(panel, NULL);

  uiItemR(layout, ptr, "use_wave", 0, IFACE_("Wave Effect"), ICON_NONE);
}

static void wave_panel_draw(const bContext *UNUSED(C), Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = shaderfx_panel_get_property_pointers(panel, NULL);

  uiLayoutSetPropSep(layout, true);

  uiLayoutSetActive(layout, RNA_boolean_get(ptr, "use_wave"));

  uiItemR(layout, ptr, "orientation", UI_ITEM_R_EXPAND, NULL, ICON_NONE);
  uiItemR(layout, ptr, "amplitude", 0, NULL, ICON_NONE);
  uiItemR(layout, ptr, "period", 0, NULL, ICON_NONE);
  uiItemR(layout, ptr, "phase", 0, NULL, ICON_NONE);
}

static void panelRegister(ARegionType *region_type)
{
  PanelType *panel_type = shaderfx_panel_register(region_type, eShaderFxType_Shadow, panel_draw);
  shaderfx_subpanel_register(region_type, "blur", "Blur", NULL, blur_panel_draw, panel_type);
  shaderfx_subpanel_register(
      region_type, "wave", "", wave_header_draw, wave_panel_draw, panel_type);
}

ShaderFxTypeInfo shaderfx_Type_Shadow = {
    /*name*/ N_("Shadow"),
    /*structName*/ "ShadowShaderFxData",
    /*structSize*/ sizeof(ShadowShaderFxData),
    /*type*/ eShaderFxType_GpencilType,
    /*flags*/ 0,

    /*copyData*/ copyData,

    /*initData*/ initData,
    /*freeData*/ NULL,
    /*isDisabled*/ isDisabled,
    /*updateDepsgraph*/ updateDepsgraph,
    /*dependsOnTime*/ NULL,
    /*foreachIDLink*/ foreachIDLink,
    /*panelRegister*/ panelRegister,
};
