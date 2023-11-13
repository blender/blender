/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#include "BLI_dial_2d.h"

#include "MEM_guardedalloc.h"

#include "BLI_math_vector.h"

struct Dial {
  /* center of the dial */
  float center[2];

  /* threshold of the dial. Distance of current position has to be greater
   * than the threshold to be used in any calculations */
  float threshold_squared;

  /* the direction of the first dial position exceeding the threshold. This
   * is later used as the basis against which rotation angle is calculated */
  float initial_direction[2];

  /* cache the last angle to detect rotations bigger than -/+ PI */
  float last_angle;

  /* number of full rotations */
  int rotations;

  /* has initial_direction been initialized */
  bool initialized;
};

Dial *BLI_dial_init(const float start_position[2], float threshold)
{
  Dial *dial = MEM_callocN(sizeof(Dial), "dial");

  copy_v2_v2(dial->center, start_position);
  dial->threshold_squared = threshold * threshold;

  return dial;
}

float BLI_dial_angle(Dial *dial, const float current_position[2])
{
  float current_direction[2];

  sub_v2_v2v2(current_direction, current_position, dial->center);

  /* only update when we have enough precision,
   * by having the mouse adequately away from center */
  if (len_squared_v2(current_direction) > dial->threshold_squared) {
    float angle;
    float cosval, sinval;

    normalize_v2(current_direction);

    if (!dial->initialized) {
      copy_v2_v2(dial->initial_direction, current_direction);
      dial->initialized = true;
    }

    /* calculate mouse angle between initial and final mouse position */
    cosval = dot_v2v2(current_direction, dial->initial_direction);
    sinval = cross_v2v2(current_direction, dial->initial_direction);

    /* Clamp to avoid NAN's in #acos */
    angle = atan2f(sinval, cosval);

    /* change of sign, we passed the 180 degree threshold. This means we need to add a turn.
     * to distinguish between transition from 0 to -1 and -PI to +PI,
     * use comparison with PI/2 */
    if ((angle * dial->last_angle < 0.0f) && (fabsf(dial->last_angle) > (float)M_PI_2)) {
      if (dial->last_angle < 0.0f) {
        dial->rotations--;
      }
      else {
        dial->rotations++;
      }
    }
    dial->last_angle = angle;

    return angle + 2.0f * (float)M_PI * dial->rotations;
  }

  return dial->last_angle;
}
