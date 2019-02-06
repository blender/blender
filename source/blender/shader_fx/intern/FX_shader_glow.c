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

/** \file \ingroup shader_fx
 */

#include <stdio.h>

#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_gpencil_types.h"

#include "BLI_utildefines.h"

#include "BKE_modifier.h"
#include "BKE_shader_fx.h"

#include "FX_shader_types.h"

static void initData(ShaderFxData *md)
{
	GlowShaderFxData *gpfx = (GlowShaderFxData *)md;
	ARRAY_SET_ITEMS(gpfx->glow_color, 0.75f, 1.0f, 1.0f);
	ARRAY_SET_ITEMS(gpfx->select_color, 0.0f, 0.0f, 0.0f);
	gpfx->threshold = 0.1f;

	ARRAY_SET_ITEMS(gpfx->blur, 50, 0);
	gpfx->samples = 16;
}

static void copyData(const ShaderFxData *md, ShaderFxData *target)
{
	BKE_shaderfx_copyData_generic(md, target);
}

ShaderFxTypeInfo shaderfx_Type_Glow = {
	/* name */              "Glow",
	/* structName */        "GlowShaderFxData",
	/* structSize */        sizeof(GlowShaderFxData),
	/* type */              eShaderFxType_GpencilType,
	/* flags */             0,

	/* copyData */          copyData,

	/* initData */          initData,
	/* freeData */          NULL,
	/* isDisabled */        NULL,
	/* updateDepsgraph */   NULL,
	/* dependsOnTime */     NULL,
	/* foreachObjectLink */ NULL,
	/* foreachIDLink */     NULL,
};
