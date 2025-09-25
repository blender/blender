/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "infos/eevee_common_infos.hh"

/* -------------------------------------------------------------------- */
/** \name Tile indirection packing
 * \{ */

#define MotionPayload uint

/* Store velocity magnitude in the MSB to be able to use it with atomicMax operations. */
MotionPayload motion_blur_tile_indirection_pack_payload(float2 motion, uint2 payload)
{
  /* NOTE: Clamp to 16383 pixel velocity. After that, it is tile position that determine the tile
   * to dilate over. */
  uint velocity = min(uint(ceil(length(motion))), 0x3FFFu);
  /* Designed for 512x512 tiles max. */
  return (velocity << 18u) | ((payload.x & 0x1FFu) << 9u) | (payload.y & 0x1FFu);
}

/* Return thread index. */
int2 motion_blur_tile_indirection_pack_payload(uint data)
{
  return int2((data >> 9u) & 0x1FFu, data & 0x1FFu);
}

uint motion_blur_tile_indirection_index(uint motion_step, uint2 tile)
{
  uint index = tile.x;
  index += tile.y * MOTION_BLUR_MAX_TILE;
  index += motion_step * MOTION_BLUR_MAX_TILE * MOTION_BLUR_MAX_TILE;
  return index;
}

#define MOTION_PREV 0u
#define MOTION_NEXT 1u

#define motion_blur_tile_indirection_store(table_, step_, tile, payload_) \
  if (true) { \
    uint index = motion_blur_tile_indirection_index(step_, tile); \
    atomicMax(table_[index], payload_); \
  }

#define motion_blur_tile_indirection_load(table_, step_, tile_, result_) \
  if (true) { \
    uint index = motion_blur_tile_indirection_index(step_, tile_); \
    result_ = motion_blur_tile_indirection_pack_payload(table_[index]); \
  }

/** \} */
