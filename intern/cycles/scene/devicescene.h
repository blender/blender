/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#ifndef __DEVICESCENE_H__
#define __DEVICESCENE_H__

#include "device/device.h"
#include "device/memory.h"

#include "util/types.h"
#include "util/vector.h"

CCL_NAMESPACE_BEGIN

class DeviceScene {
 public:
  /* BVH */
  device_vector<int4> bvh_nodes;
  device_vector<int4> bvh_leaf_nodes;
  device_vector<int> object_node;
  device_vector<int> prim_type;
  device_vector<uint> prim_visibility;
  device_vector<int> prim_index;
  device_vector<int> prim_object;
  device_vector<float2> prim_time;

  /* mesh */
  device_vector<packed_float3> tri_verts;
  device_vector<uint> tri_shader;
  device_vector<packed_float3> tri_vnormal;
  device_vector<packed_uint3> tri_vindex;
  device_vector<uint> tri_patch;
  device_vector<float2> tri_patch_uv;

  device_vector<KernelCurve> curves;
  device_vector<float4> curve_keys;
  device_vector<KernelCurveSegment> curve_segments;

  device_vector<uint> patches;

  /* point-cloud */
  device_vector<float4> points;
  device_vector<uint> points_shader;

  /* objects */
  device_vector<KernelObject> objects;
  device_vector<Transform> object_motion_pass;
  device_vector<DecomposedTransform> object_motion;
  device_vector<uint> object_flag;
  device_vector<float> object_volume_step;
  device_vector<uint> object_prim_offset;

  /* cameras */
  device_vector<DecomposedTransform> camera_motion;

  /* attributes */
  device_vector<AttributeMap> attributes_map;
  device_vector<float> attributes_float;
  device_vector<float2> attributes_float2;
  device_vector<packed_float3> attributes_float3;
  device_vector<float4> attributes_float4;
  device_vector<uchar4> attributes_uchar4;

  /* lights */
  device_vector<KernelLightDistribution> light_distribution;
  device_vector<KernelLight> lights;
  device_vector<float2> light_background_marginal_cdf;
  device_vector<float2> light_background_conditional_cdf;

  /* light tree */
  device_vector<KernelLightTreeNode> light_tree_nodes;
  device_vector<KernelLightTreeEmitter> light_tree_emitters;
  device_vector<uint> light_to_tree;
  device_vector<uint> object_to_tree;
  device_vector<uint> object_lookup_offset;
  device_vector<uint> triangle_to_tree;

  /* particles */
  device_vector<KernelParticle> particles;

  /* shaders */
  device_vector<int4> svm_nodes;
  device_vector<KernelShader> shaders;

  /* lookup tables */
  device_vector<float> lookup_table;

  /* integrator */
  device_vector<float> sample_pattern_lut;

  /* IES lights */
  device_vector<float> ies_lights;

  KernelData data;

  DeviceScene(Device *device);
};

CCL_NAMESPACE_END

#endif /*  __DEVICESCENE_H__ */
