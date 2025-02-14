/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#ifdef GPU_SHADER
#  include "gpu_glsl_cpp_stubs.hh"

#  include "draw_common_shader_shared.hh"
#endif

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(draw_globals)
TYPEDEF_SOURCE("draw_common_shader_shared.hh")
UNIFORM_BUF_FREQ(OVERLAY_GLOBALS_SLOT, GlobalsUboStorage, globalsBlock, PASS)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_clipped)
DEFINE("OVERLAY_NEXT") /* Needed for view_clipping_lib. */
DEFINE("USE_WORLD_CLIP_PLANES")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_select)
DEFINE("SELECT_ENABLE")
ADDITIONAL_INFO(select_id_patch)
GPU_SHADER_CREATE_END()

#define OVERLAY_INFO_CLIP_VARIATION(name) \
  GPU_SHADER_CREATE_INFO(name##_clipped) \
  DO_STATIC_COMPILATION() \
  ADDITIONAL_INFO(name) \
  ADDITIONAL_INFO(overlay_clipped) \
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
  ADDITIONAL_INFO(draw_modelmat_new) \
  GPU_SHADER_CREATE_END() \
\
  GPU_SHADER_CREATE_INFO(name##_selectable) \
  DO_STATIC_COMPILATION() \
  ADDITIONAL_INFO(base_info) \
  ADDITIONAL_INFO(draw_modelmat_new_with_custom_id) \
  ADDITIONAL_INFO(overlay_select) \
  GPU_SHADER_CREATE_END() \
\
  OVERLAY_INFO_CLIP_VARIATION(name) \
  OVERLAY_INFO_CLIP_VARIATION(name##_selectable)
