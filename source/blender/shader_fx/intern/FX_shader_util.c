/* SPDX-FileCopyrightText: 2018 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shader_fx
 */

#include <stdio.h>

#include "MEM_guardedalloc.h"

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
  INIT_FX_TYPE(Glow);
  INIT_FX_TYPE(Pixel);
  INIT_FX_TYPE(Rim);
  INIT_FX_TYPE(Shadow);
  INIT_FX_TYPE(Swirl);
  INIT_FX_TYPE(Wave);
#undef INIT_FX_TYPE
}
