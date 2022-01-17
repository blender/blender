/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2013 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup gpu
 */

#include "GPU_init_exit.h" /* interface */
#include "BKE_global.h"
#include "BLI_sys_types.h"
#include "GPU_batch.h"
#include "GPU_buffers.h"
#include "GPU_context.h"
#include "GPU_immediate.h"

#include "intern/gpu_codegen.h"
#include "intern/gpu_material_library.h"
#include "intern/gpu_private.h"
#include "intern/gpu_shader_create_info_private.hh"
#include "intern/gpu_shader_dependency_private.h"

/**
 * although the order of initialization and shutdown should not matter
 * (except for the extensions), I chose alphabetical and reverse alphabetical order
 */

static bool initialized = false;

void GPU_init(void)
{
  /* can't avoid calling this multiple times, see wm_window_ghostwindow_add */
  if (initialized) {
    return;
  }

  initialized = true;

  gpu_shader_dependency_init();
  gpu_shader_create_info_init();

  gpu_codegen_init();
  gpu_material_library_init();

  gpu_batch_init();

#ifndef GPU_STANDALONE
  gpu_pbvh_init();
#endif
}

void GPU_exit(void)
{
#ifndef GPU_STANDALONE
  gpu_pbvh_exit();
#endif

  gpu_batch_exit();

  gpu_material_library_exit();
  gpu_codegen_exit();

  gpu_shader_dependency_exit();
  gpu_shader_create_info_exit();

  initialized = false;
}

bool GPU_is_init(void)
{
  return initialized;
}
