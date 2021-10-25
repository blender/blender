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
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenfont/BLF_api.h
 *  \ingroup blf
 */


#ifndef __BLF_API_H__
#define __BLF_API_H__

#include "BLI_compiler_attrs.h"

struct rctf;
struct ColorManagedDisplay;
struct ResultBLF;

int BLF_init(void);
void BLF_exit(void);
void BLF_default_dpi(int dpi);
void BLF_default_set(int fontid);

void BLF_cache_clear(void);

int BLF_load(const char *name) ATTR_NONNULL();
int BLF_load_mem(const char *name, const unsigned char *mem, int mem_size) ATTR_NONNULL();

int BLF_load_unique(const char *name) ATTR_NONNULL();
int BLF_load_mem_unique(const char *name, const unsigned char *mem, int mem_size) ATTR_NONNULL();

void BLF_unload(const char *name) ATTR_NONNULL();
void BLF_unload_id(int fontid);

/* Attach a file with metrics information from memory. */
void BLF_metrics_attach(int fontid, unsigned char *mem, int mem_size);

void BLF_aspect(int fontid, float x, float y, float z);
void BLF_position(int fontid, float x, float y, float z);
void BLF_size(int fontid, int size, int dpi);

/* Set a 4x4 matrix to be multiplied before draw the text.
 * Remember that you need call BLF_enable(BLF_MATRIX)
 * to enable this.
 *
 * The order of the matrix is like GL:
 *
 *  | m[0]  m[4]  m[8]  m[12] |
 *  | m[1]  m[5]  m[9]  m[13] |
 *  | m[2]  m[6]  m[10] m[14] |
 *  | m[3]  m[7]  m[11] m[15] |
 *
 */
void BLF_matrix(int fontid, const float m[16]);

/* Draw the string using the default font, size and dpi. */
void BLF_draw_default(float x, float y, float z, const char *str, size_t len) ATTR_NONNULL();
void BLF_draw_default_ascii(float x, float y, float z, const char *str, size_t len) ATTR_NONNULL();

/* Draw the string using the current font. */
void BLF_draw_ex(int fontid, const char *str, size_t len, struct ResultBLF *r_info) ATTR_NONNULL(2);
void BLF_draw(int fontid, const char *str, size_t len) ATTR_NONNULL(2);
void BLF_draw_ascii_ex(int fontid, const char *str, size_t len, struct ResultBLF *r_info) ATTR_NONNULL(2);
void BLF_draw_ascii(int fontid, const char *str, size_t len) ATTR_NONNULL(2);
int BLF_draw_mono(int fontid, const char *str, size_t len, int cwidth) ATTR_NONNULL(2);

/* Get the string byte offset that fits within a given width */
size_t BLF_width_to_strlen(int fontid, const char *str, size_t len, float width, float *r_width) ATTR_NONNULL(2);
/* Same as BLF_width_to_strlen but search from the string end */
size_t BLF_width_to_rstrlen(int fontid, const char *str, size_t len, float width, float *r_width) ATTR_NONNULL(2);

/* This function return the bounding box of the string
 * and are not multiplied by the aspect.
 */
void BLF_boundbox_ex(int fontid, const char *str, size_t len, struct rctf *box, struct ResultBLF *r_info) ATTR_NONNULL(2);
void BLF_boundbox(int fontid, const char *str, size_t len, struct rctf *box) ATTR_NONNULL();

/* The next both function return the width and height
 * of the string, using the current font and both value 
 * are multiplied by the aspect of the font.
 */
float BLF_width_ex(int fontid, const char *str, size_t len, struct ResultBLF *r_info) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(2);
float BLF_width(int fontid, const char *str, size_t len) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
float BLF_height_ex(int fontid, const char *str, size_t len, struct ResultBLF *r_info) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(2);
float BLF_height(int fontid, const char *str, size_t len) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

/* Return dimensions of the font without any sample text. */
int BLF_height_max(int fontid) ATTR_WARN_UNUSED_RESULT;
float BLF_width_max(int fontid) ATTR_WARN_UNUSED_RESULT;
float BLF_descender(int fontid) ATTR_WARN_UNUSED_RESULT;
float BLF_ascender(int fontid) ATTR_WARN_UNUSED_RESULT;

/* The following function return the width and height of the string, but
 * just in one call, so avoid extra freetype2 stuff.
 */
void BLF_width_and_height(int fontid, const char *str, size_t len, float *r_width, float *r_height) ATTR_NONNULL();

/* For fixed width fonts only, returns the width of a
 * character.
 */
float BLF_fixed_width(int fontid) ATTR_WARN_UNUSED_RESULT;

