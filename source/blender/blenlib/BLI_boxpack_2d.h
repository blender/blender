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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

#pragma once

/** \file
 * \ingroup bli
 */

#ifdef __cplusplus
extern "C" {
#endif

struct ListBase;

/* Box Packer */

typedef struct BoxPack {
  float x;
  float y;
  float w;
  float h;

  /* Verts this box uses
   * (BL,TR,TL,BR) / 0,1,2,3 */
  struct BoxVert *v[4];

  int index;
} BoxPack;

void BLI_box_pack_2d(BoxPack *boxarray, const unsigned int len, float *r_tot_x, float *r_tot_y);

typedef struct FixedSizeBoxPack {
  struct FixedSizeBoxPack *next, *prev;
  int x, y;
  int w, h;
} FixedSizeBoxPack;

void BLI_box_pack_2d_fixedarea(struct ListBase *boxes,
                               int width,
                               int height,
                               struct ListBase *packed);

#ifdef __cplusplus
}
#endif
