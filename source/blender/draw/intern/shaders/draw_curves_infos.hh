/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_shader_compat.hh"

#  include "draw_attribute_shader_shared.hh"
#  include "draw_object_infos_infos.hh"
#endif

#ifdef GLSL_CPP_STUBS
#  define DRW_HAIR_INFO
#endif

#include "draw_curves_defines.hh"

#include "gpu_shader_create_info.hh"
