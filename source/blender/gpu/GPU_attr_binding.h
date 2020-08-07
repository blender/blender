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
 *
 * The Original Code is Copyright (C) 2016 by Mike Erwin.
 * All rights reserved.
 */

/** \file
 * \ingroup gpu
 *
 * GPU vertex attribute binding
 */

#pragma once

#include "GPU_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct GPUAttrBinding {
  /** Store 4 bits for each of the 16 attributes. */
  uint64_t loc_bits;
  /** 1 bit for each attribute. */
  uint16_t enabled_bits;
} GPUAttrBinding;

#ifdef __cplusplus
}
#endif
