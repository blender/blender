/* SPDX-FileCopyrightText: 2009 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup blf
 */

#pragma once

#include "BLI_compiler_attrs.h"
#include "BLI_sys_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Name of sub-directory inside #BLENDER_DATAFILES that contains font files. */
#define BLF_DATAFILES_FONTS_DIR "fonts"

/* File name of the default variable-width font. */
#define BLF_DEFAULT_PROPORTIONAL_FONT "Inter.woff2"

/* File name of the default fixed-pitch font. */
#define BLF_DEFAULT_MONOSPACED_FONT "DejaVuSansMono.woff2"

/* enable this only if needed (unused circa 2016) */
#define BLF_BLUR_ENABLE 0

struct ColorManagedDisplay;
struct ResultBLF;
struct rcti;

int BLF_init(void);
void BLF_exit(void);

void BLF_cache_clear(void);

/**
 * Optional cache flushing function, called before #blf_batch_draw.
 */
void BLF_cache_flush_set_fn(void (*cache_flush_fn)(void));

/**
 * Loads a font, or returns an already loaded font and increments its reference count.
 */
int BLF_load(const char *filepath) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
int BLF_load_mem(const char *name, const unsigned char *mem, int mem_size) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1, 2);

bool BLF_is_loaded(const char *filepath) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
bool BLF_is_loaded_mem(const char *name) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);

int BLF_load_unique(const char *filepath) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
int BLF_load_mem_unique(const char *name, const unsigned char *mem, int mem_size)
    ATTR_NONNULL(1, 2);

void BLF_unload(const char *filepath) ATTR_NONNULL(1);
#if 0 /* Not needed at the moment. */
void BLF_unload_mem(const char *name) ATTR_NONNULL(1);
#endif

void BLF_unload_id(int fontid);
void BLF_unload_all(void);

char *BLF_display_name_from_file(const char *filepath) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);

/**
 * Check if font supports a particular glyph.
 */
bool BLF_has_glyph(int fontid, unsigned int unicode) ATTR_WARN_UNUSED_RESULT;

/**
 * Attach a file with metrics information from memory.
 */
void BLF_metrics_attach(int fontid, unsigned char *mem, int mem_size) ATTR_NONNULL(2);

void BLF_aspect(int fontid, float x, float y, float z);
void BLF_position(int fontid, float x, float y, float z);
void BLF_size(int fontid, float size);

/* Goal: small but useful color API. */

void BLF_color4ubv(int fontid, const unsigned char rgba[4]);
void BLF_color3ubv(int fontid, const unsigned char rgb[3]);
void BLF_color3ubv_alpha(int fontid, const unsigned char rgb[3], unsigned char alpha);
void BLF_color4ub(
    int fontid, unsigned char r, unsigned char g, unsigned char b, unsigned char alpha);
void BLF_color3ub(int fontid, unsigned char r, unsigned char g, unsigned char b);
void BLF_color4f(int fontid, float r, float g, float b, float a);
void BLF_color4fv(int fontid, const float rgba[4]);
void BLF_color3f(int fontid, float r, float g, float b);
void BLF_color3fv_alpha(int fontid, const float rgb[3], float alpha);
/* Also available: `UI_FontThemeColor(fontid, colorid)`. */

/**
 * Set a 4x4 matrix to be multiplied before draw the text.
 * Remember that you need call BLF_enable(BLF_MATRIX)
 * to enable this.
 *
 * The order of the matrix is like GL:
 * \code{.unparsed}
 *  | m[0]  m[4]  m[8]  m[12] |
 *  | m[1]  m[5]  m[9]  m[13] |
 *  | m[2]  m[6]  m[10] m[14] |
 *  | m[3]  m[7]  m[11] m[15] |
 * \endcode
 */
void BLF_matrix(int fontid, const float m[16]);

/**
 * Batch draw-calls together as long as
 * the model-view matrix and the font remain unchanged.
 */
void BLF_batch_draw_begin(void);
void BLF_batch_draw_flush(void);
void BLF_batch_draw_end(void);

/**
 * Draw the string using the current font.
 */
