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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 * 
 */

#ifndef BLO_READBLENFILE_H
#define BLO_READBLENFILE_H

#ifdef __cplusplus
extern "C" {
#endif

struct ReportList;

	BlendFileData *
BLO_readblenfilename(
	char *fileName, 
	struct ReportList *reports);

	BlendFileData *
BLO_readblenfilehandle(
	int fileHandle, 
	struct ReportList *reports);

	BlendFileData *
BLO_readblenfilememory(
	char *fromBuffer,
	int fromBufferSize, 
	struct ReportList *reports);


	void
BLO_setcurrentversionnumber(
	char array[4]);

	void
BLO_setversionnumber(
	char array[4],
	int version);

	int
blo_is_a_runtime(
	char *file);

	BlendFileData *
blo_read_runtime(
	char *file, 
	struct ReportList *reports);

#define BLO_RESERVEDSIZE 12
extern char *headerMagic;

#ifdef __cplusplus
}
#endif

#endif /* BLO_READBLENFILE_H */

