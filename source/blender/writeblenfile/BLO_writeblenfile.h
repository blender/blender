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
 * 
 */

#ifndef BLO_WRITEBLENFILE_H
#define BLO_WRITEBLENFILE_H

#ifdef __cplusplus
extern "C" {
#endif

	/**
	 * Writes a file from memory to disk. The filedescriptor of the
	 * targetfile is taken from mywfile.
	 *
	 * @param data   pointer to the datachunk to write
	 * @param dataIn size of the datachunk to write (bytes)
	 * @param streamGlueHeader the eader describing this datachunk
	 * @returns 0 if the file was written correctly, non-0 if an error
	 * occured.  */
	int
BLO_writeblenfile(
	unsigned char *data,
	unsigned int dataIn,
	struct streamGlueHeaderStruct *streamGlueHeader);

#ifdef __cplusplus
}
#endif

#endif /* BLO_WRITEBLENFILE_H */

