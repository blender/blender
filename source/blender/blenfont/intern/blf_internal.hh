/* SPDX-FileCopyrightText: 2009 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup blf
 */

#pragma once

#include "BLI_array.hh"
#include "BLI_bounds_types.hh"
#include "BLI_function_ref.hh"
#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

struct FontBLF;
struct GlyphBLF;
struct GlyphCacheBLF;
struct ListBase;
struct ResultBLF;
struct rcti;
struct rctf;
enum class BLFWrapMode;

/**
 * Max number of FontBLFs in memory. Take care that every font has a glyph cache per size/dpi,
 * so we don't need load the same font with different size, just load one and call #BLF_size.
 */
#define BLF_MAX_FONT 64

/**
 * If enabled, glyphs positions are on 64ths of a pixel. Disabled, they are on whole pixels.
 */
#define BLF_SUBPIXEL_POSITION

/**
 * If enabled, glyphs are rendered at multiple horizontal subpixel positions.
 */
#define BLF_SUBPIXEL_AA

/** Maximum number of opened FT_Face objects managed by cache. 0 is default of 2. */
#define BLF_CACHE_MAX_FACES 8
/** Maximum number of opened FT_Size objects managed by cache. 0 is default of 4 */
#define BLF_CACHE_MAX_SIZES 16
/** Maximum number of bytes to use for cached data nodes. 0 is default of 200,000. */
#define BLF_CACHE_BYTES 0x100000

/**
 * Offset from icon id to Unicode Supplementary Private Use Area-B,
 * added with Unicode 2.0. 65,536 code-points at U+100000..U+10FFFF.
 */
#define BLF_ICON_OFFSET 0x100000L

/**
 * We assume square pixels at a fixed DPI of 72, scaling only the size. Therefore
 * font size = points = pixels, i.e. a size of 20 will result in a 20-pixel EM square.
 * Although we could use the actual monitor DPI instead, we would then have to scale
 * the size to cancel that out. Other libraries like Skia use this same fixed value.
 */
#define BLF_DPI 72

/** Font array. */
extern FontBLF *global_font[BLF_MAX_FONT];

void blf_batch_draw_begin(FontBLF *font);
void blf_batch_draw();

/**
 * Some font have additional file with metrics information,
 * in general, the extension of the file is: `.afm` or `.pfm`
 */
char *blf_dir_metrics_search(const char *filepath);

int blf_font_init();
void blf_font_exit();

/**
 * Return glyph id from char-code.
 */
uint blf_get_char_index(FontBLF *font, uint charcode);

/**
 * Create an FT_Face for this font if not already existing.
 */
bool blf_ensure_face(FontBLF *font);
void blf_ensure_size(FontBLF *font);

void blf_draw_buffer__start(FontBLF *font);
void blf_draw_buffer__end();

FontBLF *blf_font_new_from_filepath(const char *filepath);
FontBLF *blf_font_new_from_mem(const char *mem_name, const unsigned char *mem, size_t mem_size);
void blf_font_attach_from_mem(FontBLF *font, const unsigned char *mem, size_t mem_size);

/**
 * Change font's output size. Returns true if successful in changing the size.
 */
bool blf_font_size(FontBLF *font, float size);

void blf_font_draw(FontBLF *font, const char *str, size_t str_len, ResultBLF *r_info);
void blf_font_draw__wrap(FontBLF *font, const char *str, size_t str_len, ResultBLF *r_info);

/**
 * \param outline_alpha: Alpha value between 0 and 1.
 */
void blf_draw_svg_icon(FontBLF *font,
                       uint icon_id,
                       float x,
                       float y,
                       float size,
                       const float color[4] = nullptr,
                       float outline_alpha = 1.0f,
                       bool multicolor = false,
                       blender::FunctionRef<void(std::string &)> edit_source_cb = nullptr);

blender::Array<uchar> blf_svg_icon_bitmap(
    FontBLF *font,
    uint icon_id,
    float size,
    int *r_width,
    int *r_height,
    bool multicolor = false,
    blender::FunctionRef<void(std::string &)> edit_source_cb = nullptr);

blender::Vector<blender::StringRef> blf_font_string_wrap(FontBLF *font,
                                                         blender::StringRef str,
                                                         int max_pixel_width,
                                                         BLFWrapMode mode);

