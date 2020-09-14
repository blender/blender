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
 * The Original Code is Copyright (C) 2020 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup gpu
 *
 * Debug features of OpenGL.
 */

#include "BKE_global.h"

#include "BLI_string.h"

#include "gpu_context_private.hh"

#include "GPU_debug.h"

using namespace blender::gpu;

void GPU_debug_group_begin(const char *name)
{
  if (!(G.debug & G_DEBUG_GPU)) {
    return;
  }

  DebugStack &stack = Context::get()->debug_stack;

  if (stack.index >= DEBUG_STACK_LEN) {
    stack.index = DEBUG_STACK_LEN - 1;
    BLI_assert(!"GPUDebug: Debug group stack overflow!");
  }

  BLI_strncpy(stack.names[stack.index++], name, sizeof(stack.names[stack.index++]));

  Context::get()->debug_group_begin(name, stack.index);
}

void GPU_debug_group_end(void)
{
  if (!(G.debug & G_DEBUG_GPU)) {
    return;
  }

  DebugStack &stack = Context::get()->debug_stack;

  if (stack.index < 0) {
    stack.index = 0;
    BLI_assert(!"GPUDebug: Debug group stack underflow!");
  }

  stack.index--;

  Context::get()->debug_group_end();
}

/* Return a formated string showing the current group hierarchy in this format:
 * "Group1 > Group 2 > Group3 > ... > GroupN : " */
void GPU_debug_get_groups_names(int name_buf_len, char *r_name_buf)
{
  Context *ctx = Context::get();
  if (ctx == nullptr) {
    return;
  }

  DebugStack &stack = ctx->debug_stack;
  if (stack.index == 0) {
    r_name_buf[0] = '\0';
    return;
  }
  size_t sz = 0;
  for (int i = 0; i < stack.index; i++) {
    sz += BLI_snprintf_rlen(r_name_buf + sz, name_buf_len - sz, "%s > ", stack.names[0]);
  }
  r_name_buf[sz - 2] = ':';
}