void BLF_draw_ex(int fontid, const char *str, size_t str_len, struct ResultBLF *r_info)
    ATTR_NONNULL(2);
void BLF_draw(int fontid, const char *str, size_t str_len) ATTR_NONNULL(2);
int BLF_draw_mono(int fontid, const char *str, size_t str_len, int cwidth) ATTR_NONNULL(2);

typedef bool (*BLF_GlyphBoundsFn)(const char *str,
                                  size_t str_step_ofs,
                                  const struct rcti *bounds,
                                  void *user_data);

/**
 * Run \a user_fn for each character, with the bound-box that would be used for drawing.
 *
 * \param user_fn: Callback that runs on each glyph, returning false early exits.
 * \param user_data: User argument passed to \a user_fn.
 *
 * \note The font position, clipping, matrix and rotation are not applied.
 */
void BLF_boundbox_foreach_glyph(int fontid,
                                const char *str,
                                size_t str_len,
                                BLF_GlyphBoundsFn user_fn,
                                void *user_data) ATTR_NONNULL(2);

/**
 * Get the byte offset within a string, selected by mouse at a horizontal location.
 */
size_t BLF_str_offset_from_cursor_position(int fontid,
                                           const char *str,
                                           size_t str_len,
                                           int location_x) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(2);

/**
 * Return bounds of the glyph rect at the string offset.
 */
bool BLF_str_offset_to_glyph_bounds(int fontid,
                                    const char *str,
                                    size_t str_offset,
                                    struct rcti *glyph_bounds) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(2, 4);

/**
 * Get the string byte offset that fits within a given width.
 */
size_t BLF_width_to_strlen(int fontid,
                           const char *str,
                           size_t str_len,
                           float width,
                           float *r_width) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(2);
/**
 * Same as BLF_width_to_strlen but search from the string end.
 */
size_t BLF_width_to_rstrlen(int fontid,
                            const char *str,
                            size_t str_len,
                            float width,
                            float *r_width) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(2);

/**
 * This function return the bounding box of the string
 * and are not multiplied by the aspect.
 */
void BLF_boundbox_ex(int fontid,
                     const char *str,
                     size_t str_len,
                     struct rcti *box,
                     struct ResultBLF *r_info) ATTR_NONNULL(2);
void BLF_boundbox(int fontid, const char *str, size_t str_len, struct rcti *box) ATTR_NONNULL();

/**
 * The next both function return the width and height
 * of the string, using the current font and both value
 * are multiplied by the aspect of the font.
 */
float BLF_width_ex(int fontid, const char *str, size_t str_len, struct ResultBLF *r_info)
    ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(2);
float BLF_width(int fontid, const char *str, size_t str_len) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();
float BLF_height_ex(int fontid, const char *str, size_t str_len, struct ResultBLF *r_info)
    ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(2);
float BLF_height(int fontid, const char *str, size_t str_len) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();

/**
 * Return dimensions of the font without any sample text.
 */
int BLF_height_max(int fontid) ATTR_WARN_UNUSED_RESULT;
int BLF_width_max(int fontid) ATTR_WARN_UNUSED_RESULT;
int BLF_descender(int fontid) ATTR_WARN_UNUSED_RESULT;
int BLF_ascender(int fontid) ATTR_WARN_UNUSED_RESULT;

/**
 * The following function return the width and height of the string, but
 * just in one call, so avoid extra freetype2 stuff.
 */
void BLF_width_and_height(
    int fontid, const char *str, size_t str_len, float *r_width, float *r_height) ATTR_NONNULL();

/**
 * For fixed width fonts only, returns the width of a
 * character.
 */
float BLF_fixed_width(int fontid) ATTR_WARN_UNUSED_RESULT;

/**
 * By default, rotation and clipping are disable and
 * have to be enable/disable using BLF_enable/disable.
 */
void BLF_rotation(int fontid, float angle);
void BLF_clipping(int fontid, int xmin, int ymin, int xmax, int ymax);
void BLF_wordwrap(int fontid, int wrap_width);

#if BLF_BLUR_ENABLE
void BLF_blur(int fontid, int size);
#endif

void BLF_enable(int fontid, int option);
void BLF_disable(int fontid, int option);

