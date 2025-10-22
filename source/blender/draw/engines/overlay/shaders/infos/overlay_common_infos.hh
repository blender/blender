/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#ifdef GPU_SHADER
#  pragma once
#  include "BLI_utildefines_variadic.h"

#  include "gpu_shader_compat.hh"

#  include "overlay_shader_shared.hh"
#  include "select_shader_shared.hh"
#endif

#include "select_defines.hh"

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(draw_globals)
TYPEDEF_SOURCE("overlay_shader_shared.hh")
UNIFORM_BUF_FREQ(OVERLAY_GLOBALS_SLOT, UniformData, uniform_buf, PASS)
GPU_SHADER_CREATE_END()

GPU_SHADER_INTERFACE_INFO(select_id_patch_iface)
FLAT(uint, select_id)
GPU_SHADER_INTERFACE_END()

/* Used to patch overlay shaders. */
GPU_SHADER_CREATE_INFO(select_id_patch)
TYPEDEF_SOURCE("select_shader_shared.hh")
VERTEX_OUT(select_id_patch_iface)
/* This is on purpose. We want all fragment to be considered during selection.
 * Selection in object mode is not yet depth aware (see #135898). */
// EARLY_FRAGMENT_TEST(true)
UNIFORM_BUF(SELECT_DATA, SelectInfoData, select_info_buf)
/* Select IDs for instanced draw-calls not using #PassMain. */
STORAGE_BUF(SELECT_ID_IN, read, uint, in_select_buf[])
/* Stores the result of the whole selection drawing. Content depends on selection mode. */
STORAGE_BUF(SELECT_ID_OUT, read_write, uint, out_select_buf[])
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_select)
DEFINE("SELECT_ENABLE")
ADDITIONAL_INFO(select_id_patch)
GPU_SHADER_CREATE_END()
