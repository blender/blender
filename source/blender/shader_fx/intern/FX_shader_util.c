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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2018, Blender Foundation
 * This is a new part of Blender
 *
 * Contributor(s): Antonio Vazquez
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/shader_fx/intern/FX_shader_util.c
 *  \ingroup shader_fx
 */


#include <stdio.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BKE_shader_fx.h"

#include "FX_shader_types.h"
#include "FX_shader_util.h"

void shaderfx_type_init(ShaderFxTypeInfo *types[])
{
#define INIT_FX_TYPE(typeName) (types[eShaderFxType_##typeName] = &shaderfx_Type_##typeName)
	INIT_FX_TYPE(Blur);
	INIT_FX_TYPE(Colorize);
	INIT_FX_TYPE(Flip);
	INIT_FX_TYPE(Light);
	INIT_FX_TYPE(Pixel);
	INIT_FX_TYPE(Rim);
	INIT_FX_TYPE(Swirl);
	INIT_FX_TYPE(Wave);
#undef INIT_FX_TYPE
}

