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

/** \file blender/shader_fx/intern/FX_shader_wave.c
 *  \ingroup shader_fx
 */

#include <stdio.h>

#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_gpencil_types.h"

#include "BLI_utildefines.h"

#include "FX_shader_types.h"

static void initData(ShaderFxData *fx)
{
	WaveShaderFxData *gpfx = (WaveShaderFxData *)fx;
	gpfx->amplitude = 10.0f;
	gpfx->period = 20.0f;
	gpfx->phase = 0.0f;
	gpfx->orientation = 1;
}

static void copyData(const ShaderFxData *md, ShaderFxData *target)
{
	BKE_shaderfx_copyData_generic(md, target);
}

ShaderFxTypeInfo shaderfx_Type_Wave = {
	/* name */              "Wave Distorsion",
	/* structName */        "WaveShaderFxData",
	/* structSize */        sizeof(WaveShaderFxData),
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
