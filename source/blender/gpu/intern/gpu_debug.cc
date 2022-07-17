/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2020 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 *
 * Debug features of OpenGL.
 */

#include "BKE_global.h"

#include "BLI_string.h"

#include "gpu_context_private.hh"

#include "GPU_debug.h"

using namespace blender;
using namespace blender::gpu;

void GPU_debug_group_begin(const char *name)
{
  if (!(G.debug & G_DEBUG_GPU)) {
    return;
  }
  Context *ctx = Context::get();
  DebugStack &stack = ctx->debug_stack;
  stack.append(StringRef(name));
  ctx->debug_group_begin(name, stack.size());
}

void GPU_debug_group_end()
{
  if (!(G.debug & G_DEBUG_GPU)) {
    return;
  }
  Context *ctx = Context::get();
  ctx->debug_stack.pop_last();
  ctx->debug_group_end();
}

void GPU_debug_get_groups_names(int name_buf_len, char *r_name_buf)
{
  Context *ctx = Context::get();
  if (ctx == nullptr) {
    return;
  }
  DebugStack &stack = ctx->debug_stack;
  if (stack.size() == 0) {
    r_name_buf[0] = '\0';
    return;
  }
  size_t len = 0;
  for (StringRef &name : stack) {
    len += BLI_snprintf_rlen(r_name_buf + len, name_buf_len - len, "%s > ", name.data());
  }
  r_name_buf[len - 3] = '\0';
}

bool GPU_debug_group_match(const char *ref)
{
  /* Otherwise there will be no names. */
  BLI_assert(G.debug & G_DEBUG_GPU);
  Context *ctx = Context::get();
  if (ctx == nullptr) {
    return false;
  }
  const DebugStack &stack = ctx->debug_stack;
  for (const StringRef &name : stack) {
    if (name == ref) {
      return true;
    }
  }
  return false;
}
