/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2018, Blender Foundation
 * This is a new part of Blender
 */

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

static void panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  shaderfx_panel_get_property_pointers(C, panel, NULL, &ptr);

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, &ptr, "rim_color", 0, NULL, ICON_NONE);
  uiItemR(layout, &ptr, "mask_color", 0, NULL, ICON_NONE);
  uiItemR(layout, &ptr, "mode", 0, IFACE_("Blend"), ICON_NONE);
  uiItemR(layout, &ptr, "offset", 0, NULL, ICON_NONE);

  shaderfx_panel_end(layout, &ptr);
}

static void blur_panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  shaderfx_panel_get_property_pointers(C, panel, NULL, &ptr);

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, &ptr, "blur", 0, NULL, ICON_NONE);
  uiItemR(layout, &ptr, "samples", 0, NULL, ICON_NONE);
}

static void panelRegister(ARegionType *region_type)
{
  PanelType *panel_type = shaderfx_panel_register(region_type, eShaderFxType_Rim, panel_draw);
  shaderfx_subpanel_register(region_type, "blur", "Blur", NULL, blur_panel_draw, panel_type);
}

ShaderFxTypeInfo shaderfx_Type_Rim = {
    /* name */ "Rim",
    /* structName */ "RimShaderFxData",
    /* structSize */ sizeof(RimShaderFxData),
    /* type */ eShaderFxType_GpencilType,
    /* flags */ 0,

    /* copyData */ copyData,

    /* initData */ initData,
    /* freeData */ NULL,
    /* isDisabled */ NULL,
    /* updateDepsgraph */ NULL,
    /* dependsOnTime */ NULL,
    /* foreachObjectLink */ NULL,
    /* foreachIDLink */ NULL,
    /* panelRegister */ panelRegister,
};
