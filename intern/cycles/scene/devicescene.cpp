/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#include "scene/devicescene.h"
#include "device/device.h"
#include "device/memory.h"

CCL_NAMESPACE_BEGIN

DeviceScene::DeviceScene(Device *device)
    : bvh_nodes(device, "bvh_nodes", MEM_GLOBAL),
      bvh_leaf_nodes(device, "bvh_leaf_nodes", MEM_GLOBAL),
      object_node(device, "object_node", MEM_GLOBAL),
      prim_type(device, "prim_type", MEM_GLOBAL),
      prim_visibility(device, "prim_visibility", MEM_GLOBAL),
      prim_index(device, "prim_index", MEM_GLOBAL),
      prim_object(device, "prim_object", MEM_GLOBAL),
      prim_time(device, "prim_time", MEM_GLOBAL),
      tri_verts(device, "tri_verts", MEM_GLOBAL),
      tri_shader(device, "tri_shader", MEM_GLOBAL),
      tri_vnormal(device, "tri_vnormal", MEM_GLOBAL),
      tri_vindex(device, "tri_vindex", MEM_GLOBAL),
      tri_patch(device, "tri_patch", MEM_GLOBAL),
      tri_patch_uv(device, "tri_patch_uv", MEM_GLOBAL),
      curves(device, "curves", MEM_GLOBAL),
      curve_keys(device, "curve_keys", MEM_GLOBAL),
      curve_segments(device, "curve_segments", MEM_GLOBAL),
      patches(device, "patches", MEM_GLOBAL),
      points(device, "points", MEM_GLOBAL),
      points_shader(device, "points_shader", MEM_GLOBAL),
      objects(device, "objects", MEM_GLOBAL),
      object_motion_pass(device, "object_motion_pass", MEM_GLOBAL),
      object_motion(device, "object_motion", MEM_GLOBAL),
      object_flag(device, "object_flag", MEM_GLOBAL),
      object_volume_step(device, "object_volume_step", MEM_GLOBAL),
      object_prim_offset(device, "object_prim_offset", MEM_GLOBAL),
      camera_motion(device, "camera_motion", MEM_GLOBAL),
      attributes_map(device, "attributes_map", MEM_GLOBAL),
      attributes_float(device, "attributes_float", MEM_GLOBAL),
      attributes_float2(device, "attributes_float2", MEM_GLOBAL),
      attributes_float3(device, "attributes_float3", MEM_GLOBAL),
      attributes_float4(device, "attributes_float4", MEM_GLOBAL),
      attributes_uchar4(device, "attributes_uchar4", MEM_GLOBAL),
      light_distribution(device, "light_distribution", MEM_GLOBAL),
      lights(device, "lights", MEM_GLOBAL),
      light_background_marginal_cdf(device, "light_background_marginal_cdf", MEM_GLOBAL),
      light_background_conditional_cdf(device, "light_background_conditional_cdf", MEM_GLOBAL),
      light_tree_nodes(device, "light_tree_nodes", MEM_GLOBAL),
      light_tree_emitters(device, "light_tree_emitters", MEM_GLOBAL),
      light_to_tree(device, "light_to_tree", MEM_GLOBAL),
      object_to_tree(device, "object_to_tree", MEM_GLOBAL),
      object_lookup_offset(device, "object_lookup_offset", MEM_GLOBAL),
      triangle_to_tree(device, "triangle_to_tree", MEM_GLOBAL),
      particles(device, "particles", MEM_GLOBAL),
      svm_nodes(device, "svm_nodes", MEM_GLOBAL),
      shaders(device, "shaders", MEM_GLOBAL),
      lookup_table(device, "lookup_table", MEM_GLOBAL),
      sample_pattern_lut(device, "sample_pattern_lut", MEM_GLOBAL),
      ies_lights(device, "ies", MEM_GLOBAL)
{
  memset((void *)&data, 0, sizeof(data));
}

CCL_NAMESPACE_END
