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
#define KERNEL_TEX(type, ttype, name)
#endif

#ifndef KERNEL_IMAGE_TEX
#define KERNEL_IMAGE_TEX(type, ttype, name)
#endif

/* bvh */
KERNEL_TEX(float4, texture_float4, __bvh_nodes)
KERNEL_TEX(float4, texture_float4, __tri_woop)
KERNEL_TEX(uint, texture_uint, __prim_type)
KERNEL_TEX(uint, texture_uint, __prim_visibility)
KERNEL_TEX(uint, texture_uint, __prim_index)
KERNEL_TEX(uint, texture_uint, __prim_object)
KERNEL_TEX(uint, texture_uint, __object_node)

/* objects */
KERNEL_TEX(float4, texture_float4, __objects)
KERNEL_TEX(float4, texture_float4, __objects_vector)

/* triangles */
KERNEL_TEX(uint, texture_uint, __tri_shader)
KERNEL_TEX(float4, texture_float4, __tri_vnormal)
KERNEL_TEX(float4, texture_float4, __tri_vindex)
KERNEL_TEX(float4, texture_float4, __tri_verts)

/* curves */
KERNEL_TEX(float4, texture_float4, __curves)
KERNEL_TEX(float4, texture_float4, __curve_keys)

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

/* full-float image */
KERNEL_IMAGE_TEX(float4, texture_image_float4, __tex_image_float_000)
KERNEL_IMAGE_TEX(float4, texture_image_float4, __tex_image_float_001)
KERNEL_IMAGE_TEX(float4, texture_image_float4, __tex_image_float_002)
KERNEL_IMAGE_TEX(float4, texture_image_float4, __tex_image_float_003)
KERNEL_IMAGE_TEX(float4, texture_image_float4, __tex_image_float_004)

/* image */
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_005)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_006)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_007)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_008)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_009)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_010)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_011)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_012)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_013)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_014)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_015)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_016)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_017)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_018)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_019)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_020)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_021)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_022)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_023)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_024)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_025)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_026)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_027)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_028)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_029)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_030)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_031)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_032)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_033)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_034)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_035)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_036)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_037)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_038)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_039)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_040)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_041)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_042)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_043)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_044)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_045)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_046)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_047)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_048)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_049)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_050)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_051)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_052)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_053)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_054)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_055)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_056)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_057)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_058)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_059)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_060)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_061)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_062)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_063)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_064)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_065)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_066)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_067)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_068)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_069)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_070)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_071)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_072)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_073)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_074)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_075)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_076)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_077)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_078)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_079)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_080)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_081)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_082)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_083)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_084)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_085)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_086)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_087)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_088)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_089)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_090)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_091)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_092)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_093)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_094)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_095)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_096)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_097)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_098)

/* Kepler and above */
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_099)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_100)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_101)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_102)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_103)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_104)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_105)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_106)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_107)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_108)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_109)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_110)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_111)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_112)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_113)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_114)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_115)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_116)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_117)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_118)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_119)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_120)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_121)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_122)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_123)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_124)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_125)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_126)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_127)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_128)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_129)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_130)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_131)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_132)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_133)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_134)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_135)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_136)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_137)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_138)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_139)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_140)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_141)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_142)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_143)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_144)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_145)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_146)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_147)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_148)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_149)
KERNEL_IMAGE_TEX(uchar4, texture_image_uchar4, __tex_image_150)

/* packed image (opencl) */
KERNEL_TEX(uchar4, texture_uchar4, __tex_image_packed)
KERNEL_TEX(uint4, texture_uint4, __tex_image_packed_info)

#undef KERNEL_TEX
#undef KERNEL_IMAGE_TEX


