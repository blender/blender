/**
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 * zlib inflate and deflate stream header
 */

#ifndef BLO_IN_DE_FLATE_H
#define BLO_IN_DE_FLATE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "BLO_sys_types.h"

#define IN_DE_FLATEHEADERSTRUCTSIZE sizeof(struct BLO_in_de_flateHeaderStruct)

/* POSIX datatypes, use BYTEORDER(3) */
struct BLO_in_de_flateHeaderStruct {
	uint8_t magic;					/* poor mans header recognize check */
	uint32_t compressedLength;		/* how much compressed data is there */
	uint32_t uncompressedLength;	/* how much uncompressed data there is */
	uint32_t dictionary_id;			/* which dictionary are we using */
	uint32_t dictId;				/* Adler32 value of the dictionary */
	uint32_t crc;					/* header minus crc itself checksum */
};

#ifdef __cplusplus
}
#endif

#endif /* BLO_IN_DE_FLATE_H */

