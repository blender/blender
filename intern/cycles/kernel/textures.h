/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#ifndef KERNEL_TEX
#  define KERNEL_TEX(type, name)
#endif

/* BVH2, not used for OptiX or Embree. */
KERNEL_TEX(float4, __bvh_nodes)
KERNEL_TEX(float4, __bvh_leaf_nodes)
KERNEL_TEX(uint, __prim_type)
KERNEL_TEX(uint, __prim_visibility)
KERNEL_TEX(uint, __prim_index)
KERNEL_TEX(uint, __prim_object)
KERNEL_TEX(uint, __object_node)
KERNEL_TEX(float2, __prim_time)

/* objects */
KERNEL_TEX(KernelObject, __objects)
KERNEL_TEX(Transform, __object_motion_pass)
KERNEL_TEX(DecomposedTransform, __object_motion)
KERNEL_TEX(uint, __object_flag)
KERNEL_TEX(float, __object_volume_step)
KERNEL_TEX(uint, __object_prim_offset)

/* cameras */
KERNEL_TEX(DecomposedTransform, __camera_motion)

/* triangles */
KERNEL_TEX(uint, __tri_shader)
KERNEL_TEX(packed_float3, __tri_vnormal)
KERNEL_TEX(uint4, __tri_vindex)
KERNEL_TEX(uint, __tri_patch)
KERNEL_TEX(float2, __tri_patch_uv)
KERNEL_TEX(packed_float3, __tri_verts)

/* curves */
KERNEL_TEX(KernelCurve, __curves)
KERNEL_TEX(float4, __curve_keys)
KERNEL_TEX(KernelCurveSegment, __curve_segments)

/* patches */
KERNEL_TEX(uint, __patches)

/* pointclouds */
KERNEL_TEX(float4, __points)
KERNEL_TEX(uint, __points_shader)

/* attributes */
KERNEL_TEX(AttributeMap, __attributes_map)
KERNEL_TEX(float, __attributes_float)
KERNEL_TEX(float2, __attributes_float2)
KERNEL_TEX(packed_float3, __attributes_float3)
KERNEL_TEX(float4, __attributes_float4)
KERNEL_TEX(uchar4, __attributes_uchar4)

/* lights */
KERNEL_TEX(KernelLightDistribution, __light_distribution)
KERNEL_TEX(KernelLight, __lights)
KERNEL_TEX(float2, __light_background_marginal_cdf)
KERNEL_TEX(float2, __light_background_conditional_cdf)

/* particles */
KERNEL_TEX(KernelParticle, __particles)

/* shaders */
KERNEL_TEX(uint4, __svm_nodes)
KERNEL_TEX(KernelShader, __shaders)

/* lookup tables */
KERNEL_TEX(float, __lookup_table)

/* sobol */
KERNEL_TEX(float, __sample_pattern_lut)

/* image textures */
KERNEL_TEX(TextureInfo, __texture_info)

/* ies lights */
KERNEL_TEX(float, __ies)

#undef KERNEL_TEX
