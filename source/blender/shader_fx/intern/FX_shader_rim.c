/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2018 Blender Foundation. */

/** \file
 * \ingroup shader_fx
 */

#include <stdio.h>

#include "DNA_screen_types.h"
#include "DNA_shader_fx_types.h"

#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "BKE_context.h"
#include "BKE_screen.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"

#include "FX_shader_types.h"
#include "FX_ui_common.h"

static void initData(ShaderFxData *fx)
{
  RimShaderFxData *gpfx = (RimShaderFxData *)fx;
  ARRAY_SET_ITEMS(gpfx->offset, 50, -100);
  ARRAY_SET_ITEMS(gpfx->rim_rgb, 1.0f, 1.0f, 0.5f);
  ARRAY_SET_ITEMS(gpfx->mask_rgb, 0.0f, 0.0f, 0.0f);
  gpfx->mode = eShaderFxRimMode_Overlay;

  ARRAY_SET_ITEMS(gpfx->blur, 0, 0);
  gpfx->samples = 2;
}

static void copyData(const ShaderFxData *md, ShaderFxData *target)
{
  BKE_shaderfx_copydata_generic(md, target);
}

static void panel_draw(const bContext *UNUSED(C), Panel *panel)
{
  uiLayout *col;
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = shaderfx_panel_get_property_pointers(panel, NULL);

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, ptr, "rim_color", 0, NULL, ICON_NONE);
  uiItemR(layout, ptr, "mask_color", 0, NULL, ICON_NONE);
  uiItemR(layout, ptr, "mode", 0, IFACE_("Blend Mode"), ICON_NONE);

  /* Add the X, Y labels manually because offset is a #PROP_PIXEL. */
  col = uiLayoutColumn(layout, true);
  PropertyRNA *prop = RNA_struct_find_property(ptr, "offset");
  uiItemFullR(col, ptr, prop, 0, 0, 0, IFACE_("Offset X"), ICON_NONE);
  uiItemFullR(col, ptr, prop, 1, 0, 0, IFACE_("Y"), ICON_NONE);

  shaderfx_panel_end(layout, ptr);
}

static void blur_panel_draw(const bContext *UNUSED(C), Panel *panel)
{
  uiLayout *col;
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = shaderfx_panel_get_property_pointers(panel, NULL);

  uiLayoutSetPropSep(layout, true);

  /* Add the X, Y labels manually because blur is a #PROP_PIXEL. */
  col = uiLayoutColumn(layout, true);
  PropertyRNA *prop = RNA_struct_find_property(ptr, "blur");
  uiItemFullR(col, ptr, prop, 0, 0, 0, IFACE_("Blur X"), ICON_NONE);
  uiItemFullR(col, ptr, prop, 1, 0, 0, IFACE_("Y"), ICON_NONE);

  uiItemR(layout, ptr, "samples", 0, NULL, ICON_NONE);
}

static void panelRegister(ARegionType *region_type)
{
  PanelType *panel_type = shaderfx_panel_register(region_type, eShaderFxType_Rim, panel_draw);
  shaderfx_subpanel_register(region_type, "blur", "Blur", NULL, blur_panel_draw, panel_type);
}

ShaderFxTypeInfo shaderfx_Type_Rim = {
    /*name*/ N_("Rim"),
    /*structName*/ "RimShaderFxData",
    /*structSize*/ sizeof(RimShaderFxData),
    /*type*/ eShaderFxType_GpencilType,
    /*flags*/ 0,

    /*copyData*/ copyData,

    /*initData*/ initData,
    /*freeData*/ NULL,
    /*isDisabled*/ NULL,
    /*updateDepsgraph*/ NULL,
    /*dependsOnTime*/ NULL,
    /*foreachIDLink*/ NULL,
    /*panelRegister*/ panelRegister,
};
