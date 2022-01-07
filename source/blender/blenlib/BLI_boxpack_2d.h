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

/**
 * Main box-packing function accessed from other functions
 * This sets boxes x,y to positive values, sorting from 0,0 outwards.
 * There is no limit to the space boxes may take, only that they will be packed
 * tightly into the lower left hand corner (0,0)
 *
 * \param boxarray: a pre-allocated array of boxes.
 *      only the 'box->x' and 'box->y' are set, 'box->w' and 'box->h' are used,
 *      'box->index' is not used at all, the only reason its there
 *          is that the box array is sorted by area and programs need to be able
 *          to have some way of writing the boxes back to the original data.
 * \param len: the number of boxes in the array.
 * \param r_tot_x, r_tot_y: set so you can normalize the data.
 */
void BLI_box_pack_2d(BoxPack *boxarray, unsigned int len, float *r_tot_x, float *r_tot_y);

typedef struct FixedSizeBoxPack {
  struct FixedSizeBoxPack *next, *prev;
  int x, y;
  int w, h;
} FixedSizeBoxPack;

/**
 * Packs boxes into a fixed area.
 *
 * Boxes and packed are linked lists containing structs that can be cast to
 * #FixedSizeBoxPack (i.e. contains a #FixedSizeBoxPack as its first element).
 * Boxes that were packed successfully are placed into *packed and removed from *boxes.
 *
 * The algorithm is a simplified version of https://github.com/TeamHypersomnia/rectpack2D.
 * Better ones could be used, but for the current use case (packing Image tiles into GPU
 * textures) this is fine.
 *
 * Note that packing efficiency depends on the order of the input boxes. Generally speaking,
 * larger boxes should come first, though how exactly size is best defined (e.g. area, perimeter)
 * depends on the particular application.
 */
void BLI_box_pack_2d_fixedarea(struct ListBase *boxes,
                               int width,
                               int height,
                               struct ListBase *packed);

#ifdef __cplusplus
}
#endif
