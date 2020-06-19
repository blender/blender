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
 * The Original Code is Copyright (C) 2017, Blender Foundation
 * This is a new part of Blender
 */

/** \file
 * \ingroup shader_fx
 */

#include <stdio.h>

#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "BKE_context.h"
#include "BKE_screen.h"

#include "DNA_screen_types.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"

#include "FX_shader_types.h"
#include "FX_ui_common.h"

static void initData(ShaderFxData *fx)
{
  PixelShaderFxData *gpfx = (PixelShaderFxData *)fx;
  ARRAY_SET_ITEMS(gpfx->size, 5, 5);
  ARRAY_SET_ITEMS(gpfx->rgba, 0.0f, 0.0f, 0.0f, 0.9f);
}

static void copyData(const ShaderFxData *md, ShaderFxData *target)
{
  BKE_shaderfx_copydata_generic(md, target);
}

static void panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *col;
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  shaderfx_panel_get_property_pointers(C, panel, NULL, &ptr);

  uiLayoutSetPropSep(layout, true);

  /* Add the X, Y labels manually because size is a #PROP_PIXEL. */
  col = uiLayoutColumn(layout, true);
  PropertyRNA *prop = RNA_struct_find_property(&ptr, "size");
  uiItemFullR(col, &ptr, prop, 0, 0, 0, IFACE_("Size X"), ICON_NONE);
  uiItemFullR(col, &ptr, prop, 1, 0, 0, IFACE_("Y"), ICON_NONE);

  shaderfx_panel_end(layout, &ptr);
}

static void panelRegister(ARegionType *region_type)
{
  shaderfx_panel_register(region_type, eShaderFxType_Pixel, panel_draw);
}

ShaderFxTypeInfo shaderfx_Type_Pixel = {
    /* name */ "Pixelate",
    /* structName */ "PixelShaderFxData",
    /* structSize */ sizeof(PixelShaderFxData),
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
