/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "gpu_interface_info.hh"
#include "gpu_shader_create_info.hh"

GPU_SHADER_INTERFACE_INFO(gpu_seq_strip_iface, "")
    .no_perspective(Type::VEC2, "co_interp")
    .flat(Type::UINT, "strip_id");

GPU_SHADER_CREATE_INFO(gpu_shader_sequencer_strips)
    .vertex_out(gpu_seq_strip_iface)
    .fragment_out(0, Type::VEC4, "fragColor")
    .push_constant(Type::MAT4, "ModelViewProjectionMatrix")
    .uniform_buf(0, "SeqStripDrawData", "strip_data[GPU_SEQ_STRIP_DRAW_DATA_LEN]")
    .uniform_buf(1, "SeqContextDrawData", "context_data")
    .typedef_source("GPU_shader_shared.hh")
    .vertex_source("gpu_shader_sequencer_strips_vert.glsl")
    .fragment_source("gpu_shader_sequencer_strips_frag.glsl")
    .do_static_compilation(true);
