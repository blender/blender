/* SPDX-FileCopyrightText: 2009 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup blf
 */

#pragma once

#include "BLF_enums.hh"

#include "BLI_array.hh"
#include "BLI_bounds_types.hh"
#include "BLI_compiler_attrs.h"
#include "BLI_function_ref.hh"
#include "BLI_string_ref.hh"
#include "BLI_sys_types.h"
#include "BLI_vector.hh"

/** Name of sub-directory inside #BLENDER_DATAFILES that contains font files. */
#define BLF_DATAFILES_FONTS_DIR "fonts"

/** File name of the default variable-width font. */
#define BLF_DEFAULT_PROPORTIONAL_FONT "Inter.woff2"

/** File name of the default fixed-pitch font. */
#define BLF_DEFAULT_MONOSPACED_FONT "DejaVuSansMono.woff2"

struct ListBase;
struct ResultBLF;
struct rcti;
struct rctf;

namespace blender::ocio {
class ColorSpace;
}  // namespace blender::ocio
using ColorSpace = blender::ocio::ColorSpace;

int BLF_init();
void BLF_exit();

/**
 * Close any user-loaded fonts that are not used by the Interface. Call when
 * loading new blend files so that the old fonts are not still taking resources.
 */
void BLF_reset_fonts();

void BLF_cache_clear();

/**
 * Optional cache flushing function, called before #blf_batch_draw.
 */
void BLF_cache_flush_set_fn(void (*cache_flush_fn)());

/**
 * Loads a font, or returns an already loaded font and increments its reference count.
 *
 * Note that while loading fonts is thread-safe, most of font usage via BLF
 * state modification functions is not. If you need to use fonts from multiple threads,
 * use unique font instances for threaded parts, see #BLF_load_unique.
 */
int BLF_load(const char *filepath) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
int BLF_load_mem(const char *name, const unsigned char *mem, int mem_size) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1, 2);

bool BLF_is_loaded(const char *filepath) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
bool BLF_is_loaded_mem(const char *name) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
bool BLF_is_loaded_id(int fontid) ATTR_WARN_UNUSED_RESULT;

/**
 * Loads a font into a new font object.
 *
 * Unlike #BLF_load, it does not look whether a font with the same
 * path or name is already loaded. Primary use case is when using BLF
 * functions from a non-main thread.
 */
int BLF_load_unique(const char *filepath) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
int BLF_load_mem_unique(const char *name, const unsigned char *mem, int mem_size)
    ATTR_NONNULL(1, 2);

/**
 * Decreases font reference count, if it reaches zero the font is unloaded.
 */
void BLF_unload(const char *filepath) ATTR_NONNULL(1);
#if 0 /* Not needed at the moment. */
void BLF_unload_mem(const char *name) ATTR_NONNULL(1);
#endif

/**
 * Decreases font reference count, if it reaches zero the font is unloaded.
 * Returns true if font got unloaded.
 */
bool BLF_unload_id(int fontid);

void BLF_unload_all();

/**
 * Increases font reference count.
 */
void BLF_addref_id(int fontid);

char *BLF_display_name_from_file(const char *filepath) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);

char *BLF_display_name_from_id(int fontid);

/**
 * Get the metrics needed for the initial sizing of text objects.
 */
bool BLF_get_vfont_metrics(int fontid, float *ascend_ratio, float *em_ratio, float *scale);

#define BLF_VFONT_METRICS_SCALE_DEFAULT float(1.0 / 1000.0)
#define BLF_VFONT_METRICS_EM_RATIO_DEFAULT 1.0f
#define BLF_VFONT_METRICS_ASCEND_RATIO_DEFAULT 0.8f

/**
 * Convert a character's outlines into curves.
 * \return success if the character was found and converted.
 */
bool BLF_character_to_curves(int fontid,
                             unsigned int unicode,
                             ListBase *nurbsbase,
                             const float scale,
                             bool use_fallback,
                             float *r_advance);

/**
 * Check if font supports a particular glyph.
 */
bool BLF_has_glyph(int fontid, unsigned int unicode) ATTR_WARN_UNUSED_RESULT;

/**
 * Attach a file with metrics information from memory.
 */
