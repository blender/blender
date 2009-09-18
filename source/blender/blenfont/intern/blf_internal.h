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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 * 
 * Contributor(s): Blender Foundation.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef BLF_INTERNAL_H
#define BLF_INTERNAL_H

unsigned int blf_next_p2(unsigned int x);
unsigned int blf_hash(unsigned int val);
int blf_utf8_next(unsigned char *buf, int *iindex);

char *blf_dir_search(const char *file);
char *blf_dir_metrics_search(char *filename);
int blf_dir_split(const char *str, char *file, int *size);

int blf_font_init(void);
void blf_font_exit(void);

FontBLF *blf_font_new(char *name, char *filename);
FontBLF *blf_font_new_from_mem(char *name, unsigned char *mem, int mem_size);
void blf_font_attach_from_mem(FontBLF *font, const unsigned char *mem, int mem_size);

void blf_font_size(FontBLF *font, int size, int dpi);
void blf_font_draw(FontBLF *font, char *str);
void blf_font_buffer(FontBLF *font, char *str);
void blf_font_boundbox(FontBLF *font, char *str, rctf *box);
void blf_font_width_and_height(FontBLF *font, char *str, float *width, float *height);
float blf_font_width(FontBLF *font, char *str);
float blf_font_height(FontBLF *font, char *str);
float blf_font_fixed_width(FontBLF *font);
void blf_font_free(FontBLF *font);

GlyphCacheBLF *blf_glyph_cache_find(FontBLF *font, int size, int dpi);
GlyphCacheBLF *blf_glyph_cache_new(FontBLF *font);
void blf_glyph_cache_free(GlyphCacheBLF *gc);

GlyphBLF *blf_glyph_search(GlyphCacheBLF *gc, unsigned int c);
GlyphBLF *blf_glyph_add(FontBLF *font, FT_UInt index, unsigned int c);

void blf_glyph_free(GlyphBLF *g);
int blf_glyph_render(FontBLF *font, GlyphBLF *g, float x, float y);

#endif /* BLF_INTERNAL_H */
