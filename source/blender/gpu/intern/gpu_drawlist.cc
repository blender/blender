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
 * The Original Code is Copyright (C) 2016 by Mike Erwin.
 * All rights reserved.
 */

/** \file
 * \ingroup gpu
 *
 * Implementation of Multi Draw Indirect.
 */

#include "MEM_guardedalloc.h"

#include "GPU_batch.h"
#include "GPU_drawlist.h"

#include "gpu_backend.hh"

#include "gpu_drawlist_private.hh"

using namespace blender::gpu;

GPUDrawList GPU_draw_list_create(int list_length)
{
  DrawList *list_ptr = GPUBackend::get()->drawlist_alloc(list_length);
  return reinterpret_cast<DrawList *>(list_ptr);
}

void GPU_draw_list_discard(GPUDrawList list)
{
  DrawList *list_ptr = reinterpret_cast<DrawList *>(list);
  delete list_ptr;
}

void GPU_draw_list_append(GPUDrawList list, GPUBatch *batch, int i_first, int i_count)
{
  DrawList *list_ptr = reinterpret_cast<DrawList *>(list);
  list_ptr->append(batch, i_first, i_count);
}

void GPU_draw_list_submit(GPUDrawList list)
{
  DrawList *list_ptr = reinterpret_cast<DrawList *>(list);
  list_ptr->submit();
}