void BLF_metrics_attach(int fontid, const unsigned char *mem, int mem_size) ATTR_NONNULL(2);

void BLF_aspect(int fontid, float x, float y, float z);
void BLF_position(int fontid, float x, float y, float z);
void BLF_size(int fontid, float size);

/**
 * Weight class: 100 (Thin) - 400 (Normal) - 900 (Heavy).
 */
void BLF_character_weight(int fontid, int weight);

/**
 * \return the font's default design weight (100-900).
 */
int BLF_default_weight(int fontid) ATTR_WARN_UNUSED_RESULT;

/**
 * \return true if the font has a variable (multiple master) weight axis.
 */
bool BLF_has_variable_weight(int fontid) ATTR_WARN_UNUSED_RESULT;

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
 * Batch draw-calls together as long as
 * the model-view matrix and the font remain unchanged.
 */
void BLF_batch_draw_begin();
void BLF_batch_draw_flush();
void BLF_batch_draw_end();

/* Discard any batching in process and restart.
 * Only used as a workaround for glitchy driver sync. */
void BLF_batch_discard();

/**
 * Draw the string using the current font.
 */
void BLF_draw(int fontid, const char *str, size_t str_len, ResultBLF *r_info = nullptr)
    ATTR_NONNULL(2);
int BLF_draw_mono(int fontid, const char *str, size_t str_len, int cwidth, int tab_columns)
    ATTR_NONNULL(2);

void BLF_draw_svg_icon(uint icon_id,
                       float x,
                       float y,
                       float size,
                       const float color[4] = nullptr,
                       float outline_alpha = 1.0f,
                       bool multicolor = false,
                       blender::FunctionRef<void(std::string &)> edit_source_cb = nullptr);

blender::Array<uchar> BLF_svg_icon_bitmap(
    uint icon_id,
    float size,
    int *r_width,
    int *r_height,
    bool multicolor = false,
    blender::FunctionRef<void(std::string &)> edit_source_cb = nullptr);

using BLF_GlyphBoundsFn = bool (*)(const char *str,
                                   size_t str_step_ofs,
                                   const rcti *bounds,
                                   void *user_dataconst);

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
                                    rcti *r_glyph_bounds) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(2, 4);

/**
 * Return left edge of text cursor (caret), given a character offset and cursor width.
 */
int BLF_str_offset_to_cursor(
    int fontid, const char *str, size_t str_len, size_t str_offset, int cursor_width);

/**
 * Return bounds of selection boxes. There is just one normally but there could
 * be more for multi-line and when containing text of differing directions.
 */
blender::Vector<blender::Bounds<int>> BLF_str_selection_boxes(
    int fontid, const char *str, size_t str_len, size_t sel_start, size_t sel_length);

/**
 * Get the string byte offset that fits within a given width.
 */
size_t BLF_width_to_strlen(int fontid,
                           const char *str,
                           size_t str_len,
                           float width,
                           float *r_width) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(2);
/**
 * Same as #BLF_width_to_strlen but search from the string end.
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
void BLF_boundbox(int fontid,
                  const char *str,
                  size_t str_len,
                  rcti *r_box,
                  ResultBLF *r_info = nullptr) ATTR_NONNULL(2);

/**
 * The next both function return the width and height
 * of the string, using the current font and both value
 * are multiplied by the aspect of the font.
 */
float BLF_width(int fontid, const char *str, size_t str_len, ResultBLF *r_info = nullptr)
    ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(2);
float BLF_height(int fontid, const char *str, size_t str_len, ResultBLF *r_info = nullptr)
    ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(2);

/**
 * Return dimensions of the font without any sample text.
 */
int BLF_height_max(int fontid) ATTR_WARN_UNUSED_RESULT;
int BLF_width_max(int fontid) ATTR_WARN_UNUSED_RESULT;
int BLF_descender(int fontid) ATTR_WARN_UNUSED_RESULT;
int BLF_ascender(int fontid) ATTR_WARN_UNUSED_RESULT;

/**
 * Returns the minimum bounding box that can enclose all glyphs in the font at
 * the current size. Expect negative values as Y=0 is the baseline, X=0 is normal
 * advance position (glyphs can have negative bearing and positioning). There
 * should be little use for this as it is best to measure the bounds of the actual
 * text to be drawn. These values (unscaled) are set in the font file, not calculated
 * from the actual glyphs at load time. This should be considered correct but it is
 * possible, although very unlikely, for a defective font to contain incorrect values.
 */
