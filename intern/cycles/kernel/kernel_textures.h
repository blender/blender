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
#  define KERNEL_TEX(type, ttype, name)
#endif

#ifndef KERNEL_IMAGE_TEX
#  define KERNEL_IMAGE_TEX(type, ttype, name)
#endif

/* bvh */
KERNEL_TEX(float4, texture_float4, __bvh_nodes)
KERNEL_TEX(float4, texture_float4, __bvh_leaf_nodes)
KERNEL_TEX(float4, texture_float4, __prim_tri_verts)
KERNEL_TEX(uint, texture_uint, __prim_tri_index)
KERNEL_TEX(uint, texture_uint, __prim_type)
KERNEL_TEX(uint, texture_uint, __prim_visibility)
KERNEL_TEX(uint, texture_uint, __prim_index)
KERNEL_TEX(uint, texture_uint, __prim_object)
KERNEL_TEX(uint, texture_uint, __object_node)
KERNEL_TEX(float2, texture_float2, __prim_time)

/* objects */
KERNEL_TEX(float4, texture_float4, __objects)
KERNEL_TEX(float4, texture_float4, __objects_vector)

/* triangles */
KERNEL_TEX(uint, texture_uint, __tri_shader)
KERNEL_TEX(float4, texture_float4, __tri_vnormal)
KERNEL_TEX(uint4, texture_uint4, __tri_vindex)
KERNEL_TEX(uint, texture_uint, __tri_patch)
KERNEL_TEX(float2, texture_float2, __tri_patch_uv)

/* curves */
KERNEL_TEX(float4, texture_float4, __curves)
KERNEL_TEX(float4, texture_float4, __curve_keys)

/* patches */
KERNEL_TEX(uint, texture_uint, __patches)

/* attributes */
KERNEL_TEX(uint4, texture_uint4, __attributes_map)
KERNEL_TEX(float, texture_float, __attributes_float)
KERNEL_TEX(float4, texture_float4, __attributes_float3)
KERNEL_TEX(uchar4, texture_uchar4, __attributes_uchar4)

/* lights */
KERNEL_TEX(float4, texture_float4, __light_distribution)
KERNEL_TEX(float4, texture_float4, __light_data)
KERNEL_TEX(float2, texture_float2, __light_background_marginal_cdf)
KERNEL_TEX(float2, texture_float2, __light_background_conditional_cdf)

/* particles */
KERNEL_TEX(float4, texture_float4, __particles)

/* shaders */
KERNEL_TEX(uint4, texture_uint4, __svm_nodes)
KERNEL_TEX(uint, texture_uint, __shader_flag)
KERNEL_TEX(uint, texture_uint, __object_flag)

/* lookup tables */
KERNEL_TEX(float, texture_float, __lookup_table)

/* sobol */
KERNEL_TEX(uint, texture_uint, __sobol_directions)

#ifdef __KERNEL_CUDA__
#  if __CUDA_ARCH__ < 300
/* full-float image */
KERNEL_IMAGE_TEX(float4, texture_image_float4, __tex_image_float4_000)
KERNEL_IMAGE_TEX(float4, texture_image_float4, __tex_image_float4_001)
KERNEL_IMAGE_TEX(float4, texture_image_float4, __tex_image_float4_002)
KERNEL_IMAGE_TEX(float4, texture_image_float4, __tex_image_float4_003)
KERNEL_IMAGE_TEX(float4, texture_image_float4, __tex_image_float4_004)

KERNEL_IMAGE_TEX(float4, texture_image3d_float4, __tex_image_float4_3d_000)
KERNEL_IMAGE_TEX(float4, texture_image3d_float4, __tex_image_float4_3d_001)
KERNEL_IMAGE_TEX(float4, texture_image3d_float4, __tex_image_float4_3d_002)
KERNEL_IMAGE_TEX(float4, texture_image3d_float4, __tex_image_float4_3d_003)
KERNEL_IMAGE_TEX(float4, texture_image3d_float4, __tex_image_float4_3d_004)

/* image */
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_005)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_006)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_007)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_008)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_009)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_010)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_011)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_012)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_013)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_014)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_015)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_016)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_017)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_018)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_019)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_020)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_021)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_022)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_023)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_024)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_025)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_026)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_027)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_028)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_029)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_030)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_031)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_032)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_033)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_034)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_035)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_036)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_037)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_038)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_039)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_040)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_041)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_042)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_043)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_044)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_045)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_046)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_047)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_048)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_049)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_050)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_051)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_052)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_053)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_054)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_055)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_056)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_057)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_058)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_059)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_060)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_061)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_062)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_063)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_064)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_065)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_066)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_067)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_068)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_069)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_070)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_071)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_072)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_073)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_074)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_075)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_076)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_077)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_078)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_079)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_080)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_081)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_082)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_083)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_084)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_085)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_086)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_087)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_byte4_088)

#  else
/* bindless textures */
KERNEL_TEX(uint, texture_uint, __bindless_mapping)
#  endif
#endif

/* packed image (opencl) */
KERNEL_TEX(uchar4, texture_uchar4, __tex_image_byte4_packed)
KERNEL_TEX(float4, texture_float4, __tex_image_float4_packed)
KERNEL_TEX(uchar, texture_uchar, __tex_image_byte_packed)
KERNEL_TEX(float, texture_float, __tex_image_float_packed)
KERNEL_TEX(uint4, texture_uint4, __tex_image_packed_info)

#undef KERNEL_TEX
#undef KERNEL_IMAGE_TEX


