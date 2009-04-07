/**
 * $Id:
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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef BLF_API_H
#define BLF_API_H

struct rctf;

int BLF_init(void);
void BLF_exit(void);

int BLF_load(char *name);
int BLF_load_mem(char *name, unsigned char *mem, int mem_size);

/*
 * Set/Get the current font.
 */
void BLF_set(int fontid);
int BLF_get(void);

void BLF_aspect(float aspect);
void BLF_position(float x, float y, float z);
void BLF_size(int size, int dpi);
void BLF_draw(char *str);

/*
 * This function return the bounding box of the string
 * and are not multiplied by the aspect.
 */
void BLF_boundbox(char *str, struct rctf *box);

/*
 * The next both function return the width and height
 * of the string, using the current font and both value 
 * are multiplied by the aspect of the font.
 */
float BLF_width(char *str);
float BLF_height(char *str);

/*
 * By default, rotation and clipping are disable and
 * have to be enable/disable using BLF_enable/disable.
 */
void BLF_rotation(float angle);
void BLF_clipping(float xmin, float ymin, float xmax, float ymax);

void BLF_enable(int option);
void BLF_disable(int option);

/* Read the .Blanguages file, return 1 on success or 0 if fails. */
int BLF_lang_init(void);

/* Free the memory allocate for the .Blanguages. */
void BLF_lang_exit(void);

/* Set the current Language. */
void BLF_lang_set(int id);

/* Return a string with all the Language available. */
char *BLF_lang_pup(void);

/* Return the number of invalid lines in the .Blanguages file,
 * zero means no error found.
 */
int BLF_lang_error(void);

/* Return the code string for the specified language code. */
char *BLF_lang_find_code(short langid);

/* Add a path to the font dir paths. */
void BLF_dir_add(const char *path);

/* Remove a path from the font dir paths. */
void BLF_dir_rem(const char *path);

/* Return an array with all the font dir (this can be used for filesel) */
char **BLF_dir_get(int *ndir);

/* Free the data return by BLF_dir_get. */
void BLF_dir_free(char **dirs, int count);

/* font->flags. */
#define BLF_ROTATION (1<<0)
#define BLF_CLIPPING (1<<1)

/* font->mode. */
#define BLF_MODE_TEXTURE 0
#define BLF_MODE_BITMAP 1

#endif /* BLF_API_H */
