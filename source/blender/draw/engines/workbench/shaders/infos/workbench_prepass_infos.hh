/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once

#  include "BLI_utildefines_variadic.h"

#  include "gpu_shader_compat.hh"

#  include "draw_object_infos_infos.hh"
#  include "draw_view_infos.hh"

#  include "workbench_shader_shared.hh"
#endif

#ifdef GLSL_CPP_STUBS
#  define WORKBENCH_COLOR_MATERIAL
#  define WORKBENCH_COLOR_TEXTURE
#  define WORKBENCH_TEXTURE_IMAGE_ARRAY
#  define WORKBENCH_COLOR_MATERIAL
#  define WORKBENCH_COLOR_VERTEX
#  define WORKBENCH_LIGHTING_MATCAP 1

#  define CURVES_SHADER
#  define DRW_HAIR_INFO

#  define POINTCLOUD_SHADER
#  define DRW_POINTCLOUD_INFO
#endif

#include "gpu_shader_create_info.hh"
#include "workbench_defines.hh"
