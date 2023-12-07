/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "eevee_defines.hh"
#include "gpu_shader_create_info.hh"

GPU_SHADER_INTERFACE_INFO(eevee_lookdev_display_iface, "")
    .smooth(Type::VEC2, "uv_coord")
    .flat(Type::UINT, "sphere_id");

GPU_SHADER_CREATE_INFO(eevee_lookdev_display)
    .vertex_source("eevee_lookdev_display_vert.glsl")
    .vertex_out(eevee_lookdev_display_iface)
    .push_constant(Type::VEC2, "viewportSize")
    .push_constant(Type::VEC2, "invertedViewportSize")
    .push_constant(Type::IVEC2, "anchor")
    .sampler(0, ImageType::FLOAT_2D, "metallic_tx")
    .sampler(1, ImageType::FLOAT_2D, "diffuse_tx")
    .fragment_out(0, Type::VEC4, "out_color")
    .fragment_source("eevee_lookdev_display_frag.glsl")
    .depth_write(DepthWrite::ANY)
    .do_static_compilation(true);
