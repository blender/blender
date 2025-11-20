/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_shader_compat.hh"
#endif

#include "gpu_shader_create_info.hh"

/* Runtime create info. */
GPU_SHADER_CREATE_INFO(OCIO_Processor)
LOCAL_GROUP_SIZE(16, 16)
GPU_SHADER_CREATE_END()
