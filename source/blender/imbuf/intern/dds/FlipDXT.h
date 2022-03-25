/* SPDX-License-Identifier: GPL-2.0-or-later */

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
