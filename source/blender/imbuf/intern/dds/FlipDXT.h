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

#include "BLI_sys_types.h"

/**
 * Flips a DXTC image, by flipping and swapping DXTC blocks as appropriate.
 *
 * Use to flip vertically to fit OpenGL convention.
 */
int FlipDXTCImage(unsigned int width,
                  unsigned int height,
                  unsigned int levels,
                  int fourcc,
                  uint8_t *data,
                  int data_size,
                  unsigned int *r_num_valid_levels);
