/* SPDX-FileCopyrightText: 2018-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "draw_object_infos_infos.hh"

#include "draw_model_lib.glsl"

#if !defined(OBINFO_LIB) && !defined(GLSL_CPP_STUBS)
#  error Missing draw_object_infos additional create info on shader create info
#endif

ObjectInfos drw_object_infos()
{
  return buffer_get(draw_object_infos, drw_infos)[drw_resource_id()];
}

/* Convert local coordinates to "Original coordinates" for texture mapping.
 * This is supposed to only be used if there is no modifier that distort the object.
 * Otherwise, a geometry attribute should be used instead. */
float3 drw_object_orco(float3 lP)
{
  ObjectInfos info = drw_object_infos();
  return info.orco_add + lP * info.orco_mul;
}
