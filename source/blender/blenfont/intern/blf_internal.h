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

/** \file blender/blenfont/intern/blf_internal.h
 *  \ingroup blf
 */


#ifndef __BLF_INTERNAL_H__
#define __BLF_INTERNAL_H__

struct FontBLF;
struct GlyphBLF;
struct GlyphCacheBLF;
struct rctf;

unsigned int blf_next_p2(unsigned int x);
unsigned int blf_hash(unsigned int val);

char *blf_dir_search(const char *file);
char *blf_dir_metrics_search(const char *filename);
/* int blf_dir_split(const char *str, char *file, int *size);  *//* UNUSED */

int blf_font_init(void);
void blf_font_exit(void);

struct FontBLF *blf_font_new(const char *name, const char *filename);
struct FontBLF *blf_font_new_from_mem(const char *name, const unsigned char *mem, int mem_size);
void blf_font_attach_from_mem(struct FontBLF *font, const unsigned char *mem, int mem_size);

void blf_font_size(struct FontBLF *font, unsigned int size, unsigned int dpi);
void blf_font_draw(struct FontBLF *font, const char *str, size_t len);
void blf_font_draw_ascii(struct FontBLF *font, const char *str, size_t len);
int blf_font_draw_mono(struct FontBLF *font, const char *str, size_t len, int cwidth);
void blf_font_buffer(struct FontBLF *font, const char *str);
size_t blf_font_width_to_strlen(struct FontBLF *font, const char *str, size_t len, float width, float *r_width);
size_t blf_font_width_to_rstrlen(struct FontBLF *font, const char *str, size_t len, float width, float *r_width);
void blf_font_boundbox(struct FontBLF *font, const char *str, size_t len, struct rctf *box);
void blf_font_width_and_height(struct FontBLF *font, const char *str, size_t len, float *width, float *height);
float blf_font_width(struct FontBLF *font, const char *str, size_t len);
float blf_font_height(struct FontBLF *font, const char *str, size_t len);
float blf_font_fixed_width(struct FontBLF *font);
void blf_font_free(struct FontBLF *font);

struct GlyphCacheBLF *blf_glyph_cache_find(struct FontBLF *font, unsigned int size, unsigned int dpi);
struct GlyphCacheBLF *blf_glyph_cache_new(struct FontBLF *font);
void blf_glyph_cache_clear(struct FontBLF *font);
void blf_glyph_cache_free(struct GlyphCacheBLF *gc);

struct GlyphBLF *blf_glyph_search(struct GlyphCacheBLF *gc, unsigned int c);
struct GlyphBLF *blf_glyph_add(struct FontBLF *font, unsigned int index, unsigned int c);

void blf_glyph_free(struct GlyphBLF *g);
void blf_glyph_render(struct FontBLF *font, struct GlyphBLF *g, float x, float y);

#endif /* __BLF_INTERNAL_H__ */