/**
 * Shadow options, level is the blur level, can be 3, 5 or 0 and
 * the other argument are the RGBA color.
 * Take care that shadow need to be enable using #BLF_enable!
 */
void BLF_shadow(int fontid, int level, const float rgba[4]) ATTR_NONNULL(3);

/**
 * Set the offset for shadow text, this is the current cursor
 * position plus this offset, don't need call BLF_position before
 * this function, the current position is calculate only on
 * BLF_draw, so it's safe call this whenever you like.
 */
void BLF_shadow_offset(int fontid, int x, int y);

/**
 * Set the buffer, size and number of channels to draw, one thing to take care is call
 * this function with NULL pointer when we finish, for example:
 * \code{.c}
 * BLF_buffer(my_fbuf, my_cbuf, 100, 100, 4, true, NULL);
 *
 * ... set color, position and draw ...
 *
 * BLF_buffer(NULL, NULL, NULL, 0, 0, false, NULL);
 * \endcode
 */
void BLF_buffer(int fontid,
                float *fbuf,
                unsigned char *cbuf,
                int w,
                int h,
                int nch,
                struct ColorManagedDisplay *display);

/**
 * Set the color to be used for text.
 */
void BLF_buffer_col(int fontid, const float rgba[4]) ATTR_NONNULL(2);

/**
 * Draw the string into the buffer, this function draw in both buffer,
 * float and unsigned char _BUT_ it's not necessary set both buffer, NULL is valid here.
 */
void BLF_draw_buffer_ex(int fontid, const char *str, size_t str_len, struct ResultBLF *r_info)
    ATTR_NONNULL(2);
void BLF_draw_buffer(int fontid, const char *str, size_t str_len) ATTR_NONNULL(2);

/* `blf_thumbs.cc` */

/**
 * This function is used for generating thumbnail previews.
 *
 * \note called from a thread, so it bypasses the normal BLF_* api (which isn't thread-safe).
 */
bool BLF_thumb_preview(const char *filename, unsigned char *buf, int w, int h, int channels)
    ATTR_NONNULL();

/* `blf_default.cc` */

void BLF_default_size(float size);
void BLF_default_set(int fontid);
/**
 * Get default font ID so we can pass it to other functions.
 */
int BLF_default(void);
/**
 * Draw the string using the default font, size and DPI.
 */
void BLF_draw_default(float x, float y, float z, const char *str, size_t str_len) ATTR_NONNULL();
/**
 * Set size and DPI, and return default font ID.
 */
int BLF_set_default(void);

/* `blf_font_default.cc` */

int BLF_load_default(bool unique);
int BLF_load_mono_default(bool unique);
void BLF_load_font_stack(void);

#ifdef DEBUG
void BLF_state_print(int fontid);
#endif

/** #FontBLF.flags. */
enum {
  BLF_ROTATION = 1 << 0,
  BLF_CLIPPING = 1 << 1,
  BLF_SHADOW = 1 << 2,
  // BLF_FLAG_UNUSED_3 = 1 << 3, /* dirty */
  BLF_MATRIX = 1 << 4,
  BLF_ASPECT = 1 << 5,
  BLF_WORD_WRAP = 1 << 6,
  /** No anti-aliasing. */
  BLF_MONOCHROME = 1 << 7,
  BLF_HINTING_NONE = 1 << 8,
  BLF_HINTING_SLIGHT = 1 << 9,
  BLF_HINTING_FULL = 1 << 10,
  BLF_BOLD = 1 << 11,
  BLF_ITALIC = 1 << 12,
  /** Intended USE is monospaced, regardless of font type. */
  BLF_MONOSPACED = 1 << 13,
  /** A font within the default stack of fonts. */
  BLF_DEFAULT = 1 << 14,
  /** Must only be used as last font in the stack. */
  BLF_LAST_RESORT = 1 << 15,
  /** Failure to load this font. Don't try again. */
  BLF_BAD_FONT = 1 << 16,
  /** This font is managed by the FreeType cache subsystem. */
  BLF_CACHED = 1 << 17,
};

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

#ifdef __cplusplus
}
#endif
