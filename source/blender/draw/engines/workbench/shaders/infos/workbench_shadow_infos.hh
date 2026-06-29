/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once

#  include "BLI_utildefines_variadic.hh"

#  include "gpu_shader_compat.hh"

#  include "draw_object_infos_infos.hh"
#  include "draw_view_infos.hh"
#  include "gpu_index_load_infos.hh"

#  include "workbench_shader_shared.hh"
#endif

#ifdef GLSL_CPP_STUBS
#  define DYNAMIC_PASS_SELECTION
#endif

#include "draw_defines.hh"

#include "gpu_shader_create_info.hh"
