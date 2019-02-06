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
#ifndef __BLO_BLEND_DEFS_H__
#define __BLO_BLEND_DEFS_H__

/** \file \ingroup blenloader
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

/**
 * Codes used for #BHead.code.
 *
 * These coexist with ID codes such as #ID_OB, #ID_SCE ... etc.
 */
enum {
	/**
	 * Arbitrary allocated memory
	 * (typically owned by #ID's, will be freed when there are no users).
	 */
	DATA = BLEND_MAKE_ID('D', 'A', 'T', 'A'),
	/**
	 * Used for #Global struct.
	 */
	GLOB = BLEND_MAKE_ID('G', 'L', 'O', 'B'),
	/**
	 * Used for storing the encoded SDNA string
	 * (decoded into an #SDNA on load).
	 */
	DNA1 = BLEND_MAKE_ID('D', 'N', 'A', '1'),
	/**
	 * Used to store thumbnail previews, written between #REND and #GLOB blocks,
	 * (ignored for regular file reading).
	 */
	TEST = BLEND_MAKE_ID('T', 'E', 'S', 'T'),
	/**
	 * Used for #RenderInfo, basic Scene and frame range info,
	 * can be easily read by other applications without writing a full blend file parser.
	 */
	REND = BLEND_MAKE_ID('R', 'E', 'N', 'D'),
	/**
	 * Used for #UserDef, (user-preferences data).
	 * (written to #BLENDER_STARTUP_FILE & #BLENDER_USERPREF_FILE).
	 */
	USER = BLEND_MAKE_ID('U', 'S', 'E', 'R'),
	/**
	 * Terminate reading (no data).
	 */
	ENDB = BLEND_MAKE_ID('E', 'N', 'D', 'B'),
};

#define BLEN_THUMB_MEMSIZE_FILE(_x, _y) (sizeof(int) * (2 + (size_t)(_x) * (size_t)(_y)))

#endif  /* __BLO_BLEND_DEFS_H__ */
