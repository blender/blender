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

int BLF_init(int points, int dpi);
void BLF_exit(void);

int BLF_load(char *name);
int BLF_load_mem(char *name, unsigned char *mem, int mem_size);

int BLF_load_unique(char *name);
int BLF_load_mem_unique(char *name, unsigned char *mem, int mem_size);

/* Attach a file with metrics information from memory. */
void BLF_metrics_attach(int fontid, unsigned char *mem, int mem_size);

void BLF_aspect(int fontid, float aspect);
void BLF_position(int fontid, float x, float y, float z);
void BLF_size(int fontid, int size, int dpi);

/* Draw the string using the default font, size and dpi. */
void BLF_draw_default(float x, float y, float z, char *str);

/* Draw the string using the current font. */
void BLF_draw(int fontid, char *str);

/*
 * This function return the bounding box of the string
 * and are not multiplied by the aspect.
 */
void BLF_boundbox(int fontid, char *str, struct rctf *box);

/*
 * The next both function return the width and height
 * of the string, using the current font and both value 
 * are multiplied by the aspect of the font.
 */
float BLF_width(int fontid, char *str);
float BLF_height(int fontid, char *str);

/*
 * The following function return the width and height of the string, but
 * just in one call, so avoid extra freetype2 stuff.
 */
void BLF_width_and_height(int fontid, char *str, float *width, float *height);

/*
 * For fixed width fonts only, returns the width of a
 * character.
 */
float BLF_fixed_width(int fontid);

/*
 * and this two function return the width and height
 * of the string, using the default font and both value
 * are multiplied by the aspect of the font.
 */
float BLF_width_default(char *str);
float BLF_height_default(char *str);

/*
 * Set rotation for default font.
 */
void BLF_rotation_default(float angle);

/*
 * Enable/disable options to the default font.
 */
void BLF_enable_default(int option);
void BLF_disable_default(int option);

/*
 * By default, rotation and clipping are disable and
 * have to be enable/disable using BLF_enable/disable.
 */
void BLF_rotation(int fontid, float angle);
void BLF_clipping(int fontid, float xmin, float ymin, float xmax, float ymax);
void BLF_clipping_default(float xmin, float ymin, float xmax, float ymax);
void BLF_blur(int fontid, int size);

void BLF_enable(int fontid, int option);
void BLF_disable(int fontid, int option);

/*
 * Shadow options, level is the blur level, can be 3, 5 or 0 and
 * the other argument are the rgba color.
 * Take care that shadow need to be enable using BLF_enable!!.
 */
void BLF_shadow(int fontid, int level, float r, float g, float b, float a);

/*
 * Set the offset for shadow text, this is the current cursor
 * position plus this offset, don't need call BLF_position before
 * this function, the current position is calculate only on
 * BLF_draw, so it's safe call this whenever you like.
 */
void BLF_shadow_offset(int fontid, int x, int y);

/*
 * Set the buffer, size and number of channels to draw, one thing to take care is call
 * this function with NULL pointer when we finish, for example:
 *	BLF_buffer(my_fbuf, my_cbuf, 100, 100, 4);
 *
 *	... set color, position and draw ...
 *
 *	BLF_buffer(NULL, NULL, 0, 0, 0);
 */
void BLF_buffer(int fontid, float *fbuf, unsigned char *cbuf, unsigned int w, unsigned int h, int nch);

/*
 * Set the color to be used for text.
 */
void BLF_buffer_col(int fontid, float r, float g, float b, float a);

/*
 * Draw the string into the buffer, this function draw in both buffer, float and unsigned char _BUT_
 * it's not necessary set both buffer, NULL is valid here.
 */
void BLF_draw_buffer(int fontid, char *str);

/*
 * Search the path directory to the locale files, this try all
 * the case for Linux, Win and Mac.
 */
void BLF_lang_init(void);

/* Set the current locale. */
void BLF_lang_set(const char *);

/* Set the current encoding name. */
void BLF_lang_encoding_name(const char *str);

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
#define BLF_SHADOW (1<<2)
#define BLF_KERNING_DEFAULT (1<<3)

#endif /* BLF_API_H */