/**
 * Use fixed column width, but an UTF8 character may occupy multiple columns.
 */
int blf_font_draw_mono(
    FontBLF *font, const char *str, size_t str_len, int cwidth, int tab_columns);
void blf_font_draw_buffer(FontBLF *font, const char *str, size_t str_len, ResultBLF *r_info);
void blf_font_draw_buffer__wrap(FontBLF *font, const char *str, size_t str_len, ResultBLF *r_info);
size_t blf_font_width_to_strlen(
    FontBLF *font, const char *str, size_t str_len, int width, int *r_width);
size_t blf_font_width_to_rstrlen(
    FontBLF *font, const char *str, size_t str_len, int width, int *r_width);
void blf_font_boundbox(
    FontBLF *font, const char *str, size_t str_len, rcti *r_box, ResultBLF *r_info);
void blf_font_boundbox__wrap(
    FontBLF *font, const char *str, size_t str_len, rcti *r_box, ResultBLF *r_info);
void blf_font_width_and_height(FontBLF *font,
                               const char *str,
                               size_t str_len,
                               float *r_width,
                               float *r_height,
                               ResultBLF *r_info);
float blf_font_width(FontBLF *font, const char *str, size_t str_len, ResultBLF *r_info);
float blf_font_height(FontBLF *font, const char *str, size_t str_len, ResultBLF *r_info);
float blf_font_fixed_width(FontBLF *font);
int blf_font_glyph_advance(FontBLF *font, const char *str);
int blf_font_height_max(FontBLF *font);
int blf_font_width_max(FontBLF *font);
int blf_font_descender(FontBLF *font);
int blf_font_ascender(FontBLF *font);
bool blf_font_bounds_max(FontBLF *font, rctf *r_bounds);

char *blf_display_name(FontBLF *font);

void blf_font_boundbox_foreach_glyph(
    FontBLF *font,
    const char *str,
    size_t str_len,
    bool (*user_fn)(const char *str, size_t str_step_ofs, const rcti *bounds, void *user_data),
    void *user_data);

size_t blf_str_offset_from_cursor_position(FontBLF *font,
                                           const char *str,
                                           size_t str_len,
                                           int location_x);

void blf_str_offset_to_glyph_bounds(FontBLF *font,
                                    const char *str,
                                    size_t str_offset,
                                    rcti *r_glyph_bounds);

blender::Vector<blender::Bounds<int>> blf_str_selection_boxes(
    FontBLF *font, const char *str, size_t str_len, size_t sel_start, size_t sel_length);

int blf_str_offset_to_cursor(
    FontBLF *font, const char *str, size_t str_len, size_t str_offset, int cursor_width);

void blf_font_free(FontBLF *font);

GlyphCacheBLF *blf_glyph_cache_acquire(FontBLF *font);
void blf_glyph_cache_release(FontBLF *font);
void blf_glyph_cache_clear(FontBLF *font);

/**
 * Create (or load from cache) a fully-rendered bitmap glyph.
 */
GlyphBLF *blf_glyph_ensure(FontBLF *font, GlyphCacheBLF *gc, uint charcode, uint8_t subpixel = 0);

#ifdef BLF_SUBPIXEL_AA
GlyphBLF *blf_glyph_ensure_subpixel(FontBLF *font, GlyphCacheBLF *gc, GlyphBLF *g, int32_t pen_x);
#endif

GlyphBLF *blf_glyph_ensure_icon(
    GlyphCacheBLF *gc,
    uint icon_id,
    bool color = false,
    blender::FunctionRef<void(std::string &)> edit_source_cb = nullptr);

/**
 * Convert a character's outlines into curves.
 * \return success if the character was found and converted.
 */
bool blf_character_to_curves(FontBLF *font,
                             unsigned int unicode,
                             ListBase *nurbsbase,
                             const float scale,
                             bool use_fallback,
                             float *r_advance);

void blf_glyph_draw(FontBLF *font, GlyphCacheBLF *gc, GlyphBLF *g, int x, int y);

#ifdef WIN32
/* `blf_font_win32_compat.cc` */

#  ifdef FT_FREETYPE_H
extern FT_Error FT_New_Face__win32_compat(FT_Library library,
                                          const char *pathname,
                                          FT_Long face_index,
                                          FT_Face *aface);
#  endif
#endif
