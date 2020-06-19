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

#include "BLI_math_base.h"
#include "BLI_utildefines.h"

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
  SwirlShaderFxData *gpmd = (SwirlShaderFxData *)md;
  gpmd->radius = 100;
  gpmd->angle = M_PI_2;
}

static void copyData(const ShaderFxData *md, ShaderFxData *target)
{
  BKE_shaderfx_copydata_generic(md, target);
}

static void updateDepsgraph(ShaderFxData *fx, const ModifierUpdateDepsgraphContext *ctx)
{
  SwirlShaderFxData *fxd = (SwirlShaderFxData *)fx;
  if (fxd->object != NULL) {
    DEG_add_object_relation(ctx->node, fxd->object, DEG_OB_COMP_TRANSFORM, "Swirl ShaderFx");
  }
  DEG_add_object_relation(ctx->node, ctx->object, DEG_OB_COMP_TRANSFORM, "Swirl ShaderFx");
}

static bool isDisabled(ShaderFxData *fx, int UNUSED(userRenderParams))
{
  SwirlShaderFxData *fxd = (SwirlShaderFxData *)fx;

  return !fxd->object;
}

static void foreachObjectLink(ShaderFxData *fx,
                              Object *ob,
                              ShaderFxObjectWalkFunc walk,
                              void *userData)
{
  SwirlShaderFxData *fxd = (SwirlShaderFxData *)fx;

  walk(userData, ob, &fxd->object, IDWALK_CB_NOP);
}

static void panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  PointerRNA ob_ptr;
  shaderfx_panel_get_property_pointers(C, panel, &ob_ptr, &ptr);

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, &ptr, "object", 0, NULL, ICON_NONE);
  uiItemR(layout, &ptr, "radius", 0, NULL, ICON_NONE);
  uiItemR(layout, &ptr, "angle", 0, NULL, ICON_NONE);

  shaderfx_panel_end(layout, &ptr);
}

static void panelRegister(ARegionType *region_type)
{
  shaderfx_panel_register(region_type, eShaderFxType_Swirl, panel_draw);
}

ShaderFxTypeInfo shaderfx_Type_Swirl = {
    /* name */ "Swirl",
    /* structName */ "SwirlShaderFxData",
    /* structSize */ sizeof(SwirlShaderFxData),
    /* type */ eShaderFxType_GpencilType,
    /* flags */ 0,

    /* copyData */ copyData,

    /* initData */ initData,
    /* freeData */ NULL,
    /* isDisabled */ isDisabled,
    /* updateDepsgraph */ updateDepsgraph,
    /* dependsOnTime */ NULL,
    /* foreachObjectLink */ foreachObjectLink,
    /* foreachIDLink */ NULL,
    /* panelRegister */ panelRegister,
};
