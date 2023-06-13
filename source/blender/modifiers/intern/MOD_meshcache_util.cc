/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include "BLI_utildefines.h"

#include "BLI_math.h"

#include "DNA_modifier_types.h"

#include "MOD_meshcache_util.hh"

void MOD_meshcache_calc_range(const float frame,
                              const char interp,
                              const int frame_tot,
                              int r_index_range[2],
                              float *r_factor)
{
  if (interp == MOD_MESHCACHE_INTERP_NONE) {
    r_index_range[0] = r_index_range[1] = max_ii(0, min_ii(frame_tot - 1, round_fl_to_int(frame)));
    *r_factor = 1.0f; /* dummy */
  }
  else {
    const float tframe = floorf(frame);
    const float range = frame - tframe;
    r_index_range[0] = int(tframe);
    if (range <= FRAME_SNAP_EPS) {
      /* we're close enough not to need blending */
      r_index_range[1] = r_index_range[0];
      *r_factor = 1.0f; /* dummy */
    }
    else {
      /* blend between 2 frames */
      r_index_range[1] = r_index_range[0] + 1;
      *r_factor = range;
    }

    /* clamp */
    if ((r_index_range[0] >= frame_tot) || (r_index_range[1] >= frame_tot)) {
      r_index_range[0] = r_index_range[1] = frame_tot - 1;
      *r_factor = 1.0f; /* dummy */
    }
    else if ((r_index_range[0] < 0) || (r_index_range[1] < 0)) {
      r_index_range[0] = r_index_range[1] = 0;
      *r_factor = 1.0f; /* dummy */
    }
  }
}
