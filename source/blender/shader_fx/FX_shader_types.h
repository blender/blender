/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shader_fx
 */

#pragma once

#include "BKE_shader_fx.h"

#ifdef __cplusplus
extern "C" {
#endif

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

#ifdef __cplusplus
}
#endif
