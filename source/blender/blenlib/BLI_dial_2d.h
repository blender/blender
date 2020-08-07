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
 */

#pragma once

/** \file
 * \ingroup bli
 *
 * \note dials act similar to old rotation based phones and output an angle.
 *
 * They just are initialized with the center of the dial and a threshold value as input.
 *
 * When the distance of the current position of the dial from the center
 * exceeds the threshold, this position is used to calculate the initial direction.
 * After that, the angle from the initial direction is calculated based on
 * current and previous directions of the digit, and returned to the user.
 *
 * Usage examples:
 *
 * \code{.c}
 * float start_position[2] = {0.0f, 0.0f};
 * float current_position[2];
 * float threshold = 0.5f;
 * float angle;
 * Dial *dial;
 *
 * dial = BLI_dial_init(start_position, threshold);
 *
 * angle = BLI_dial_angle(dial, current_position);
 *
 * MEM_freeN(dial);
 * \endcode
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Dial Dial;

Dial *BLI_dial_init(const float start_position[2], float threshold);

float BLI_dial_angle(Dial *dial, const float current_position[2]);

#ifdef __cplusplus
}
#endif
