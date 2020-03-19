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
 * along with this program; if not, write to the Free Software  Foundation,
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

#include "BLI_utildefines.h"

#include "BKE_lib_query.h"
#include "BKE_modifier.h"
#include "BKE_shader_fx.h"

#include "FX_shader_types.h"

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
  BKE_shaderfx_copyData_generic(md, target);
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

static void foreachObjectLink(ShaderFxData *fx,
                              Object *ob,
                              ShaderFxObjectWalkFunc walk,
                              void *userData)
{
  ShadowShaderFxData *fxd = (ShadowShaderFxData *)fx;

  walk(userData, ob, &fxd->object, IDWALK_CB_NOP);
}

ShaderFxTypeInfo shaderfx_Type_Shadow = {
    /* name */ "Shadow",
    /* structName */ "ShadowShaderFxData",
    /* structSize */ sizeof(ShadowShaderFxData),
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
};
