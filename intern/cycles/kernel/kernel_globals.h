/*
 * Copyright 2011, Blender Foundation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/* Constant Globals */

#ifdef __KERNEL_CPU__

#ifdef WITH_OSL
#include "osl_globals.h"
#endif

CCL_NAMESPACE_BEGIN

/* On the CPU, we pass along the struct KernelGlobals to nearly everywhere in
   the kernel, to access constant data. These are all stored as "textures", but
   these are really just standard arrays. We can't use actually globals because
   multiple renders may be running inside the same process. */
typedef struct KernelGlobals {

#else

/* On the GPU, constant memory textures must be globals, so we can't put them
   into a struct. As a result we don't actually use this struct and use actual
   globals and simply pass along a NULL pointer everywhere, which we hope gets
   optimized out. */
#ifdef __KERNEL_CUDA__
typedef struct KernelGlobals {} KernelGlobals;
#endif

#endif

/* globals */
__constant KernelData __data;

#ifndef __KERNEL_OPENCL__

/* bvh */
texture_float4 __bvh_nodes;
texture_float4 __tri_woop;
texture_uint __prim_index;
texture_uint __prim_object;
texture_uint __object_node;

/* objects */
texture_float4 __objects;

/* triangles */
texture_float4 __tri_normal;
texture_float4 __tri_vnormal;
texture_float4 __tri_vindex;
texture_float4 __tri_verts;

/* attributes */
texture_uint4 __attributes_map;
texture_float __attributes_float;
texture_float4 __attributes_float3;

/* lights */
texture_float4 __light_distribution;
texture_float4 __light_point;

/* shaders */
texture_uint4 __svm_nodes;

/* camera/film */
texture_float __filter_table;
texture_float __response_curve_R;
texture_float __response_curve_G;
texture_float __response_curve_B;

/* sobol */
texture_uint __sobol_directions;

/* image */
texture_image_uchar4 __tex_image_000;
texture_image_uchar4 __tex_image_001;
texture_image_uchar4 __tex_image_002;
texture_image_uchar4 __tex_image_003;
texture_image_uchar4 __tex_image_004;
texture_image_uchar4 __tex_image_005;
texture_image_uchar4 __tex_image_006;
texture_image_uchar4 __tex_image_007;
texture_image_uchar4 __tex_image_008;
texture_image_uchar4 __tex_image_009;
texture_image_uchar4 __tex_image_010;
texture_image_uchar4 __tex_image_011;
texture_image_uchar4 __tex_image_012;
texture_image_uchar4 __tex_image_013;
texture_image_uchar4 __tex_image_014;
texture_image_uchar4 __tex_image_015;
texture_image_uchar4 __tex_image_016;
texture_image_uchar4 __tex_image_017;
texture_image_uchar4 __tex_image_018;
texture_image_uchar4 __tex_image_019;
texture_image_uchar4 __tex_image_020;
texture_image_uchar4 __tex_image_021;
texture_image_uchar4 __tex_image_022;
texture_image_uchar4 __tex_image_023;
texture_image_uchar4 __tex_image_024;
texture_image_uchar4 __tex_image_025;
texture_image_uchar4 __tex_image_026;
texture_image_uchar4 __tex_image_027;
texture_image_uchar4 __tex_image_028;
texture_image_uchar4 __tex_image_029;
texture_image_uchar4 __tex_image_030;
texture_image_uchar4 __tex_image_031;
texture_image_uchar4 __tex_image_032;
texture_image_uchar4 __tex_image_033;
texture_image_uchar4 __tex_image_034;
texture_image_uchar4 __tex_image_035;
texture_image_uchar4 __tex_image_036;
texture_image_uchar4 __tex_image_037;
texture_image_uchar4 __tex_image_038;
texture_image_uchar4 __tex_image_039;
texture_image_uchar4 __tex_image_040;
texture_image_uchar4 __tex_image_041;
texture_image_uchar4 __tex_image_042;
texture_image_uchar4 __tex_image_043;
texture_image_uchar4 __tex_image_044;
texture_image_uchar4 __tex_image_045;
texture_image_uchar4 __tex_image_046;
texture_image_uchar4 __tex_image_047;
texture_image_uchar4 __tex_image_048;
texture_image_uchar4 __tex_image_049;
texture_image_uchar4 __tex_image_050;
texture_image_uchar4 __tex_image_051;
texture_image_uchar4 __tex_image_052;
texture_image_uchar4 __tex_image_053;
texture_image_uchar4 __tex_image_054;
texture_image_uchar4 __tex_image_055;
texture_image_uchar4 __tex_image_056;
texture_image_uchar4 __tex_image_057;
texture_image_uchar4 __tex_image_058;
texture_image_uchar4 __tex_image_059;
texture_image_uchar4 __tex_image_060;
texture_image_uchar4 __tex_image_061;
texture_image_uchar4 __tex_image_062;
texture_image_uchar4 __tex_image_063;
texture_image_uchar4 __tex_image_064;
texture_image_uchar4 __tex_image_065;
texture_image_uchar4 __tex_image_066;
texture_image_uchar4 __tex_image_067;
texture_image_uchar4 __tex_image_068;
texture_image_uchar4 __tex_image_069;
texture_image_uchar4 __tex_image_070;
texture_image_uchar4 __tex_image_071;
texture_image_uchar4 __tex_image_072;
texture_image_uchar4 __tex_image_073;
texture_image_uchar4 __tex_image_074;
texture_image_uchar4 __tex_image_075;
texture_image_uchar4 __tex_image_076;
texture_image_uchar4 __tex_image_077;
texture_image_uchar4 __tex_image_078;
texture_image_uchar4 __tex_image_079;
texture_image_uchar4 __tex_image_080;
texture_image_uchar4 __tex_image_081;
texture_image_uchar4 __tex_image_082;
texture_image_uchar4 __tex_image_083;
texture_image_uchar4 __tex_image_084;
texture_image_uchar4 __tex_image_085;
texture_image_uchar4 __tex_image_086;
texture_image_uchar4 __tex_image_087;
texture_image_uchar4 __tex_image_088;
texture_image_uchar4 __tex_image_089;
texture_image_uchar4 __tex_image_090;
texture_image_uchar4 __tex_image_091;
texture_image_uchar4 __tex_image_092;
texture_image_uchar4 __tex_image_093;
texture_image_uchar4 __tex_image_094;
texture_image_uchar4 __tex_image_095;
texture_image_uchar4 __tex_image_096;
texture_image_uchar4 __tex_image_097;
texture_image_uchar4 __tex_image_098;
texture_image_uchar4 __tex_image_099;

#endif

#ifdef __KERNEL_CPU__

#ifdef WITH_OSL

/* On the CPU, we also have the OSL globals here. Most data structures are shared
   with SVM, the difference is in the shaders and object/mesh attributes. */

OSLGlobals osl;

#endif

} KernelGlobals;
#endif

CCL_NAMESPACE_END

