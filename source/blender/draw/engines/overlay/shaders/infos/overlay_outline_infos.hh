/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_shader_compat.hh"

#  include "draw_object_infos_infos.hh"
#  include "draw_view_infos.hh"

#  include "gpu_index_load_infos.hh"
#  include "gpu_shader_create_info.hh"

#  include "overlay_fullscreen_infos.hh"
#  include "overlay_shader_shared.hh"
#endif

#ifdef GLSL_CPP_STUBS
#  define CURVES_SHADER
#  define DRW_HAIR_INFO

#  define POINTCLOUD_SHADER
#  define DRW_POINTCLOUD_INFO
#endif

#include "overlay_common_infos.hh"

/* -------------------------------------------------------------------- */
/** \name Outline Pre-pass
 * \{ */
/** \} */

/* -------------------------------------------------------------------- */
/** \name Outline Rendering
 * \{ */

GPU_SHADER_CREATE_INFO(overlay_outline_detect)
DO_STATIC_COMPILATION()
PUSH_CONSTANT(float, alpha_occlu)
PUSH_CONSTANT(bool, is_xray_wires)
PUSH_CONSTANT(bool, do_anti_aliasing)
PUSH_CONSTANT(bool, do_thick_outlines)
SAMPLER(0, usampler2D, outline_id_tx)
SAMPLER(1, sampler2DDepth, outline_depth_tx)
SAMPLER(2, sampler2DDepth, scene_depth_tx)
FRAGMENT_OUT(0, float4, frag_color)
FRAGMENT_OUT(1, float4, line_output)
FRAGMENT_SOURCE("overlay_outline_detect_frag.glsl")
ADDITIONAL_INFO(overlay_fullscreen)
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

/** \} */
