/* SPDX-FileCopyrightText: 2018-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "draw_object_infos_infos.hh"

#include "draw_model_lib.glsl"

#if !defined(OBINFO_LIB) && !defined(GLSL_CPP_STUBS)
#  error Missing draw_object_infos additional create info on shader create info
#endif

#if defined(GPU_VERTEX_SHADER)
VERTEX_SHADER_CREATE_INFO(draw_object_infos)
#elif defined(GPU_FRAGMENT_SHADER)
FRAGMENT_SHADER_CREATE_INFO(draw_object_infos)
#elif defined(GPU_LIBRARY_SHADER)
SHADER_LIBRARY_CREATE_INFO(draw_object_infos)
#endif

ObjectInfos drw_object_infos()
{
#ifdef OBINFO_LIB
  return drw_infos[drw_resource_id()];
#else
  return ObjectInfos();
#endif
}

/* Convert local coordinates to "Original coordinates" for texture mapping.
 * This is supposed to only be used if there is no modifier that distort the object.
 * Otherwise, a geometry attribute should be used instead. */
float3 drw_object_orco(float3 lP)
{
  ObjectInfos info = drw_object_infos();
  return info.orco_add + lP * info.orco_mul;
}

float4 drw_object_attribute(const uint attr_hash)
{
#if defined(OBATTR_LIB)
  ObjectInfos infos = drw_object_infos();
  uint index = infos.object_attrs_offset;
  for (uint i = 0; i < infos.object_attrs_len; i++, index++) {
    ObjectAttribute attr = drw_attrs[index];
    if (attr.hash_code == attr_hash) {
      return float4(attr.data_x, attr.data_y, attr.data_z, attr.data_w);
    }
  }
#endif
  return float4(0.0f);
}
