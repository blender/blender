/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * Contributor(s): Antonio Vazquez
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

/** \file blender/shader_fx/intern/FX_shader_light.c
 *  \ingroup shader_fx
 */

#include <stdio.h>

#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_gpencil_types.h"

#include "BLI_math_base.h"
#include "BLI_utildefines.h"

#include "BKE_library_query.h"
#include "BKE_modifier.h"
#include "BKE_shader_fx.h"

#include "FX_shader_types.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

static void initData(ShaderFxData *fx)
{
	LightShaderFxData *gpfx = (LightShaderFxData *)fx;
	gpfx->energy = 10.0f;
	gpfx->ambient = 5.0f;
	gpfx->object = NULL;
}

static void copyData(const ShaderFxData *md, ShaderFxData *target)
{
	BKE_shaderfx_copyData_generic(md, target);
}

static void updateDepsgraph(ShaderFxData *md, const ModifierUpdateDepsgraphContext *ctx)
{
	LightShaderFxData *fxd = (LightShaderFxData *)md;
	if (fxd->object != NULL) {
		DEG_add_object_relation(ctx->node, fxd->object, DEG_OB_COMP_GEOMETRY, "Light ShaderFx");
		DEG_add_object_relation(ctx->node, fxd->object, DEG_OB_COMP_TRANSFORM, "Light ShaderFx");
	}
	DEG_add_object_relation(ctx->node, ctx->object, DEG_OB_COMP_TRANSFORM, "Light ShaderFx");
}

static bool isDisabled(ShaderFxData *fx, int UNUSED(userRenderParams))
{
	LightShaderFxData *fxd = (LightShaderFxData *)fx;

	return !fxd->object;
}

static void foreachObjectLink(
	ShaderFxData *fx, Object *ob,
	ShaderFxObjectWalkFunc walk, void *userData)
{
	LightShaderFxData *fxd = (LightShaderFxData *)fx;

	walk(userData, ob, &fxd->object, IDWALK_CB_NOP);
}

ShaderFxTypeInfo shaderfx_Type_Light = {
	/* name */              "Light",
	/* structName */        "LightShaderFxData",
	/* structSize */        sizeof(LightShaderFxData),
	/* type */              eShaderFxType_GpencilType,
	/* flags */             0,

	/* copyData */          copyData,

	/* initData */          initData,
	/* freeData */          NULL,
	/* isDisabled */        isDisabled,
	/* updateDepsgraph */   updateDepsgraph,
	/* dependsOnTime */     NULL,
	/* foreachObjectLink */ foreachObjectLink,
	/* foreachIDLink */     NULL,
};
