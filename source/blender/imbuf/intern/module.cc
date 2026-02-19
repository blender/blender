/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

#include <cstddef>

#include "BLI_assert.h"
#include "BLI_mutex.hh"
#include "BLI_threads.h"

#include "GPU_context.hh"

#include "IMB_colormanagement_intern.hh"
#include "IMB_filetype.hh"
#include "IMB_imbuf.hh"

namespace blender {

static gpu::GPUSecondaryContextData g_gpu_context;
static Mutex g_gpu_context_mutex;

void IMB_init()
{
  imb_filetypes_init();
  colormanagement_init();
}

void IMB_exit()
{
  imb_filetypes_exit();
  colormanagement_exit();

  if (g_gpu_context.gpu_context) {
    gpu::GPU_destroy_secondary_context(g_gpu_context);
  }
}

void IMB_ensure_gpu_context()
{
  BLI_assert(BLI_thread_is_main());

  if (g_gpu_context.gpu_context) {
    return;
  }

  g_gpu_context = gpu::GPU_create_secondary_context();
}

void IMB_activate_gpu_context()
{
  BLI_assert(g_gpu_context.gpu_context);

  g_gpu_context_mutex.lock();
  gpu::GPU_activate_secondary_context(g_gpu_context);
}

void IMB_deactivate_gpu_context()
{
  BLI_assert(g_gpu_context.gpu_context);

  gpu::GPU_deactivate_secondary_context(g_gpu_context);
  g_gpu_context_mutex.unlock();
}

}  // namespace blender
