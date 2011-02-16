/**
 * blenlib/BLI_listBase.h    mar 2001 Nzc
 *
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
 * More low-level fileops from Daniel Dunbar. Two functions were also
 * defined in storage.c. These are the old fop_ prefixes. There is
 * definitely some redundancy here!
 * */

#ifndef BLI_FILEOPS_H
#define BLI_FILEOPS_H

#ifdef __cplusplus
extern "C" {
#endif

void  BLI_recurdir_fileops(const char *dirname);
int BLI_link(const char *file, const char *to);
int BLI_is_writable(const char *filename);

/**
 * @attention Do not confuse with BLI_exist
 */
int   BLI_exists(const char *file);
int   BLI_copy_fileops(const char *file, const char *to);
int   BLI_rename(const char *from, const char *to);
int   BLI_gzip(const char *from, const char *to);
int   BLI_delete(const char *file, int dir, int recursive);
int   BLI_move(const char *file, const char *to);
int   BLI_touch(const char *file);

/* only for the sane unix world: direct calls to system functions :( */
#ifndef WIN32
void BLI_setCmdCallBack(int (*f)(char*));
#endif

#ifdef __cplusplus
}
#endif

#endif

