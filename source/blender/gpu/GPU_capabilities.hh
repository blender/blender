/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * GPU Capabilities & workarounds
 * This module expose the reported implementation limits & enabled
 * workaround for drivers that needs specific code-paths.
 */

#pragma once

#include "BLI_sys_types.h"

int GPU_max_texture_size();
int GPU_max_texture_3d_size();
int GPU_max_texture_layers();
int GPU_max_textures();
int GPU_max_textures_vert();
int GPU_max_textures_geom();
int GPU_max_textures_frag();
int GPU_max_images();
int GPU_max_work_group_count(int index);
int GPU_max_work_group_size(int index);
int GPU_max_uniforms_vert();
int GPU_max_uniforms_frag();
int GPU_max_batch_indices();
int GPU_max_batch_vertices();
int GPU_max_vertex_attribs();
int GPU_max_varying_floats();
int GPU_max_shader_storage_buffer_bindings();
int GPU_max_compute_shader_storage_blocks();
int GPU_max_samplers();
size_t GPU_max_storage_buffer_size();

int GPU_extensions_len();
const char *GPU_extension_get(int i);

int GPU_texture_size_with_limit(int res);

bool GPU_mip_render_workaround();
bool GPU_depth_blitting_workaround();
bool GPU_use_main_context_workaround();
bool GPU_use_hq_normals_workaround();
bool GPU_clear_viewport_workaround();
bool GPU_crappy_amd_driver();

bool GPU_geometry_shader_support();
bool GPU_compute_shader_support();
bool GPU_shader_draw_parameters_support();
bool GPU_hdr_support();
bool GPU_texture_view_support();
bool GPU_stencil_export_support();

bool GPU_mem_stats_supported();
void GPU_mem_stats_get(int *totalmem, int *freemem);

/**
 * Return support for the active context + window.
 */
bool GPU_stereo_quadbuffer_support();

int GPU_minimum_per_vertex_stride();
bool GPU_transform_feedback_support();
