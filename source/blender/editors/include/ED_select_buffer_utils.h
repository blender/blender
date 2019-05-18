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

/** \file
 * \ingroup editors
 */

#ifndef __ED_SELECT_BUFFER_UTILS_H__
#define __ED_SELECT_BUFFER_UTILS_H__

struct rcti;

uint *ED_select_buffer_bitmap_from_rect(const uint bitmap_len, const struct rcti *rect);
uint *ED_select_buffer_bitmap_from_circle(const uint bitmap_len,
                                          const int center[2],
                                          const int radius);
uint *ED_select_buffer_bitmap_from_poly(const uint bitmap_len,
                                        const int poly[][2],
                                        const int poly_len,
                                        const rcti *rect);

#endif /* __ED_SELECT_BUFFER_UTILS_H__ */
