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

void blf_texture_draw(float uv[2][2], float dx, float y1, float dx1, float y2);
void blf_texture5_draw(float uv[2][2], float x1, float y1, float x2, float y2);
void blf_texture3_draw(float uv[2][2], float x1, float y1, float x2, float y2);

char *blf_dir_search(const char *file);
int blf_dir_split(const char *str, char *file, int *size);

int blf_font_init(void);
void blf_font_exit(void);

FontBLF *blf_internal_new(char *name);

#ifdef WITH_FREETYPE2

FontBLF *blf_font_new(char *name, char *filename);
FontBLF *blf_font_new_from_mem(char *name, unsigned char *mem, int mem_size);

GlyphCacheBLF *blf_glyph_cache_find(FontBLF *font, int size, int dpi);
GlyphCacheBLF *blf_glyph_cache_new(FontBLF *font);
void blf_glyph_cache_free(GlyphCacheBLF *gc);

GlyphBLF *blf_glyph_search(GlyphCacheBLF *gc, unsigned int c);
GlyphBLF *blf_glyph_add(FontBLF *font, FT_UInt index, unsigned int c);

void blf_glyph_free(GlyphBLF *g);
int blf_glyph_render(FontBLF *font, GlyphBLF *g, float x, float y);

#endif /* WITH_FREETYPE2 */
#endif /* BLF_INTERNAL_H */
