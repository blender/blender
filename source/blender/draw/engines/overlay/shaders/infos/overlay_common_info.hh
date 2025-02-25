/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_glsl_cpp_stubs.hh"

#  include "draw_common_shader_shared.hh"
#  include "select_shader_shared.hh"
#endif

#include "select_defines.hh"

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(draw_globals)
TYPEDEF_SOURCE("draw_common_shader_shared.hh")
UNIFORM_BUF_FREQ(OVERLAY_GLOBALS_SLOT, GlobalsUboStorage, globalsBlock, PASS)
GPU_SHADER_CREATE_END()

GPU_SHADER_INTERFACE_INFO(select_id_patch_iface)
FLAT(INT, select_id)
GPU_SHADER_INTERFACE_END()

/* Used to patch overlay shaders. */
GPU_SHADER_CREATE_INFO(select_id_patch)
TYPEDEF_SOURCE("select_shader_shared.hh")
VERTEX_OUT(select_id_patch_iface)
/* Need to make sure the depth & stencil comparison runs before the fragment shader. */
EARLY_FRAGMENT_TEST(true)
UNIFORM_BUF(SELECT_DATA, SelectInfoData, select_info_buf)
/* Select IDs for instanced draw-calls not using #PassMain. */
STORAGE_BUF(SELECT_ID_IN, READ, int, in_select_buf[])
/* Stores the result of the whole selection drawing. Content depends on selection mode. */
STORAGE_BUF(SELECT_ID_OUT, READ_WRITE, uint, out_select_buf[])
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_select)
DEFINE("SELECT_ENABLE")
ADDITIONAL_INFO(select_id_patch)
GPU_SHADER_CREATE_END()

#define OVERLAY_INFO_CLIP_VARIATION(name) \
  GPU_SHADER_CREATE_INFO(name##_clipped) \
  DO_STATIC_COMPILATION() \
  ADDITIONAL_INFO(name) \
  ADDITIONAL_INFO(drw_clipped) \
  GPU_SHADER_CREATE_END()

#define OVERLAY_INFO_SELECT_VARIATION(name) \
  GPU_SHADER_CREATE_INFO(name##_selectable) \
  DO_STATIC_COMPILATION() \
  ADDITIONAL_INFO(name) \
  ADDITIONAL_INFO(overlay_select) \
  GPU_SHADER_CREATE_END()

#define OVERLAY_INFO_VARIATIONS(name) \
  OVERLAY_INFO_SELECT_VARIATION(name) \
  OVERLAY_INFO_CLIP_VARIATION(name) \
  OVERLAY_INFO_CLIP_VARIATION(name##_selectable)

#define OVERLAY_INFO_VARIATIONS_MODELMAT(name, base_info) \
  GPU_SHADER_CREATE_INFO(name) \
  DO_STATIC_COMPILATION() \
  ADDITIONAL_INFO(base_info) \
  ADDITIONAL_INFO(draw_modelmat) \
  GPU_SHADER_CREATE_END() \
\
  GPU_SHADER_CREATE_INFO(name##_selectable) \
  DO_STATIC_COMPILATION() \
  ADDITIONAL_INFO(base_info) \
  ADDITIONAL_INFO(draw_modelmat_with_custom_id) \
  ADDITIONAL_INFO(overlay_select) \
  GPU_SHADER_CREATE_END() \
\
  OVERLAY_INFO_CLIP_VARIATION(name) \
  OVERLAY_INFO_CLIP_VARIATION(name##_selectable)
