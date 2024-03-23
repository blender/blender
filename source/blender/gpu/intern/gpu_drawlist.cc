/* SPDX-FileCopyrightText: 2016 by Mike Erwin. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * Implementation of Multi Draw Indirect.
 */

#include "GPU_drawlist.hh"

#include "gpu_backend.hh"

#include "gpu_drawlist_private.hh"

using namespace blender::gpu;

GPUDrawList *GPU_draw_list_create(int list_length)
{
  DrawList *list_ptr = GPUBackend::get()->drawlist_alloc(list_length);
  return wrap(list_ptr);
}

void GPU_draw_list_discard(GPUDrawList *list)
{
  DrawList *list_ptr = unwrap(list);
  delete list_ptr;
}

void GPU_draw_list_append(GPUDrawList *list, GPUBatch *batch, int i_first, int i_count)
{
  DrawList *list_ptr = unwrap(list);
  list_ptr->append(batch, i_first, i_count);
}

void GPU_draw_list_submit(GPUDrawList *list)
{
  DrawList *list_ptr = unwrap(list);
  list_ptr->submit();
}
