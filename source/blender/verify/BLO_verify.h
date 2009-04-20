/**
 * $Id$
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef BLO_VERIFY_H
#define BLO_VERIFY_H

#ifdef __cplusplus
extern "C" {
#endif

#define VERIFY_DECLARE_HANDLE(name) typedef struct name##__ { int unused; } *name

VERIFY_DECLARE_HANDLE(BLO_verifyStructHandle);

/**
 * openssl verify initializer
 * @retval pointer to verify control structure
 */
	BLO_verifyStructHandle
BLO_verify_begin(
	void *endControl);

/**
 * openssl verify dataprocessor wrapper
 * @param BLO_verify Pointer to verify control structure
 * @param data Pointer to new data
 * @param dataIn New data amount
 * @retval streamGlueRead return value
 */
	int
BLO_verify_process(
	BLO_verifyStructHandle BLO_verifyHandle,
	unsigned char *data,
	unsigned int dataIn);

/**
 * openssl verify final call and cleanup
 * @param BLO_verify Pointer to verify control structure
 * @retval streamGlueRead return value
 */
	int
BLO_verify_end(
	BLO_verifyStructHandle BLO_verifyHandle);

#ifdef __cplusplus
}
#endif

#endif /* BLO_VERIFY_H */

