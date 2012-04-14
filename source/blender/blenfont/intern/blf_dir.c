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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 * 
 * Contributor(s): Blender Foundation.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenfont/intern/blf_dir.c
 *  \ingroup blf
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ft2build.h>

#include FT_FREETYPE_H
#include FT_GLYPH_H

#include "MEM_guardedalloc.h"

#include "DNA_vec_types.h"

#include "BKE_utildefines.h"

#include "BLI_blenlib.h"

#include "BIF_gl.h"

#include "BLF_api.h"
#include "blf_internal_types.h"
#include "blf_internal.h"

static ListBase global_font_dir = { NULL, NULL };

static DirBLF *blf_dir_find(const char *path)
{
	DirBLF *p;
	
	p = global_font_dir.first;
	while (p) {
		if (BLI_path_cmp(p->path, path) == 0)
			return p;
		p = p->next;
	}
	return NULL;
}

void BLF_dir_add(const char *path)
{
	DirBLF *dir;
	
	dir = blf_dir_find(path);
	if (dir) /* already in the list ? just return. */
		return;
	
	dir = (DirBLF *)MEM_callocN(sizeof(DirBLF), "BLF_dir_add");
	dir->path = BLI_strdup(path);
	BLI_addhead(&global_font_dir, dir);
}

void BLF_dir_rem(const char *path)
{
	DirBLF *dir;
	
	dir = blf_dir_find(path);
	if (dir) {
		BLI_remlink(&global_font_dir, dir);
		MEM_freeN(dir->path);
		MEM_freeN(dir);
	}
}

char **BLF_dir_get(int *ndir)
{
	DirBLF *p;
	char **dirs;
	char *path;
	int i, count;
	
	count = BLI_countlist(&global_font_dir);
	if (!count)
		return NULL;
	
	dirs = (char **)MEM_callocN(sizeof(char *) * count, "BLF_dir_get");
	p = global_font_dir.first;
	i = 0;
	while (p) {
		path = BLI_strdup(p->path);
		dirs[i] = path;
		p = p->next;
	}
	*ndir = i;
	return dirs;
}

void BLF_dir_free(char **dirs, int count)
{
	char *path;
	int i;
	
	for (i = 0; i < count; i++) {
		path = dirs[i];
		MEM_freeN(path);
	}
	MEM_freeN(dirs);
}

char *blf_dir_search(const char *file)
{
	DirBLF *dir;
	char full_path[FILE_MAX];
	char *s = NULL;

	for (dir = global_font_dir.first; dir; dir = dir->next) {
		BLI_join_dirfile(full_path, sizeof(full_path), dir->path, file);
		if (BLI_exists(full_path)) {
			s = BLI_strdup(full_path);
			break;
		}
	}

	if (!s) {
		/* check the current directory, why not ? */
		if (BLI_exists(file))
			s = BLI_strdup(file);
	}

	return s;
}

#if 0 /* UNUSED */
int blf_dir_split(const char *str, char *file, int *size)
{
	int i, len;
	char *s;
	
	/* Window, Linux or Mac, this is always / */
	s = strrchr(str, '/');
	if (s) {
		len = s - str;
		for (i = 0; i < len; i++)
			file[i] = str[i];

		file[i] = '.';
		file[i+1] = 't';
		file[i+2] = 't';
		file[i+3] = 'f';
		file[i+4] = '\0';
		s++;
		*size = atoi(s);
		return 1;
	}
	return 0;
}
#endif

/* Some font have additional file with metrics information,
 * in general, the extension of the file is: .afm or .pfm
 */
char *blf_dir_metrics_search(const char *filename)
{
	char *mfile;
	char *s;

	mfile = BLI_strdup(filename);
	s = strrchr(mfile, '.');
	if (s) {
		if (BLI_strnlen(s, 4) < 4) {
			MEM_freeN(mfile);
			return NULL;
		}
		s++;
		s[0] = 'a';
		s[1] = 'f';
		s[2] = 'm';

		/* first check .afm */
		if (BLI_exists(s))
			return s;

		/* and now check .pfm */
		s[0] = 'p';

		if (BLI_exists(s))
			return s;
	}
	MEM_freeN(mfile);
	return NULL;
}
