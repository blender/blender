/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BLI_POLYFILL2D_H__
#define __BLI_POLYFILL2D_H__

struct MemArena;

void BLI_polyfill_calc_ex(
        const float (*coords)[2],
        const unsigned int count,
        unsigned int (*r_tris)[3],

        /* avoid allocating each time */
        unsigned int *r_indices, signed char *r_coords_sign);

void BLI_polyfill_calc_arena(
        const float (*coords)[2],
        const unsigned int coords_tot,
        unsigned int (*r_tris)[3],

        struct MemArena *arena);

void BLI_polyfill_calc(
        const float (*coords)[2],
        const unsigned int coords_tot,
        unsigned int (*r_tris)[3]);

#endif  /* __BLI_POLYFILL2D_H__ */
