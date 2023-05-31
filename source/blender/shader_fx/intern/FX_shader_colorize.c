/* SPDX-FileCopyrightText: 2018 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shader_fx
 */

#include <stdio.h>

#include "BKE_context.h"
#include "BKE_screen.h"

#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_screen_types.h"
#include "DNA_shader_fx_types.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"

#include "FX_shader_types.h"
#include "FX_ui_common.h"

static void initData(ShaderFxData *fx)
{
  ColorizeShaderFxData *gpfx = (ColorizeShaderFxData *)fx;
  ARRAY_SET_ITEMS(gpfx->low_color, 0.0f, 0.0f, 0.0f, 1.0f);
  ARRAY_SET_ITEMS(gpfx->high_color, 1.0f, 1.0f, 1.0f, 1.0f);
  gpfx->mode = eShaderFxColorizeMode_GrayScale;
  gpfx->factor = 0.5f;
}

static void copyData(const ShaderFxData *md, ShaderFxData *target)
{
  BKE_shaderfx_copydata_generic(md, target);
}

static void panel_draw(const bContext *UNUSED(C), Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = shaderfx_panel_get_property_pointers(panel, NULL);

  int mode = RNA_enum_get(ptr, "mode");

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, ptr, "mode", 0, NULL, ICON_NONE);

  if (ELEM(mode, eShaderFxColorizeMode_Custom, eShaderFxColorizeMode_Duotone)) {
    const char *text = (mode == eShaderFxColorizeMode_Duotone) ? IFACE_("Low Color") :
                                                                 IFACE_("Color");
    uiItemR(layout, ptr, "low_color", 0, text, ICON_NONE);
  }
  if (mode == eShaderFxColorizeMode_Duotone) {
    uiItemR(layout, ptr, "high_color", 0, NULL, ICON_NONE);
  }

  uiItemR(layout, ptr, "factor", 0, NULL, ICON_NONE);

  shaderfx_panel_end(layout, ptr);
}

static void panelRegister(ARegionType *region_type)
{
  shaderfx_panel_register(region_type, eShaderFxType_Colorize, panel_draw);
}

ShaderFxTypeInfo shaderfx_Type_Colorize = {
    /*name*/ N_("Colorize"),
    /*structName*/ "ColorizeShaderFxData",
    /*structSize*/ sizeof(ColorizeShaderFxData),
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
