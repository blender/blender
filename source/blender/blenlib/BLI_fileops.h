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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file BLI_fileops.h
 *  \ingroup bli
 *  \brief File and directory operations.
 * */

#ifndef __BLI_FILEOPS_H__
#define __BLI_FILEOPS_H__

#include <stdio.h>


#ifdef __cplusplus
extern "C" {
#endif

#include "BLI_fileops_types.h"

/* for size_t (needed on windows) */
#include <stddef.h>

struct gzFile;

/* Common */

int    BLI_exists(const char *path);
int    BLI_copy(const char *path, const char *to);
int    BLI_rename(const char *from, const char *to);
int    BLI_delete(const char *path, int dir, int recursive);
int    BLI_move(const char *path, const char *to);
int    BLI_create_symlink(const char *path, const char *to);

/* Directories */

struct direntry;

int    BLI_is_dir(const char *path);
int    BLI_is_file(const char *path);
void   BLI_dir_create_recursive(const char *dir);
double BLI_dir_free_space(const char *dir);
char  *BLI_current_working_dir(char *dir, const int maxlen);

unsigned int BLI_dir_contents(const char *dir, struct direntry **filelist);

/* Files */

FILE  *BLI_fopen(const char *filename, const char *mode);
void  *BLI_gzopen(const char *filename, const char *mode);
int    BLI_open(const char *filename, int oflag, int pmode);

int    BLI_file_is_writable(const char *file);
int    BLI_file_touch(const char *file);

int    BLI_file_gzip(const char *from, const char *to);
char  *BLI_file_ungzip_to_mem(const char *from_file, int *size_r);

size_t BLI_file_descriptor_size(int file);
size_t BLI_file_size(const char *file);

	/* compare if one was last modified before the other */
int    BLI_file_older(const char *file1, const char *file2);

	/* read ascii file as lines, empty list if reading fails */
struct LinkNode *BLI_file_read_as_lines(const char *file);
void   BLI_file_free_lines(struct LinkNode *lines);

#ifdef __cplusplus
}
#endif

#endif

