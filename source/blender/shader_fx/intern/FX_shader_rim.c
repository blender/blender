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

/** \file blender/shader_fx/intern/FX_shader_rim.c
 *  \ingroup shader_fx
 */

#include <stdio.h>

#include "DNA_shader_fx_types.h"

#include "BLI_utildefines.h"

#include "FX_shader_types.h"

static void initData(ShaderFxData *fx)
{
	RimShaderFxData *gpfx = (RimShaderFxData *)fx;
	ARRAY_SET_ITEMS(gpfx->offset, 50, -100);
	ARRAY_SET_ITEMS(gpfx->rim_rgb, 1.0f, 1.0f, 0.5f, 0.9f);
	ARRAY_SET_ITEMS(gpfx->mask_rgb, 0.0f, 0.0f, 0.0f, 1.0f);
	gpfx->mode = eShaderFxRimMode_Multiply;
	ARRAY_SET_ITEMS(gpfx->blur, 0, 0);
}

static void copyData(const ShaderFxData *md, ShaderFxData *target)
{
	BKE_shaderfx_copyData_generic(md, target);
}

ShaderFxTypeInfo shaderfx_Type_Rim = {
	/* name */              "Rim",
	/* structName */        "RimShaderFxData",
	/* structSize */        sizeof(RimShaderFxData),
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
