/*
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
 */

/** \file
 * \ingroup blf
 */

#pragma once

struct FontBLF;
struct GlyphBLF;
struct GlyphCacheBLF;
struct ResultBLF;
struct rctf;
struct rcti;

void blf_batch_draw_begin(struct FontBLF *font);
void blf_batch_draw(void);

unsigned int blf_next_p2(unsigned int x);
unsigned int blf_hash(unsigned int val);

char *blf_dir_search(const char *file);
char *blf_dir_metrics_search(const char *filename);
/* int blf_dir_split(const char *str, char *file, int *size); */ /* UNUSED */

int blf_font_init(void);
void blf_font_exit(void);

bool blf_font_id_is_valid(int fontid);

void blf_draw_buffer__start(struct FontBLF *font);
void blf_draw_buffer__end(void);

struct FontBLF *blf_font_new(const char *name, const char *filename);
struct FontBLF *blf_font_new_from_mem(const char *name, const unsigned char *mem, int mem_size);
void blf_font_attach_from_mem(struct FontBLF *font, const unsigned char *mem, int mem_size);

void blf_font_size(struct FontBLF *font, unsigned int size, unsigned int dpi);
void blf_font_draw(struct FontBLF *font,
                   const char *str,
                   size_t str_len,
                   struct ResultBLF *r_info);
void blf_font_draw__wrap(struct FontBLF *font,
                         const char *str,
                         size_t str_len,
                         struct ResultBLF *r_info);
void blf_font_draw_ascii(struct FontBLF *font,
                         const char *str,
                         size_t str_len,
                         struct ResultBLF *r_info);
int blf_font_draw_mono(struct FontBLF *font, const char *str, size_t str_len, int cwidth);
void blf_font_draw_buffer(struct FontBLF *font,
                          const char *str,
                          size_t str_len,
                          struct ResultBLF *r_info);
void blf_font_draw_buffer__wrap(struct FontBLF *font,
                                const char *str,
                                size_t str_len,
                                struct ResultBLF *r_info);
size_t blf_font_width_to_strlen(
    struct FontBLF *font, const char *str, size_t str_len, float width, float *r_width);
size_t blf_font_width_to_rstrlen(
    struct FontBLF *font, const char *str, size_t str_len, float width, float *r_width);
void blf_font_boundbox(struct FontBLF *font,
                       const char *str,
                       size_t str_len,
                       struct rctf *r_box,
                       struct ResultBLF *r_info);
void blf_font_boundbox__wrap(struct FontBLF *font,
                             const char *str,
                             size_t str_len,
                             struct rctf *r_box,
                             struct ResultBLF *r_info);
void blf_font_width_and_height(struct FontBLF *font,
                               const char *str,
                               size_t str_len,
                               float *r_width,
                               float *r_height,
                               struct ResultBLF *r_info);
float blf_font_width(struct FontBLF *font,
                     const char *str,
                     size_t str_len,
                     struct ResultBLF *r_info);
float blf_font_height(struct FontBLF *font,
                      const char *str,
                      size_t str_len,
                      struct ResultBLF *r_info);
float blf_font_fixed_width(struct FontBLF *font);
int blf_font_height_max(struct FontBLF *font);
int blf_font_width_max(struct FontBLF *font);
float blf_font_descender(struct FontBLF *font);
float blf_font_ascender(struct FontBLF *font);

char *blf_display_name(struct FontBLF *font);

void blf_font_boundbox_foreach_glyph(struct FontBLF *font,
                                     const char *str,
                                     size_t str_len,
                                     bool (*user_fn)(const char *str,
                                                     const size_t str_step_ofs,
                                                     const struct rcti *glyph_step_bounds,
                                                     const int glyph_advance_x,
                                                     const struct rctf *glyph_bounds,
                                                     const int glyph_bearing[2],
                                                     void *user_data),
                                     void *user_data,
                                     struct ResultBLF *r_info);

int blf_font_count_missing_chars(struct FontBLF *font,
                                 const char *str,
                                 const size_t str_len,
                                 int *r_tot_chars);

void blf_font_free(struct FontBLF *font);

struct GlyphCacheBLF *blf_glyph_cache_find(struct FontBLF *font,
                                           unsigned int size,
                                           unsigned int dpi);
struct GlyphCacheBLF *blf_glyph_cache_new(struct FontBLF *font);
struct GlyphCacheBLF *blf_glyph_cache_acquire(struct FontBLF *font);
void blf_glyph_cache_release(struct FontBLF *font);
void blf_glyph_cache_clear(struct FontBLF *font);
void blf_glyph_cache_free(struct GlyphCacheBLF *gc);

struct GlyphBLF *blf_glyph_search(struct GlyphCacheBLF *gc, unsigned int c);
struct GlyphBLF *blf_glyph_ensure(struct FontBLF *font, struct GlyphCacheBLF *gc, uint charcode);

void blf_glyph_free(struct GlyphBLF *g);
void blf_glyph_draw(
    struct FontBLF *font, struct GlyphCacheBLF *gc, struct GlyphBLF *g, float x, float y);

#ifdef WIN32
/* blf_font_win32_compat.c */
#  ifdef FT_FREETYPE_H
extern FT_Error FT_New_Face__win32_compat(FT_Library library,
                                          const char *pathname,
                                          FT_Long face_index,
                                          FT_Face *aface);
#  endif
#endif
