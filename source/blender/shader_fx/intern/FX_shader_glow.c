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

#include "DNA_gpencil_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_modifier.h"
#include "BKE_screen.h"
#include "BKE_shader_fx.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"

#include "FX_shader_types.h"
#include "FX_ui_common.h"

static void initData(ShaderFxData *md)
{
  GlowShaderFxData *gpfx = (GlowShaderFxData *)md;
  ARRAY_SET_ITEMS(gpfx->glow_color, 0.75f, 1.0f, 1.0f, 1.0f);
  ARRAY_SET_ITEMS(gpfx->select_color, 0.0f, 0.0f, 0.0f);
  copy_v2_fl(gpfx->blur, 50.0f);
  gpfx->threshold = 0.1f;
  gpfx->samples = 8;
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

  int mode = RNA_enum_get(&ptr, "mode");

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, &ptr, "mode", 0, NULL, ICON_NONE);

  if (mode == eShaderFxGlowMode_Luminance) {
    uiItemR(layout, &ptr, "threshold", 0, NULL, ICON_NONE);
  }
  else {
    uiItemR(layout, &ptr, "select_color", 0, NULL, ICON_NONE);
  }
  uiItemR(layout, &ptr, "glow_color", 0, NULL, ICON_NONE);

  uiItemS(layout);

  uiItemR(layout, &ptr, "blend_mode", 0, NULL, ICON_NONE);
  uiItemR(layout, &ptr, "opacity", 0, NULL, ICON_NONE);
  uiItemR(layout, &ptr, "size", 0, NULL, ICON_NONE);
  uiItemR(layout, &ptr, "rotation", 0, NULL, ICON_NONE);
  uiItemR(layout, &ptr, "samples", 0, NULL, ICON_NONE);
  uiItemR(layout, &ptr, "use_glow_under", 0, NULL, ICON_NONE);

  shaderfx_panel_end(layout, &ptr);
}

static void panelRegister(ARegionType *region_type)
{
  shaderfx_panel_register(region_type, eShaderFxType_Glow, panel_draw);
}

ShaderFxTypeInfo shaderfx_Type_Glow = {
    /* name */ "Glow",
    /* structName */ "GlowShaderFxData",
    /* structSize */ sizeof(GlowShaderFxData),
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
