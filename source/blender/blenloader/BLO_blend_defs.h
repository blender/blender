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
#ifndef __BLO_BLEND_DEFS_H__
#define __BLO_BLEND_DEFS_H__

/** \file BLO_blend_defs.h
 *  \ingroup blenloader
 *  \brief defines for blendfile codes
 */

/* INTEGER CODES */
#ifdef __BIG_ENDIAN__
/* Big Endian */
#  define BLEND_MAKE_ID(a, b, c, d) ( (int)(a) << 24 | (int)(b) << 16 | (c) << 8 | (d) )
#else
/* Little Endian */
#  define BLEND_MAKE_ID(a, b, c, d) ( (int)(d) << 24 | (int)(c) << 16 | (b) << 8 | (a) )
#endif

#define DATA BLEND_MAKE_ID('D', 'A', 'T', 'A')
#define GLOB BLEND_MAKE_ID('G', 'L', 'O', 'B')

#define DNA1 BLEND_MAKE_ID('D', 'N', 'A', '1')
#define TEST BLEND_MAKE_ID('T', 'E', 'S', 'T') /* used as preview between 'REND' and 'GLOB' */
#define REND BLEND_MAKE_ID('R', 'E', 'N', 'D')
#define USER BLEND_MAKE_ID('U', 'S', 'E', 'R')

#define ENDB BLEND_MAKE_ID('E', 'N', 'D', 'B')

#endif  /* __BLO_BLEND_DEFS_H__ */
