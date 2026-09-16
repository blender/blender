/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "select_defines.hh"
#include "select_shader_shared.hh"

namespace draw {

struct Select {
  [[uniform(SELECT_DATA)]] const SelectInfoData &select_info_buf;
  [[storage(SELECT_ID_IN, read)]] const uint (&in_select_buf)[];
  [[storage(SELECT_ID_OUT, read_write)]] uint (&out_select_buf)[];

  void select_id_output(uint id, float4 frag_coord)
  {
    if (id == -1u) {
      /* Invalid index */
      return;
    }

    if (select_info_buf.mode == SELECT_ALL) {
      if (select_info_buf.radius > 0) {
        /* When the radius is set, we assume that the center is cursor is the center of a circle.
         */
        int2 coord = int2(frag_coord.xy) - select_info_buf.cursor;
        uint dist_sq = uint(coord.x * coord.x + coord.y * coord.y);
        uint rad_sq = select_info_buf.radius * select_info_buf.radius;
        if (dist_sq > rad_sq) {
          return;
        }
      }

      /* Set the bit of the select id in the bitmap. */
      atomicOr(out_select_buf[id / 32u], 1u << (id % 32u));
    }
    else if (select_info_buf.mode == SELECT_PICK_ALL) {
      /* Stores the nearest depth for this select id. */
      atomicMin(out_select_buf[id], floatBitsToUint(frag_coord.z));
    }
    else if (select_info_buf.mode == SELECT_PICK_NEAREST) {
      /* Stores the nearest depth with the distance to the cursor. */

      /* Distance function to the cursor. Currently a simple pixel ring distance. */
      int2 coord = abs(int2(frag_coord.xy) - select_info_buf.cursor);
      uint dist = uint(max(coord.x, coord.y));

      uint depth = uint(frag_coord.z * float(0x00FFFFFFu));

      /* Reject hits outside of valid range. */
      if (dist < 0xFFu) {
        /* Packed values to ensure the atomicMin is performed on the whole result. */
        atomicMin(out_select_buf[id], (depth << 8u) | dist);
      }
    }
  }
};

}  // namespace draw
