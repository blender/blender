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
 */

/** \file
 * \ingroup shader_fx
 */

#pragma once

#include "BKE_shader_fx.h"

/* ****************** Type structures for all effects ****************** */

extern ShaderFxTypeInfo shaderfx_Type_None;
extern ShaderFxTypeInfo shaderfx_Type_Blur;
extern ShaderFxTypeInfo shaderfx_Type_Colorize;
extern ShaderFxTypeInfo shaderfx_Type_Flip;
extern ShaderFxTypeInfo shaderfx_Type_Glow;
extern ShaderFxTypeInfo shaderfx_Type_Pixel;
extern ShaderFxTypeInfo shaderfx_Type_Rim;
extern ShaderFxTypeInfo shaderfx_Type_Shadow;
extern ShaderFxTypeInfo shaderfx_Type_Swirl;
extern ShaderFxTypeInfo shaderfx_Type_Wave;

/* FX_shaderfx_util.c */
void shaderfx_type_init(ShaderFxTypeInfo *types[]);