bool BLF_bounds_max(int fontid, rctf *r_bounds) ATTR_NONNULL(2);

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
 * Returns offset for drawing next character in the string.
 */
int BLF_glyph_advance(int fontid, const char *str);

/**
 * By default, rotation and clipping are disable and
 * have to be enable/disable using BLF_enable/disable.
 */
void BLF_rotation(int fontid, float angle);
void BLF_clipping(int fontid, int xmin, int ymin, int xmax, int ymax);
void BLF_wordwrap(int fontid, int wrap_width, BLFWrapMode mode = BLFWrapMode::Minimal);

blender::Vector<blender::StringRef> BLF_string_wrap(int fontid,
                                                    blender::StringRef str,
                                                    const int max_pixel_width,
                                                    BLFWrapMode mode = BLFWrapMode::Minimal);

void BLF_enable(int fontid, FontFlags flag);
void BLF_disable(int fontid, FontFlags flag);

/**
 * Is this font part of the default fonts in the fallback stack?
 */
bool BLF_is_builtin(int fontid);

/**
 * Note that shadow needs to be enabled with #BLF_enable.
 */
void BLF_shadow(int fontid, FontShadowType type, const float rgba[4] = nullptr);

/**
 * Set the offset for shadow text, this is the current cursor
 * position plus this offset, don't need call BLF_position before
 * this function, the current position is calculate only on
 * BLF_draw, so it's safe call this whenever you like.
 */
void BLF_shadow_offset(int fontid, int x, int y);

/**
 * Make font be rasterized into a given memory image/buffer.
 * The image is assumed to have 4 color channels (RGBA) per pixel.
 * When done, call this function with null buffer pointers.
 */
void BLF_buffer(
    int fontid, float *fbuf, unsigned char *cbuf, int w, int h, const ColorSpace *colorspace);

/**
 * Opaque structure used to push/pop values set by the #BLF_buffer function.
 */
struct BLFBufferState;
/**
 * Store the current buffer state.
 * This state *must* be popped with #BLF_buffer_state_pop.
 */
BLFBufferState *BLF_buffer_state_push(int fontid);
/**
 * Pop the state (restoring the state when #BLF_buffer_state_push was called).
 */
void BLF_buffer_state_pop(BLFBufferState *buffer_state);
/**
 * Free the state, only use in the rare case pop is not called
 * (if the font itself is unloaded after pushing for example).
 */
void BLF_buffer_state_free(BLFBufferState *buffer_state);

/**
 * Set the color to be used for text.
 */
void BLF_buffer_col(int fontid, const float srgb_color[4]) ATTR_NONNULL(2);

/**
 * Draw the string into the buffer, this function draw in both buffer,
 * float and unsigned char _BUT_ it's not necessary set both buffer, NULL is valid here.
 */
void BLF_draw_buffer(int fontid, const char *str, size_t str_len, ResultBLF *r_info = nullptr)
    ATTR_NONNULL(2);

/* `blf_thumbs.cc` */

/**
 * This function is used for generating thumbnail previews.
 *
 * \note called from a thread, so it bypasses the normal BLF_* API (which isn't thread-safe).
 */
bool BLF_thumb_preview(const char *filepath, unsigned char *buf, int w, int h, int channels)
    ATTR_NONNULL();

/* `blf_default.cc` */

void BLF_default_size(float size);
void BLF_default_set(int fontid);
/**
 * Get default font ID so we can pass it to other functions.
 */
int BLF_default();
/**
 * Draw the string using the default font, size and DPI.
 */
void BLF_draw_default(float x, float y, float z, const char *str, size_t str_len) ATTR_NONNULL();
/**
 * Set size and DPI, and return default font ID.
 */
int BLF_set_default();

/* `blf_font_default.cc` */

int BLF_load_default(bool unique);
int BLF_load_mono_default(bool unique);
void BLF_load_font_stack();

#ifndef NDEBUG
void BLF_state_print(int fontid);
#endif

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
