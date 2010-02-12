/* $Id$ 
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
 */

#ifndef BLI_STORAGE_H
#define BLI_STORAGE_H

/* NOTE: these have to be defined before including unistd.h! */
#ifndef __APPLE__
#ifndef WIN32
#ifndef _LARGEFILE_SOURCE
#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64
#endif
#endif
#endif

struct direntry;

void   BLI_adddirstrings(void);
void   BLI_builddir(char *dirname, char *relname);
int    BLI_compare(struct direntry *entry1, struct direntry *entry2);

int    BLI_filesize(int file);
int    BLI_filepathsize(const char *path);
double BLI_diskfree(char *dir);
char *BLI_getwdN(char *dir);
void BLI_hide_dot_files(int set);
unsigned int BLI_getdir(char *dirname, struct direntry **filelist);
/**
 * @attention Do not confuse with BLI_exists
 */
int    BLI_exist(char *name);
/**
	 * Read a file as ASCII lines. An empty list is
	 * returned if the file cannot be opened or read.
	 * 
	 * @attention The returned list should be free'd with
	 * BLI_free_file_lines.
	 * 
	 * @param name The name of the file to read.
	 * @retval A list of strings representing the file lines.
	 */

int BLI_is_dir(char *file);

struct LinkNode *BLI_read_file_as_lines(char *name);

	/**
	 * Free the list returned by BLI_read_file_as_lines.
	 */
void BLI_free_file_lines(struct LinkNode *lines);

#endif /* BLI_STORAGE_H */