/* and this two function return the width and height
 * of the string, using the default font and both value
 * are multiplied by the aspect of the font.
 */
void  BLF_width_and_height_default(const char *str, size_t len, float *r_width, float *r_height) ATTR_NONNULL();
float BLF_width_default(const char *str, size_t len) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
float BLF_height_default(const char *str, size_t len) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

/* Set rotation for default font. */
void BLF_rotation_default(float angle);

/* Enable/disable options to the default font. */
void BLF_enable_default(int option);
void BLF_disable_default(int option);

/* By default, rotation and clipping are disable and
 * have to be enable/disable using BLF_enable/disable.
 */
void BLF_rotation(int fontid, float angle);
void BLF_clipping(int fontid, float xmin, float ymin, float xmax, float ymax);
void BLF_clipping_default(float xmin, float ymin, float xmax, float ymax);
void BLF_wordwrap(int fontid, int wrap_width);
void BLF_blur(int fontid, int size);

void BLF_enable(int fontid, int option);
void BLF_disable(int fontid, int option);

/* Shadow options, level is the blur level, can be 3, 5 or 0 and
 * the other argument are the rgba color.
 * Take care that shadow need to be enable using BLF_enable!!!
 */
void BLF_shadow(int fontid, int level, const float rgba[4]) ATTR_NONNULL(3);

/* Set the offset for shadow text, this is the current cursor
 * position plus this offset, don't need call BLF_position before
 * this function, the current position is calculate only on
 * BLF_draw, so it's safe call this whenever you like.
 */
void BLF_shadow_offset(int fontid, int x, int y);

/* Set the buffer, size and number of channels to draw, one thing to take care is call
 * this function with NULL pointer when we finish, for example:
 *
 *     BLF_buffer(my_fbuf, my_cbuf, 100, 100, 4, true, NULL);
 *
 *     ... set color, position and draw ...
 *
 *     BLF_buffer(NULL, NULL, NULL, 0, 0, false, NULL);
 */
void BLF_buffer(int fontid, float *fbuf, unsigned char *cbuf, int w, int h, int nch, struct ColorManagedDisplay *display);

/* Set the color to be used for text. */
void BLF_buffer_col(int fontid, const float rgba[4]) ATTR_NONNULL(2);

/* Draw the string into the buffer, this function draw in both buffer, float and unsigned char _BUT_
 * it's not necessary set both buffer, NULL is valid here.
 */
void BLF_draw_buffer_ex(int fontid, const char *str, size_t len, struct ResultBLF *r_info) ATTR_NONNULL(2);
void BLF_draw_buffer(int fontid, const char *str, size_t len) ATTR_NONNULL(2);

/* Add a path to the font dir paths. */
void BLF_dir_add(const char *path) ATTR_NONNULL();

/* Remove a path from the font dir paths. */
void BLF_dir_rem(const char *path) ATTR_NONNULL();

/* Return an array with all the font dir (this can be used for filesel) */
char **BLF_dir_get(int *ndir) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

/* Free the data return by BLF_dir_get. */
void BLF_dir_free(char **dirs, int count) ATTR_NONNULL();

/* blf_thumbs.c */
void BLF_thumb_preview(
        const char *filename, const char **draw_str, const unsigned char draw_str_lines,
        const float font_color[4], const int font_size,
        unsigned char *buf, int w, int h, int channels) ATTR_NONNULL();

/* blf_font_i18.c */
unsigned char *BLF_get_unifont(int *unifont_size);
void           BLF_free_unifont(void);
unsigned char *BLF_get_unifont_mono(int *unifont_size);
void           BLF_free_unifont_mono(void);

#ifdef DEBUG
void BLF_state_print(int fontid);
#endif

/* font->flags. */
#define BLF_ROTATION         (1 << 0)
#define BLF_CLIPPING         (1 << 1)
#define BLF_SHADOW           (1 << 2)
#define BLF_KERNING_DEFAULT  (1 << 3)
#define BLF_MATRIX           (1 << 4)
#define BLF_ASPECT           (1 << 5)
#define BLF_HINTING          (1 << 6)
#define BLF_WORD_WRAP        (1 << 7)

#define BLF_DRAW_STR_DUMMY_MAX 1024

/* XXX, bad design */
extern int blf_mono_font;
extern int blf_mono_font_render; /* don't mess drawing with render threads. */

/**
 * Result of drawing/evaluating the string
 */
struct ResultBLF {
	/**
	 * Number of lines drawn when #BLF_WORD_WRAP is enabled (both wrapped and `\n` newline).
	 */
	int lines;
	/**
	 * The 'cursor' position on completion (ignoring character boundbox).
	 */
	int width;
};

#endif /* __BLF_API_H__ */
