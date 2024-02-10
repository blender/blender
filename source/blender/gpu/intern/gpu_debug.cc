/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * Debug features of OpenGL.
 */

#include "BKE_global.hh"

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
  if (stack.is_empty()) {
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

void GPU_debug_capture_begin()
{
  /* GPU Frame capture is only enabled when --debug-gpu is specified. */
  if (!(G.debug & G_DEBUG_GPU)) {
    return;
  }

  Context *ctx = Context::get();
  if (ctx && !ctx->debug_is_capturing) {
    ctx->debug_is_capturing = ctx->debug_capture_begin();
    if (!ctx->debug_is_capturing) {
      printf("Failed to start GPU frame capture!\n");
    }
    /* Call GPU_finish to ensure all desired GPU commands occur within the capture boundary. */
    GPU_finish();
  }
}

void GPU_debug_capture_end()
{
  /* GPU Frame capture is only enabled when --debug-gpu is specified. */
  if (!(G.debug & G_DEBUG_GPU)) {
    return;
  }

  Context *ctx = Context::get();
  if (ctx && ctx->debug_is_capturing) {
    /* Call GPU_finish to ensure all desired GPU commands occur within the capture boundary. */
    GPU_finish();
    ctx->debug_capture_end();
    ctx->debug_is_capturing = false;
  }
}

void *GPU_debug_capture_scope_create(const char *name)
{
  /* GPU Frame capture is only enabled when --debug-gpu is specified. */
  if (!(G.debug & G_DEBUG_GPU)) {
    return nullptr;
  }

  Context *ctx = Context::get();
  if (!ctx) {
    return nullptr;
  }
  return ctx->debug_capture_scope_create(name);
}

bool GPU_debug_capture_scope_begin(void *scope)
{
  /* Early exit if scope does not exist or not in debug mode. */
  if (!(G.debug & G_DEBUG_GPU) || !scope) {
    return false;
  }

  Context *ctx = Context::get();
  if (!ctx) {
    return false;
  }

  /* Declare beginning of capture scope region. */
  bool scope_capturing = ctx->debug_capture_scope_begin(scope);
  if (scope_capturing && !ctx->debug_is_capturing) {
    /* Call GPU_finish to ensure all desired GPU commands occur within the capture boundary. */
    GPU_finish();
    ctx->debug_is_capturing = true;
  }
  return ctx->debug_is_capturing;
}

void GPU_debug_capture_scope_end(void *scope)
{
  /* Early exit if scope does not exist or not in debug mode. */
  if (!(G.debug & G_DEBUG_GPU) || !scope) {
    return;
  }

  Context *ctx = Context::get();
  if (!ctx) {
    return;
  }

  /* If capturing, call GPU_finish to ensure all desired GPU commands occur within the capture
   * boundary. */
  if (ctx->debug_is_capturing) {
    GPU_finish();
    ctx->debug_is_capturing = false;
  }

  /* Declare end of capture scope region. */
  ctx->debug_capture_scope_end(scope);
}
