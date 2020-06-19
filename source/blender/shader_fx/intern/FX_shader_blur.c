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

#include "BLI_math.h"
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
  BlurShaderFxData *gpfx = (BlurShaderFxData *)fx;
  copy_v2_fl(gpfx->radius, 50.0f);
  gpfx->samples = 8;
  gpfx->rotation = 0.0f;
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

  uiItemR(layout, &ptr, "samples", 0, NULL, ICON_NONE);

  uiItemR(layout, &ptr, "use_dof_mode", 0, IFACE_("Use Depth of Field"), ICON_NONE);
  col = uiLayoutColumn(layout, false);
  uiLayoutSetActive(col, !RNA_boolean_get(&ptr, "use_dof_mode"));
  uiItemR(col, &ptr, "size", 0, NULL, ICON_NONE);
  uiItemR(col, &ptr, "rotation", 0, NULL, ICON_NONE);

  shaderfx_panel_end(layout, &ptr);
}

static void panelRegister(ARegionType *region_type)
{
  shaderfx_panel_register(region_type, eShaderFxType_Blur, panel_draw);
}

ShaderFxTypeInfo shaderfx_Type_Blur = {
    /* name */ "Blur",
    /* structName */ "BlurShaderFxData",
    /* structSize */ sizeof(BlurShaderFxData),
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
