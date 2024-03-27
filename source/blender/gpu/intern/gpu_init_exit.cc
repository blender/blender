/* SPDX-FileCopyrightText: 2013 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "GPU_init_exit.hh" /* interface */
#include "BLI_sys_types.h"
#include "GPU_batch.hh"

#include "intern/gpu_codegen.hh"
#include "intern/gpu_private.hh"
#include "intern/gpu_shader_create_info_private.hh"
#include "intern/gpu_shader_dependency_private.hh"

/**
 * although the order of initialization and shutdown should not matter
 * (except for the extensions), I chose alphabetical and reverse alphabetical order
 */
static bool initialized = false;

void GPU_init()
{
  /* can't avoid calling this multiple times, see wm_window_ghostwindow_add */
  if (initialized) {
    return;
  }

  initialized = true;

  gpu_shader_dependency_init();
  gpu_shader_create_info_init();

  gpu_codegen_init();

  gpu_batch_init();
}

void GPU_exit()
{
  gpu_batch_exit();

  gpu_codegen_exit();

  gpu_shader_dependency_exit();
  gpu_shader_create_info_exit();

  gpu_backend_delete_resources();

  initialized = false;
}

bool GPU_is_init()
{
  return initialized;
}
