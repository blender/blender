/* SPDX-FileCopyrightText: 2017 Blender Foundation. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * Glue to make the new Select-Next engine work with the old GPU select API.
 */
#include <float.h>

#include "BLI_rect.h"
#include "BLI_span.hh"

#include "GPU_select.h"

#include "gpu_select_private.h"

struct GPUSelectNextState {
  /** Result buffer set on initialization. */
  GPUSelectResult *buffer;
  uint buffer_len;
  /** Area of the viewport to render / select from. */
  rcti rect;
  /** Number of hits. Set to -1 if it overflows buffer_len. */
  uint hits;
  /** Mode of operation. */
  eGPUSelectMode mode;
};

static GPUSelectNextState g_state = {};

void gpu_select_next_begin(GPUSelectResult *buffer,
                           uint buffer_len,
                           const rcti *input,
                           eGPUSelectMode mode)

{
  g_state.buffer = buffer;
  g_state.rect = *input;
  g_state.buffer_len = buffer_len;
  g_state.mode = mode;
}

int gpu_select_next_get_pick_area_center()
{
  BLI_assert(BLI_rcti_size_x(&g_state.rect) == BLI_rcti_size_y(&g_state.rect));
  return BLI_rcti_size_x(&g_state.rect) / 2;
}

eGPUSelectMode gpu_select_next_get_mode()
{
  return g_state.mode;
}

void gpu_select_next_set_result(GPUSelectResult *hit_buf, uint hit_len)

{
  if (hit_len > g_state.buffer_len) {
    g_state.hits = -1;
    return;
  }

  blender::MutableSpan<GPUSelectResult> result(g_state.buffer, g_state.buffer_len);
  blender::Span<GPUSelectResult> hits(hit_buf, hit_len);

  /* TODO(fclem): There might be some conversion to do to align to the other APIs output. */
  switch (g_state.mode) {
    case eGPUSelectMode::GPU_SELECT_ALL:
      result.take_front(hit_len).copy_from(hits);
      break;
    case eGPUSelectMode::GPU_SELECT_NEAREST_FIRST_PASS:
      result.take_front(hit_len).copy_from(hits);
      break;
    case eGPUSelectMode::GPU_SELECT_NEAREST_SECOND_PASS:
      result.take_front(hit_len).copy_from(hits);
      break;
    case eGPUSelectMode::GPU_SELECT_PICK_ALL:
      result.take_front(hit_len).copy_from(hits);
      break;
    case eGPUSelectMode::GPU_SELECT_PICK_NEAREST:
      result.take_front(hit_len).copy_from(hits);
      break;
  }

  g_state.hits = hit_len;
}

uint gpu_select_next_end()
{
  return g_state.hits;
}
