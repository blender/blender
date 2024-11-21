/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_glsl_cpp_stubs.hh"

#  include "GPU_shader_shared.hh"
#  include "gpu_srgb_to_framebuffer_space_info.hh"
#  define SMOOTH_WIDTH 1.0
#endif

#include "gpu_interface_info.hh"
#include "gpu_shader_create_info.hh"

GPU_SHADER_NAMED_INTERFACE_INFO(gpu_shader_3D_polyline_iface, interp)
SMOOTH(VEC4, final_color)
SMOOTH(FLOAT, clip)
GPU_SHADER_NAMED_INTERFACE_END(interp)

GPU_SHADER_NAMED_INTERFACE_INFO(gpu_shader_3D_polyline_noperspective_iface, interp_noperspective)
NO_PERSPECTIVE(FLOAT, smoothline)
GPU_SHADER_NAMED_INTERFACE_END(interp_noperspective)

GPU_SHADER_CREATE_INFO(gpu_shader_3D_polyline)
DEFINE_VALUE("SMOOTH_WIDTH", "1.0")
PUSH_CONSTANT(MAT4, ModelViewProjectionMatrix)
PUSH_CONSTANT(VEC2, viewportSize)
PUSH_CONSTANT(FLOAT, lineWidth)
PUSH_CONSTANT(BOOL, lineSmooth)
VERTEX_IN(0, VEC3, pos)
VERTEX_OUT(gpu_shader_3D_polyline_iface)
VERTEX_OUT(gpu_shader_3D_polyline_noperspective_iface)
GEOMETRY_LAYOUT(PrimitiveIn::LINES, PrimitiveOut::TRIANGLE_STRIP, 4)
GEOMETRY_OUT(gpu_shader_3D_polyline_iface)
GEOMETRY_OUT(gpu_shader_3D_polyline_noperspective_iface)
FRAGMENT_OUT(0, VEC4, fragColor)
VERTEX_SOURCE("gpu_shader_3D_polyline_vert.glsl")
GEOMETRY_SOURCE("gpu_shader_3D_polyline_geom.glsl")
FRAGMENT_SOURCE("gpu_shader_3D_polyline_frag.glsl")
ADDITIONAL_INFO(gpu_srgb_to_framebuffer_space)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpu_shader_3D_polyline_no_geom)
DEFINE_VALUE("SMOOTH_WIDTH", "1.0")
PUSH_CONSTANT(MAT4, ModelViewProjectionMatrix)
PUSH_CONSTANT(VEC2, viewportSize)
PUSH_CONSTANT(FLOAT, lineWidth)
PUSH_CONSTANT(BOOL, lineSmooth)
VERTEX_IN(0, VEC3, pos)
VERTEX_OUT(gpu_shader_3D_polyline_iface)
VERTEX_OUT(gpu_shader_3D_polyline_noperspective_iface)
FRAGMENT_OUT(0, VEC4, fragColor)
VERTEX_SOURCE("gpu_shader_3D_polyline_vert_no_geom.glsl")
FRAGMENT_SOURCE("gpu_shader_3D_polyline_frag.glsl")
ADDITIONAL_INFO(gpu_srgb_to_framebuffer_space)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpu_shader_3D_polyline_uniform_color)
DO_STATIC_COMPILATION()
DEFINE("UNIFORM")
PUSH_CONSTANT(VEC4, color)
ADDITIONAL_INFO(gpu_shader_3D_polyline)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpu_shader_3D_polyline_uniform_color_no_geom)
METAL_BACKEND_ONLY()
DO_STATIC_COMPILATION()
DEFINE("UNIFORM")
PUSH_CONSTANT(VEC4, color)
ADDITIONAL_INFO(gpu_shader_3D_polyline_no_geom)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpu_shader_3D_polyline_uniform_color_clipped)
DO_STATIC_COMPILATION()
/* TODO(fclem): Put in a UBO to fit the 128byte requirement. */
PUSH_CONSTANT(MAT4, ModelMatrix)
PUSH_CONSTANT(VEC4, ClipPlane)
DEFINE("CLIP")
ADDITIONAL_INFO(gpu_shader_3D_polyline_uniform_color)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpu_shader_3D_polyline_uniform_color_clipped_no_geom)
METAL_BACKEND_ONLY()
DO_STATIC_COMPILATION()
/* TODO(fclem): Put in an UBO to fit the 128byte requirement. */
PUSH_CONSTANT(MAT4, ModelMatrix)
PUSH_CONSTANT(VEC4, ClipPlane)
DEFINE("CLIP")
ADDITIONAL_INFO(gpu_shader_3D_polyline_uniform_color_no_geom)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpu_shader_3D_polyline_flat_color)
DO_STATIC_COMPILATION()
DEFINE("FLAT")
VERTEX_IN(1, VEC4, color)
ADDITIONAL_INFO(gpu_shader_3D_polyline)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpu_shader_3D_polyline_flat_color_no_geom)
METAL_BACKEND_ONLY()
DO_STATIC_COMPILATION()
DEFINE("FLAT")
VERTEX_IN(1, VEC4, color)
ADDITIONAL_INFO(gpu_shader_3D_polyline_no_geom)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpu_shader_3D_polyline_smooth_color)
DO_STATIC_COMPILATION()
DEFINE("SMOOTH")
VERTEX_IN(1, VEC4, color)
ADDITIONAL_INFO(gpu_shader_3D_polyline)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpu_shader_3D_polyline_smooth_color_no_geom)
METAL_BACKEND_ONLY()
DO_STATIC_COMPILATION()
DEFINE("SMOOTH")
VERTEX_IN(1, VEC4, color)
ADDITIONAL_INFO(gpu_shader_3D_polyline_no_geom)
GPU_SHADER_CREATE_END()
