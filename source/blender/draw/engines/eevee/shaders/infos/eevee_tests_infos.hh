/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_shader_compat.hh"
#endif

#include "eevee_defines.hh"
#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(eevee_tests_data)
TYPEDEF_SOURCE("eevee_defines.hh")
DEFINE("MAT_REFLECTION")
DEFINE("MAT_REFRACTION")
DEFINE("MAT_SUBSURFACE")
DEFINE("MAT_TRANSLUCENT")
GPU_SHADER_CREATE_END()
