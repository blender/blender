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
 * zlib inflate decompression wrapper library interface
 */

#ifndef BLO_INFLATE_H
#define BLO_INFLATE_H

#ifdef __cplusplus
extern "C" {
#endif

#define INFLATE_DECLARE_HANDLE(name) typedef struct name##__ { int unused; } *name

INFLATE_DECLARE_HANDLE(BLO_inflateStructHandle);

/**
 * zlib inflate decompression initializer
 * @retval pointer to inflate control structure
 */

	BLO_inflateStructHandle
BLO_inflate_begin(
	void *endControl);

/**
 * zlib inflate dataprocessor wrapper
 * @param BLO_inflate Pointer to inflate control structure
 * @param data Pointer to new data
 * @param dataIn New data amount
 * @retval streamGlueRead return value
 */
	int
BLO_inflate_process(
	BLO_inflateStructHandle BLO_inflate_handle,
	unsigned char *data,
	unsigned int dataIn);

/**
 * zlib inflate final call and cleanup
 * @param BLO_inflate Pointer to inflate control structure
 * @retval streamGlueRead return value
 */
	int
BLO_inflate_end(
	BLO_inflateStructHandle BLO_inflate_handle);

#ifdef __cplusplus
}
#endif

#endif /* BLO_INFLATE_H */
