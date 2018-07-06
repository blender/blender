/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef KERNEL_TEX
#  define KERNEL_TEX(type, name)
#endif

/* bvh */
KERNEL_TEX(float4, __bvh_nodes)
KERNEL_TEX(float4, __bvh_leaf_nodes)
KERNEL_TEX(float4, __prim_tri_verts)
KERNEL_TEX(uint, __prim_tri_index)
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

/* cameras */
KERNEL_TEX(DecomposedTransform, __camera_motion)

/* triangles */
KERNEL_TEX(uint, __tri_shader)
KERNEL_TEX(float4, __tri_vnormal)
KERNEL_TEX(uint4, __tri_vindex)
KERNEL_TEX(uint, __tri_patch)
KERNEL_TEX(float2, __tri_patch_uv)

/* curves */
KERNEL_TEX(float4, __curves)
KERNEL_TEX(float4, __curve_keys)

/* patches */
KERNEL_TEX(uint, __patches)

/* attributes */
KERNEL_TEX(uint4, __attributes_map)
KERNEL_TEX(float, __attributes_float)
KERNEL_TEX(float4, __attributes_float3)
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
KERNEL_TEX(uint, __sobol_directions)

/* image textures */
KERNEL_TEX(TextureInfo, __texture_info)

/* ies lights */
KERNEL_TEX(float, __ies)

#undef KERNEL_TEX
