/* SPDX-FileCopyrightText: 2017 Blender Authors. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * Glue to make the new Select-Next engine work with the old GPU select API.
 */
#include <cfloat>

#include "BLI_rect.h"
#include "BLI_span.hh"

#include "gpu_select_private.hh"

struct GPUSelectNextState {
  /** Result buffer set on initialization. */
  GPUSelectBuffer *buffer = nullptr;
  /** Area of the viewport to render / select from. */
  rcti rect = {0, 0, 0, 0};
  /** Number of hits. Set to -1 if it overflows buffer_len. */
  uint hits = -1;
  /** Mode of operation. */
  GPUSelectMode mode = GPUSelectMode::GPU_SELECT_INVALID;
};

static GPUSelectNextState g_state = {};

void gpu_select_next_begin(GPUSelectBuffer *buffer, const rcti *input, GPUSelectMode mode)

{
  g_state.buffer = buffer;
  g_state.rect = *input;
  g_state.mode = mode;
}

int gpu_select_next_get_pick_area_center()
{
  BLI_assert(BLI_rcti_size_x(&g_state.rect) == BLI_rcti_size_y(&g_state.rect));
  return BLI_rcti_size_x(&g_state.rect) / 2;
}

GPUSelectMode gpu_select_next_get_mode()
{
  return g_state.mode;
}

void gpu_select_next_set_result(GPUSelectResult *hit_buf, uint hit_len)
{
  const int old_size = g_state.buffer->storage.size();
  g_state.buffer->storage.resize(old_size + hit_len);
  blender::MutableSpan<GPUSelectResult> hit_results =
      g_state.buffer->storage.as_mutable_span().slice(old_size, hit_len);
  const blender::Span<GPUSelectResult> hits(hit_buf, hit_len);

  /* TODO(fclem): There might be some conversion to do to align to the other APIs output. */
  switch (g_state.mode) {
    case GPUSelectMode::GPU_SELECT_ALL:
      hit_results.copy_from(hits);
      break;
    case GPUSelectMode::GPU_SELECT_PICK_ALL:
      hit_results.copy_from(hits);
      break;
    case GPUSelectMode::GPU_SELECT_PICK_NEAREST:
      hit_results.copy_from(hits);
      break;
    case GPUSelectMode::GPU_SELECT_NEAREST_FIRST_PASS:
    case GPUSelectMode::GPU_SELECT_NEAREST_SECOND_PASS:
    case GPUSelectMode::GPU_SELECT_INVALID:
      BLI_assert_unreachable();
      break;
  }

  g_state.hits = hit_len;
}

uint gpu_select_next_end()
{
  return g_state.hits;
}
