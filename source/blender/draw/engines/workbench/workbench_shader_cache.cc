/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "workbench_private.hh"

namespace blender::workbench {

ShaderCache::~ShaderCache()
{
  for (auto i : IndexRange(pipeline_type_len)) {
    for (auto j : IndexRange(geometry_type_len)) {
      for (auto k : IndexRange(shader_type_len)) {
        for (auto l : IndexRange(lighting_type_len)) {
          for (auto m : IndexRange(2) /*clip*/) {
            DRW_SHADER_FREE_SAFE(prepass_shader_cache_[i][j][k][l][m]);
          }
        }
      }
    }
  }
  for (auto i : IndexRange(pipeline_type_len)) {
    for (auto j : IndexRange(lighting_type_len)) {
      for (auto k : IndexRange(2) /*cavity*/) {
        for (auto l : IndexRange(2) /*curvature*/) {
          for (auto m : IndexRange(2) /*shadow*/) {
            DRW_SHADER_FREE_SAFE(resolve_shader_cache_[i][j][k][l][m]);
          }
        }
      }
    }
  }
}

GPUShader *ShaderCache::prepass_shader_get(ePipelineType pipeline_type,
                                           eGeometryType geometry_type,
                                           eShaderType shader_type,
                                           eLightingType lighting_type,
                                           bool clip)
{
  GPUShader *&shader_ptr = prepass_shader_cache_[int(pipeline_type)][int(geometry_type)][int(
      shader_type)][int(lighting_type)][clip ? 1 : 0];

  if (shader_ptr != nullptr) {
    return shader_ptr;
  }
  std::string info_name = "workbench_prepass_";
  switch (geometry_type) {
    case eGeometryType::MESH:
      info_name += "mesh_";
      break;
    case eGeometryType::CURVES:
      info_name += "curves_";
      break;
    case eGeometryType::POINTCLOUD:
      info_name += "ptcloud_";
      break;
  }
  switch (pipeline_type) {
    case ePipelineType::OPAQUE:
      info_name += "opaque_";
      break;
    case ePipelineType::TRANSPARENT:
      info_name += "transparent_";
      break;
    case ePipelineType::SHADOW:
      info_name += "shadow_";
      break;
  }
  switch (lighting_type) {
    case eLightingType::FLAT:
      info_name += "flat_";
      break;
    case eLightingType::STUDIO:
      info_name += "studio_";
      break;
    case eLightingType::MATCAP:
      info_name += "matcap_";
      break;
  }
  switch (shader_type) {
    case eShaderType::MATERIAL:
      info_name += "material";
      break;
    case eShaderType::TEXTURE:
      info_name += "texture";
      break;
  }
  info_name += clip ? "_clip" : "_no_clip";
  shader_ptr = GPU_shader_create_from_info_name(info_name.c_str());
  return shader_ptr;
}

GPUShader *ShaderCache::resolve_shader_get(ePipelineType pipeline_type,
                                           eLightingType lighting_type,
                                           bool cavity,
                                           bool curvature,
                                           bool shadow)
{
  GPUShader *&shader_ptr =
      resolve_shader_cache_[int(pipeline_type)][int(lighting_type)][cavity][curvature][shadow];

  if (shader_ptr != nullptr) {
    return shader_ptr;
  }
  std::string info_name = "workbench_resolve_";
  switch (pipeline_type) {
    case ePipelineType::OPAQUE:
      info_name += "opaque_";
      break;
    case ePipelineType::TRANSPARENT:
      info_name += "transparent_";
      break;
    case ePipelineType::SHADOW:
      BLI_assert_unreachable();
      break;
  }
  switch (lighting_type) {
    case eLightingType::FLAT:
      info_name += "flat";
      break;
    case eLightingType::STUDIO:
      info_name += "studio";
      break;
    case eLightingType::MATCAP:
      info_name += "matcap";
      break;
  }
  info_name += cavity ? "_cavity" : "_no_cavity";
  info_name += curvature ? "_curvature" : "_no_curvature";
  info_name += shadow ? "_shadow" : "_no_shadow";

  shader_ptr = GPU_shader_create_from_info_name(info_name.c_str());
  return shader_ptr;
}

}  // namespace blender::workbench
