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
 * The Original Code is Copyright (C) 2017, Blender Foundation
 * This is a new part of Blender
 *
 * Contributor(s): Antonio Vazquez
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

/** \file blender/shader_fx/intern/FX_shader_pixel.c
 *  \ingroup shader_fx
 */

#include <stdio.h>

#include "BLI_utildefines.h"

#include "FX_shader_types.h"

static void initData(ShaderFxData *fx)
{
	PixelShaderFxData *gpfx = (PixelShaderFxData *)fx;
	ARRAY_SET_ITEMS(gpfx->size, 5, 5);
	ARRAY_SET_ITEMS(gpfx->rgba, 0.0f, 0.0f, 0.0f, 0.9f);
	gpfx->flag |= FX_PIXEL_USE_LINES;
}

static void copyData(const ShaderFxData *md, ShaderFxData *target)
{
	BKE_shaderfx_copyData_generic(md, target);
}

ShaderFxTypeInfo shaderfx_Type_Pixel = {
	/* name */              "Pixelate",
	/* structName */        "PixelShaderFxData",
	/* structSize */        sizeof(PixelShaderFxData),
	/* type */              eShaderFxType_GpencilType,
	/* flags */             eShaderFxTypeFlag_Single,

	/* copyData */          copyData,

	/* initData */          initData,
	/* freeData */          NULL,
	/* isDisabled */        NULL,
	/* updateDepsgraph */   NULL,
	/* dependsOnTime */     NULL,
	/* foreachObjectLink */ NULL,
	/* foreachIDLink */     NULL,
};
