/* SPDX-FileCopyrightText: 2009 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup blf
 *
 * Deals with drawing text to OpenGL or bitmap buffers.
 *
 * Also low level functions for managing \a FontBLF.
 */

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <ft2build.h>

#include FT_FREETYPE_H
#include FT_CACHE_H /* FreeType Cache. */
#include FT_GLYPH_H
#include FT_MULTIPLE_MASTERS_H /* Variable font support. */
#include FT_TRUETYPE_IDS_H     /* Code-point coverage constants. */
#include FT_TRUETYPE_TABLES_H  /* For TT_OS2 */

#include "MEM_guardedalloc.h"

#include "DNA_vec_types.h"

#include "BLI_math_bits.h"
#include "BLI_math_color_blend.h"
#include "BLI_math_matrix.h"
#include "BLI_mutex.hh"
#include "BLI_path_utils.hh"
#include "BLI_rect.h"
#include "BLI_string.h"
#include "BLI_string_cursor_utf8.h"
#include "BLI_string_utf8.h"
#include "BLI_vector.hh"

#include "BLF_api.hh"

#include "GPU_batch.hh"
#include "GPU_matrix.hh"
#include "GPU_state.hh"

#include "blf_internal.hh"
#include "blf_internal_types.hh"

#include "BLI_strict_flags.h" /* IWYU pragma: keep. Keep last. */

#ifdef WIN32
#  define FT_New_Face FT_New_Face__win32_compat
#endif

/* Batching buffer for drawing. */

BatchBLF g_batch;

/* `freetype2` handle ONLY for this file! */
static FT_Library ft_lib = nullptr;
static FTC_Manager ftc_manager = nullptr;
static FTC_CMapCache ftc_charmap_cache = nullptr;

/* Lock for FreeType library, used around face creation and deletion. */
static blender::Mutex ft_lib_mutex;

/* May be set to #UI_widgetbase_draw_cache_flush. */
static void (*blf_draw_cache_flush)() = nullptr;

static ft_pix blf_font_height_max_ft_pix(FontBLF *font);
static ft_pix blf_font_width_max_ft_pix(FontBLF *font);

/* -------------------------------------------------------------------- */

/** \name FreeType Caching
 * \{ */

static bool blf_setup_face(FontBLF *font);

/**
 * Called when a face is removed by the cache. FreeType will call #FT_Done_Face.
 */
static void blf_face_finalizer(void *object)
{
  FT_Face face = static_cast<FT_Face>(object);
  FontBLF *font = (FontBLF *)face->generic.data;
  font->face = nullptr;
}

/**
 * Called in response to #FTC_Manager_LookupFace. Now add a face to our font.
 *
 * \note Unused arguments are kept to match #FTC_Face_Requester function signature.
 */
static FT_Error blf_cache_face_requester(FTC_FaceID faceID,
                                         FT_Library lib,
                                         FT_Pointer /*req_data*/,
                                         FT_Face *face)
{
  FontBLF *font = (FontBLF *)faceID;
  int err = FT_Err_Cannot_Open_Resource;

  std::scoped_lock lock(ft_lib_mutex);
  if (font->filepath) {
    err = FT_New_Face(lib, font->filepath, 0, face);
  }
  else if (font->mem) {
    err = FT_New_Memory_Face(
        lib, static_cast<const FT_Byte *>(font->mem), (FT_Long)font->mem_size, 0, face);
  }

  if (err == FT_Err_Ok) {
    font->face = *face;
    font->face->generic.data = font;
    font->face->generic.finalizer = blf_face_finalizer;

    /* More FontBLF setup now that we have a face. */
    if (!blf_setup_face(font)) {
      err = FT_Err_Cannot_Open_Resource;
    }
  }
  else {
    /* Clear this on error to avoid exception in FTC_Manager_LookupFace. */
    *face = nullptr;
  }

  return err;
}

/**
 * Called when the FreeType cache is removing a font size.
 */
static void blf_size_finalizer(void *object)
{
  FT_Size size = static_cast<FT_Size>(object);
  FontBLF *font = (FontBLF *)size->generic.data;
  font->ft_size = nullptr;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name FreeType Utilities (Internal)
 * \{ */

uint blf_get_char_index(FontBLF *font, const uint charcode)
{
  if (font->flags & BLF_CACHED) {
    /* Use char-map cache for much faster lookup. */
    return FTC_CMapCache_Lookup(ftc_charmap_cache, font, -1, charcode);
  }
  /* Fonts that are not cached need to use the regular lookup function. */
  return blf_ensure_face(font) ? FT_Get_Char_Index(font->face, charcode) : 0;
}

/* Convert a FreeType 26.6 value representing an unscaled design size to fractional pixels. */
static ft_pix blf_unscaled_F26Dot6_to_pixels(FontBLF *font, const FT_Pos value)
{
  /* Make sure we have a valid font->ft_size. */
  blf_ensure_size(font);

  /* Scale value by font size using integer-optimized multiplication. */
  FT_Long scaled = FT_MulFix(value, font->ft_size->metrics.x_scale);

  /* Copied from FreeType's FT_Get_Kerning (with FT_KERNING_DEFAULT), scaling down. */
  /* Kerning distances at small PPEM values so that they don't become too big. */
  if (font->ft_size->metrics.x_ppem < 25) {
    scaled = FT_MulDiv(scaled, font->ft_size->metrics.x_ppem, 25);
  }

  return (ft_pix)scaled;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Glyph Batching
 * \{ */

/**
 * Draw-calls are precious! make them count!
 * Since most of the Text elements are not covered by other UI elements, we can
 * group some strings together and render them in one draw-call. This behavior
 * is on demand only, between #BLF_batch_draw_begin() and #BLF_batch_draw_end().
 */
static void blf_batch_draw_init()
{
  g_batch.glyph_buf = GPU_storagebuf_create_ex(
      sizeof(g_batch.glyph_data), nullptr, GPU_USAGE_STREAM, __func__);
  g_batch.glyph_len = 0;
  /* We render a quad as a triangle strip and instance it for each glyph. */
  g_batch.batch = GPU_batch_create_procedural(GPU_PRIM_TRI_STRIP, 4);
}

static void blf_batch_draw_exit()
{
  GPU_BATCH_DISCARD_SAFE(g_batch.batch);
  if (g_batch.glyph_buf) {
    GPU_storagebuf_free(g_batch.glyph_buf);
  }
}

void blf_batch_draw_begin(FontBLF *font)
{
  if (g_batch.batch == nullptr) {
    blf_batch_draw_init();
  }

  const bool font_changed = (g_batch.font != font);
  const bool simple_shader = ((font->flags & (BLF_ROTATION | BLF_ASPECT)) == 0);
  const bool shader_changed = (simple_shader != g_batch.simple_shader);

  g_batch.active = g_batch.enabled && simple_shader;

  if (simple_shader) {
    /* Offset is applied to each glyph. */
    g_batch.ofs[0] = font->pos[0];
    g_batch.ofs[1] = font->pos[1];
  }
  else {
    /* Offset is baked in model-view matrix. */
    zero_v2_int(g_batch.ofs);
  }

  if (g_batch.active) {
    float gpumat[4][4];
    GPU_matrix_model_view_get(gpumat);

    bool mat_changed = equals_m4m4(gpumat, g_batch.mat) == false;

    if (mat_changed) {
      /* Model view matrix is no longer the same.
       * Flush cache but with the previous matrix. */
      GPU_matrix_push();
      GPU_matrix_set(g_batch.mat);
    }

    /* Flush cache if configuration is not the same. */
    if (mat_changed || font_changed || shader_changed) {
      blf_batch_draw();
      g_batch.simple_shader = simple_shader;
      g_batch.font = font;
    }
    else {
      /* Nothing changed continue batching. */
      return;
    }

    if (mat_changed) {
      GPU_matrix_pop();
      /* Save for next `memcmp`. */
      memcpy(g_batch.mat, gpumat, sizeof(g_batch.mat));
    }
  }
  else {
    /* Flush cache. */
    blf_batch_draw();
    g_batch.font = font;
    g_batch.simple_shader = simple_shader;
  }
}

static blender::gpu::Texture *blf_batch_cache_texture_load()
{
  GlyphCacheBLF *gc = g_batch.glyph_cache;
  BLI_assert(gc);
  BLI_assert(gc->bitmap_len > 0);

  if (gc->bitmap_len > gc->bitmap_len_landed) {
    const int tex_width = GPU_texture_width(gc->texture);

    int bitmap_len_landed = gc->bitmap_len_landed;
    int remain = gc->bitmap_len - bitmap_len_landed;
    int offset_x = bitmap_len_landed % tex_width;
    int offset_y = bitmap_len_landed / tex_width;

    /* TODO(@germano): Update more than one row in a single call. */
    while (remain) {
      int remain_row = tex_width - offset_x;
      int width = remain > remain_row ? remain_row : remain;
      GPU_texture_update_sub(gc->texture,
                             GPU_DATA_UBYTE,
                             &gc->bitmap_result[bitmap_len_landed],
                             offset_x,
                             offset_y,
                             0,
                             width,
                             1,
                             0);

      bitmap_len_landed += width;
      remain -= width;
      offset_x = 0;
      offset_y += 1;
    }

    gc->bitmap_len_landed = bitmap_len_landed;
  }

  return gc->texture;
}

void blf_batch_draw()
{
  if (g_batch.glyph_len == 0) {
    return;
  }

  GPU_blend(GPU_BLEND_ALPHA);

  /* We need to flush widget base first to ensure correct ordering. */
  if (blf_draw_cache_flush != nullptr) {
    blf_draw_cache_flush();
  }

  blender::gpu::Texture *texture = blf_batch_cache_texture_load();
  GPU_storagebuf_usage_size_set(g_batch.glyph_buf, size_t(g_batch.glyph_len) * sizeof(GlyphQuad));
  GPU_storagebuf_update(g_batch.glyph_buf, g_batch.glyph_data);
  GPU_storagebuf_bind(g_batch.glyph_buf, 0);

  GPU_batch_program_set_builtin(g_batch.batch, GPU_SHADER_TEXT);
  GPU_batch_texture_bind(g_batch.batch, "glyph", texture);
  /* Setup texture width mask and shift, so that shader can avoid costly divisions. */
  int tex_width = GPU_texture_width(texture);
  BLI_assert_msg(is_power_of_2_i(tex_width), "Font texture width must be power of two");
  int width_shift = 31 - bitscan_reverse_i(tex_width);
  GPU_batch_uniform_1i(g_batch.batch, "glyph_tex_width_mask", tex_width - 1);
  GPU_batch_uniform_1i(g_batch.batch, "glyph_tex_width_shift", width_shift);
  GPU_batch_draw_advanced(g_batch.batch, 0, 4, 0, g_batch.glyph_len);

  GPU_blend(GPU_BLEND_NONE);

  GPU_texture_unbind(texture);
  g_batch.glyph_len = 0;
}

static void blf_batch_draw_end()
{
  if (!g_batch.active) {
    blf_batch_draw();
  }
}

void BLF_batch_discard()
{
  if (g_batch.glyph_buf) {
    GPU_storagebuf_free(g_batch.glyph_buf);
    g_batch.glyph_buf = GPU_storagebuf_create_ex(
        sizeof(g_batch.glyph_data), nullptr, GPU_USAGE_STREAM, __func__);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Glyph Stepping Utilities (Internal)
 * \{ */

BLI_INLINE ft_pix blf_kerning(FontBLF *font, const GlyphBLF *g_prev, const GlyphBLF *g)
{
  ft_pix adjustment = 0;

  /* Small adjust if there is hinting. */
  adjustment += g->lsb_delta - ((g_prev) ? g_prev->rsb_delta : 0);

  if (FT_HAS_KERNING(font) && g_prev) {
    FT_Vector delta = {KERNING_ENTRY_UNSET};

    /* Get unscaled kerning value from our cache if ASCII. */
    if ((g_prev->c < KERNING_CACHE_TABLE_SIZE) && (g->c < KERNING_CACHE_TABLE_SIZE)) {
      delta.x = font->kerning_cache->ascii_table[g->c][g_prev->c];
    }

    /* If not ASCII or not found in cache, ask FreeType for kerning. */
    if (UNLIKELY(font->face && delta.x == KERNING_ENTRY_UNSET)) {
      /* Note that this function sets delta values to zero on any error. */
      FT_Get_Kerning(font->face, g_prev->idx, g->idx, FT_KERNING_UNSCALED, &delta);
    }

    /* If ASCII we save this value to our cache for quicker access next time. */
    if ((g_prev->c < KERNING_CACHE_TABLE_SIZE) && (g->c < KERNING_CACHE_TABLE_SIZE)) {
      font->kerning_cache->ascii_table[g->c][g_prev->c] = int(delta.x);
    }

    if (delta.x != 0) {
      /* Convert unscaled design units to pixels and move pen. */
      adjustment += blf_unscaled_F26Dot6_to_pixels(font, delta.x);
    }
  }

  return adjustment;
}

BLI_INLINE GlyphBLF *blf_glyph_from_utf8_and_step(FontBLF *font,
                                                  GlyphCacheBLF *gc,
                                                  const GlyphBLF *g_prev,
                                                  const char *str,
                                                  const size_t str_len,
                                                  size_t *i_p,
                                                  int32_t *pen_x)
{
  uint charcode = BLI_str_utf8_as_unicode_step_safe(str, str_len, i_p);
  /* Invalid unicode sequences return the byte value, stepping forward one.
   * This allows `latin1` to display (which is sometimes used for file-paths). */
  BLI_assert(charcode != BLI_UTF8_ERR);
  GlyphBLF *g = blf_glyph_ensure(font, gc, charcode);
  if (g && pen_x && !(font->flags & BLF_MONOSPACED)) {
    *pen_x += blf_kerning(font, g_prev, g);

#ifdef BLF_SUBPIXEL_POSITION
    if (!(font->flags & BLF_RENDER_SUBPIXELAA)) {
      *pen_x = FT_PIX_ROUND(*pen_x);
    }
#else
    *pen_x = FT_PIX_ROUND(*pen_x);
#endif

#ifdef BLF_SUBPIXEL_AA
    g = blf_glyph_ensure_subpixel(font, gc, g, *pen_x);
#endif
  }
  return g;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UTF8 Utilities (Internal)
 * \{ */

/**
 * Only assert on invalid UTF8 handling if the strings are valid UTF8.
 */
[[maybe_unused]] static int blf_str_is_utf8_valid_lazy_init(const char *str,
                                                            const size_t str_len,
                                                            int &is_utf8_valid)
{
  if (is_utf8_valid == -1) {
    is_utf8_valid = BLI_str_utf8_invalid_byte(str, str_len) == -1;
  }
  return is_utf8_valid;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Text Drawing: GPU
 * \{ */

static void blf_font_draw_ex(FontBLF *font,
                             GlyphCacheBLF *gc,
                             const char *str,
                             const size_t str_len,
                             ResultBLF *r_info,
                             const ft_pix pen_y)
{
  if (str_len == 0) {
    /* Early exit, don't do any immediate-mode GPU operations. */
    return;
  }

  GlyphBLF *g = nullptr;
  ft_pix pen_x = 0;
  size_t i = 0;

  blf_batch_draw_begin(font);

  while ((i < str_len) && str[i]) {
    g = blf_glyph_from_utf8_and_step(font, gc, g, str, str_len, &i, &pen_x);
    if (UNLIKELY(g == nullptr)) {
      continue;
    }
    /* Do not return this loop if clipped, we want every character tested. */
    blf_glyph_draw(font, gc, g, ft_pix_to_int_floor(pen_x), ft_pix_to_int_floor(pen_y));
    pen_x += g->advance_x;
  }

  blf_batch_draw_end();

  if (r_info) {
    r_info->lines = 1;
    r_info->width = ft_pix_to_int(pen_x);
  }
}
void blf_font_draw(FontBLF *font, const char *str, const size_t str_len, ResultBLF *r_info)
{
  GlyphCacheBLF *gc = blf_glyph_cache_acquire(font);
  blf_font_draw_ex(font, gc, str, str_len, r_info, 0);
  blf_glyph_cache_release(font);
}

int blf_font_draw_mono(
    FontBLF *font, const char *str, const size_t str_len, const int cwidth, const int tab_columns)
{
  GlyphBLF *g;
  int columns = 0;
  ft_pix pen_x = 0, pen_y = 0;
  ft_pix cwidth_fpx = ft_pix_from_int(cwidth);

  size_t i = 0;

  GlyphCacheBLF *gc = blf_glyph_cache_acquire(font);

  blf_batch_draw_begin(font);

  while ((i < str_len) && str[i]) {
    g = blf_glyph_from_utf8_and_step(font, gc, nullptr, str, str_len, &i, nullptr);

    if (UNLIKELY(g == nullptr)) {
      continue;
    }
    /* Do not return this loop if clipped, we want every character tested. */
    blf_glyph_draw(font, gc, g, ft_pix_to_int_floor(pen_x), ft_pix_to_int_floor(pen_y));

    const int col = UNLIKELY(g->c == '\t') ? (tab_columns - (columns % tab_columns)) :
                                             BLI_wcwidth_safe(char32_t(g->c));
    columns += col;
    pen_x += cwidth_fpx * col;
  }

  blf_batch_draw_end();

  blf_glyph_cache_release(font);
  return columns;
}

#ifndef WITH_HEADLESS
void blf_draw_svg_icon(FontBLF *font,
                       const uint icon_id,
                       const float x,
                       const float y,
                       const float size,
                       const float color[4],
                       const float outline_alpha,
                       const bool multicolor,
                       blender::FunctionRef<void(std::string &)> edit_source_cb)
{
  BLI_assert(outline_alpha <= 1.0f); /* Higher values overflow, caller must ensure. */
  blf_font_size(font, size);
  font->pos[0] = int(x);
  font->pos[1] = int(y);
  font->pos[2] = 0;

  if (color != nullptr) {
    rgba_float_to_uchar(font->color, color);
  }

  if (outline_alpha > 0.0f) {
    font->flags |= BLF_SHADOW;
    font->shadow = FontShadowType::Outline;
    font->shadow_x = 0;
    font->shadow_y = 0;
    font->shadow_color[0] = 0;
    font->shadow_color[1] = 0;
    font->shadow_color[2] = 0;
    font->shadow_color[3] = char(outline_alpha * 255.0f);
  }

  GlyphCacheBLF *gc = blf_glyph_cache_acquire(font);
  blf_batch_draw_begin(font);

  GlyphBLF *g = blf_glyph_ensure_icon(gc, icon_id, multicolor, edit_source_cb);
  if (g) {
    blf_glyph_draw(font, gc, g, 0, 0);
  }

  if (outline_alpha > 0) {
    font->flags &= ~BLF_SHADOW;
  }

  blf_batch_draw_end();
  blf_glyph_cache_release(font);
}

blender::Array<uchar> blf_svg_icon_bitmap(FontBLF *font,
                                          const uint icon_id,
                                          const float size,
                                          int *r_width,
                                          int *r_height,
                                          const bool multicolor,
                                          blender::FunctionRef<void(std::string &)> edit_source_cb)
{
  blf_font_size(font, size);
  GlyphCacheBLF *gc = blf_glyph_cache_acquire(font);
  GlyphBLF *g = blf_glyph_ensure_icon(gc, icon_id, multicolor, edit_source_cb);

  if (!g) {
    blf_glyph_cache_release(font);
    *r_width = 0;
    *r_height = 0;
    return {};
  }

  *r_width = g->dims[0];
  *r_height = g->dims[1];
  blender::Array<uchar> bitmap(g->dims[0] * g->dims[1] * 4);

  if (g->num_channels == 4) {
    memcpy(bitmap.data(), g->bitmap, size_t(bitmap.size()));
  }
  else if (g->num_channels == 1) {
    for (int64_t y = 0; y < int64_t(g->dims[1]); y++) {
      for (int64_t x = 0; x < int64_t(g->dims[0]); x++) {
        int64_t offs_in = (y * int64_t(g->pitch)) + x;
        bitmap[int64_t(offs_in * 4)] = g->bitmap[offs_in];
        bitmap[int64_t(offs_in * 4 + 1)] = g->bitmap[offs_in];
        bitmap[int64_t(offs_in * 4 + 2)] = g->bitmap[offs_in];
        bitmap[int64_t(offs_in * 4 + 3)] = g->bitmap[offs_in];
      }
    }
  }
  blf_glyph_cache_release(font);
  return bitmap;
}
#endif /* WITH_HEADLESS */

/** \} */

/* -------------------------------------------------------------------- */
/** \name Text Drawing: Buffer
 * \{ */

/**
 * Draw glyph `g` into `buf_info` pixels.
 */
static void blf_glyph_draw_buffer(FontBufInfoBLF *buf_info,
                                  GlyphBLF *g,
                                  const ft_pix pen_x,
                                  const ft_pix pen_y_basis)
{
  const int chx = ft_pix_to_int(pen_x + ft_pix_from_int(g->pos[0]));
  const int chy = ft_pix_to_int(pen_y_basis + ft_pix_from_int(g->dims[1]));

  ft_pix pen_y = (g->pitch < 0) ? (pen_y_basis + ft_pix_from_int(g->dims[1] - g->pos[1])) :
                                  (pen_y_basis - ft_pix_from_int(g->dims[1] - g->pos[1]));

  if ((chx + g->dims[0]) < 0 ||                  /* Out of bounds: left. */
      chx >= buf_info->dims[0] ||                /* Out of bounds: right. */
      (ft_pix_to_int(pen_y) + g->dims[1]) < 0 || /* Out of bounds: bottom. */
      ft_pix_to_int(pen_y) >= buf_info->dims[1]  /* Out of bounds: top. */
  )
  {
    return;
  }

  /* Don't draw beyond the buffer bounds. */
  int width_clip = g->dims[0];
  int height_clip = g->dims[1];
  int yb_start = g->pitch < 0 ? 0 : g->dims[1] - 1;

  if (width_clip + chx > buf_info->dims[0]) {
    width_clip -= chx + width_clip - buf_info->dims[0];
  }
  if (height_clip + ft_pix_to_int(pen_y) > buf_info->dims[1]) {
    height_clip -= ft_pix_to_int(pen_y) + height_clip - buf_info->dims[1];
  }

  /* Clip drawing below the image. */
  if (pen_y < 0) {
    yb_start += (g->pitch < 0) ? -ft_pix_to_int(pen_y) : ft_pix_to_int(pen_y);
    height_clip += ft_pix_to_int(pen_y);
    pen_y = 0;
  }

  /* Avoid conversions in the pixel writing loop. */
  const int pen_y_px = ft_pix_to_int(pen_y);

  const float *b_col_float = buf_info->col_float;
  const uchar *b_col_char = buf_info->col_char;

  if (buf_info->fbuf) {
    int yb = yb_start;
    for (int y = ((chy >= 0) ? 0 : -chy); y < height_clip; y++) {
      const int x_start = (chx >= 0) ? 0 : -chx;
      const uchar *a_ptr = g->bitmap + x_start + (yb * g->pitch);
      const int64_t buf_ofs = (int64_t(buf_info->dims[0]) * (pen_y_px + y) + (chx + x_start)) * 4;
      float *fbuf = buf_info->fbuf + buf_ofs;
      for (int x = x_start; x < width_clip; x++, a_ptr++, fbuf += 4) {
        const char a_byte = *a_ptr;
        if (a_byte) {
          const float a = (a_byte / 255.0f) * b_col_float[3];

          float font_pixel[4];
          font_pixel[0] = b_col_float[0] * a;
          font_pixel[1] = b_col_float[1] * a;
          font_pixel[2] = b_col_float[2] * a;
          font_pixel[3] = a;
          blend_color_mix_float(fbuf, fbuf, font_pixel);
        }
      }

      if (g->pitch < 0) {
        yb++;
      }
      else {
        yb--;
      }
    }
  }

  if (buf_info->cbuf) {
    int yb = yb_start;
    for (int y = ((chy >= 0) ? 0 : -chy); y < height_clip; y++) {
      const int x_start = (chx >= 0) ? 0 : -chx;
      const uchar *a_ptr = g->bitmap + x_start + (yb * g->pitch);
      const int64_t buf_ofs = (int64_t(buf_info->dims[0]) * (pen_y_px + y) + (chx + x_start)) * 4;
      uchar *cbuf = buf_info->cbuf + buf_ofs;
      for (int x = x_start; x < width_clip; x++, a_ptr++, cbuf += 4) {
        const char a_byte = *a_ptr;

        if (a_byte) {
          const float a = (a_byte / 255.0f) * b_col_float[3];

          uchar font_pixel[4];
          font_pixel[0] = b_col_char[0];
          font_pixel[1] = b_col_char[1];
          font_pixel[2] = b_col_char[2];
          font_pixel[3] = unit_float_to_uchar_clamp(a);
          blend_color_mix_byte(cbuf, cbuf, font_pixel);
        }
      }

      if (g->pitch < 0) {
        yb++;
      }
      else {
        yb--;
      }
    }
  }
}

/* Sanity checks are done by BLF_draw_buffer() */
static void blf_font_draw_buffer_ex(FontBLF *font,
                                    GlyphCacheBLF *gc,
                                    const char *str,
                                    const size_t str_len,
                                    const ft_pix pen_y,
                                    ResultBLF *r_info)
{
  GlyphBLF *g = nullptr;
  ft_pix pen_x = ft_pix_from_int(font->pos[0]);
  ft_pix pen_y_basis = ft_pix_from_int(font->pos[1]) + pen_y;
  size_t i = 0;

  /* Buffer specific variables. */
  FontBufInfoBLF *buf_info = &font->buf_info;

  /* Another buffer specific call for color conversion. */

  while ((i < str_len) && str[i]) {
    g = blf_glyph_from_utf8_and_step(font, gc, g, str, str_len, &i, &pen_x);

    if (UNLIKELY(g == nullptr)) {
      continue;
    }
    blf_glyph_draw_buffer(buf_info, g, pen_x, pen_y_basis);
    pen_x += g->advance_x;
  }

  if (r_info) {
    r_info->lines = 1;
    r_info->width = ft_pix_to_int(pen_x);
  }
}

void blf_font_draw_buffer(FontBLF *font, const char *str, const size_t str_len, ResultBLF *r_info)
{
  GlyphCacheBLF *gc = blf_glyph_cache_acquire(font);
  blf_font_draw_buffer_ex(font, gc, str, str_len, 0, r_info);
  blf_glyph_cache_release(font);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Text Evaluation: Width to String Length
 *
 * Use to implement exported functions:
 * - #BLF_width_to_strlen
 * - #BLF_width_to_rstrlen
 * \{ */

static bool blf_font_width_to_strlen_glyph_process(FontBLF *font,
                                                   GlyphCacheBLF *gc,
                                                   const GlyphBLF *g_prev,
                                                   GlyphBLF *g,
                                                   ft_pix *pen_x,
                                                   const int width_i)
{
  if (UNLIKELY(g == nullptr)) {
    /* Continue the calling loop. */
    return false;
  }

  if (!(font->flags & BLF_MONOSPACED)) {
    *pen_x += blf_kerning(font, g_prev, g);

#ifdef BLF_SUBPIXEL_POSITION
    if (!(font->flags & BLF_RENDER_SUBPIXELAA)) {
      *pen_x = FT_PIX_ROUND(*pen_x);
    }
#else
    *pen_x = FT_PIX_ROUND(*pen_x);
#endif

#ifdef BLF_SUBPIXEL_AA
    g = blf_glyph_ensure_subpixel(font, gc, g, *pen_x);
#endif
  }

  *pen_x += g->advance_x;

  /* When true, break the calling loop. */
  return (ft_pix_to_int(*pen_x) >= width_i);
}

size_t blf_font_width_to_strlen(
    FontBLF *font, const char *str, const size_t str_len, int width, int *r_width)
{
  GlyphBLF *g;
  const GlyphBLF *g_prev;
  ft_pix pen_x;
  ft_pix width_new;
  size_t i, i_prev;

  GlyphCacheBLF *gc = blf_glyph_cache_acquire(font);
  const int width_i = width;

  for (i_prev = i = 0, width_new = pen_x = 0, g_prev = nullptr; (i < str_len) && str[i];
       i_prev = i, width_new = pen_x, g_prev = g)
  {
    g = blf_glyph_from_utf8_and_step(font, gc, nullptr, str, str_len, &i, nullptr);
    if (blf_font_width_to_strlen_glyph_process(font, gc, g_prev, g, &pen_x, width_i)) {
      break;
    }
  }

  if (r_width) {
    *r_width = ft_pix_to_int(width_new);
  }

  blf_glyph_cache_release(font);
  return i_prev;
}

size_t blf_font_width_to_rstrlen(
    FontBLF *font, const char *str, const size_t str_len, int width, int *r_width)
{
  GlyphBLF *g, *g_prev;
  ft_pix pen_x, width_new;
  size_t i, i_prev, i_tmp;
  std::optional<size_t> i_next = {};
  const char *s, *s_prev;

  GlyphCacheBLF *gc = blf_glyph_cache_acquire(font);
#ifndef NDEBUG
  int is_utf8_valid = -1;
#endif

  i = BLI_strnlen(str, str_len);
  s = BLI_str_find_prev_char_utf8(&str[i], str);
  i = size_t(s - str);
  s_prev = BLI_str_find_prev_char_utf8(s, str);
  i_prev = size_t(s_prev - str);

  i_tmp = i;
  g = blf_glyph_from_utf8_and_step(font, gc, nullptr, str, str_len, &i_tmp, nullptr);
  for (width_new = pen_x = 0; (s != nullptr && i > 0);
       i_next = i, i = i_prev, s = s_prev, g = g_prev, g_prev = nullptr, width_new = pen_x)
  {
    s_prev = BLI_str_find_prev_char_utf8(s, str);
    i_prev = size_t(s_prev - str);

    i_tmp = i_prev;
    g_prev = blf_glyph_from_utf8_and_step(font, gc, nullptr, str, str_len, &i_tmp, nullptr);
    BLI_assert(i_tmp == i ||
               /* TODO: proper handling of non UTF8 strings. */
               (blf_str_is_utf8_valid_lazy_init(str, str_len, is_utf8_valid) == 0));

    if (blf_font_width_to_strlen_glyph_process(font, gc, g_prev, g, &pen_x, width)) {
      break;
    }
  }

  if (r_width) {
    *r_width = ft_pix_to_int(width_new);
  }

  blf_glyph_cache_release(font);
  return i_next ? *i_next : i;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Text Evaluation: Glyph Bound Box with Callback
 * \{ */

static void blf_font_boundbox_ex(FontBLF *font,
                                 GlyphCacheBLF *gc,
                                 const char *str,
                                 const size_t str_len,
                                 rcti *r_box,
                                 ResultBLF *r_info,
                                 ft_pix pen_y)
{
  const GlyphBLF *g = nullptr;
  ft_pix pen_x = 0;
  size_t i = 0;

  ft_pix box_xmin = ft_pix_from_int(32000);
  ft_pix box_xmax = ft_pix_from_int(-32000);
  ft_pix box_ymin = ft_pix_from_int(32000);
  ft_pix box_ymax = ft_pix_from_int(-32000);

  while ((i < str_len) && str[i]) {
    g = blf_glyph_from_utf8_and_step(font, gc, g, str, str_len, &i, &pen_x);

    if (UNLIKELY(g == nullptr)) {
      continue;
    }
    const ft_pix pen_x_next = pen_x + g->advance_x;

    const ft_pix gbox_xmin = std::min(pen_x, pen_x + g->box_xmin);
    /* Mono-spaced characters should only use advance. See #130385. */
    const ft_pix gbox_xmax = (font->flags & BLF_MONOSPACED) ?
                                 pen_x_next :
                                 std::max(pen_x_next, pen_x + g->box_xmax);
    const ft_pix gbox_ymin = g->box_ymin + pen_y;
    const ft_pix gbox_ymax = g->box_ymax + pen_y;

    box_xmin = std::min(gbox_xmin, box_xmin);
    box_ymin = std::min(gbox_ymin, box_ymin);

    box_xmax = std::max(gbox_xmax, box_xmax);
    box_ymax = std::max(gbox_ymax, box_ymax);

    pen_x = pen_x_next;
  }

  if (box_xmin > box_xmax) {
    box_xmin = 0;
    box_ymin = 0;
    box_xmax = 0;
    box_ymax = 0;
  }

  r_box->xmin = ft_pix_to_int_floor(box_xmin);
  r_box->xmax = ft_pix_to_int_ceil(box_xmax);
  r_box->ymin = ft_pix_to_int_floor(box_ymin);
  r_box->ymax = ft_pix_to_int_ceil(box_ymax);

  if (r_info) {
    r_info->lines = 1;
    r_info->width = ft_pix_to_int(pen_x);
  }
}
void blf_font_boundbox(
    FontBLF *font, const char *str, const size_t str_len, rcti *r_box, ResultBLF *r_info)
{
  GlyphCacheBLF *gc = blf_glyph_cache_acquire(font);
  blf_font_boundbox_ex(font, gc, str, str_len, r_box, r_info, 0);
  blf_glyph_cache_release(font);
}

void blf_font_width_and_height(FontBLF *font,
                               const char *str,
                               const size_t str_len,
                               float *r_width,
                               float *r_height,
                               ResultBLF *r_info)
{
  float xa, ya;
  rcti box;

  if (font->flags & BLF_ASPECT) {
    xa = font->aspect[0];
    ya = font->aspect[1];
  }
  else {
    xa = 1.0f;
    ya = 1.0f;
  }

  if (font->flags & BLF_WORD_WRAP) {
    blf_font_boundbox__wrap(font, str, str_len, &box, r_info);
  }
  else {
    blf_font_boundbox(font, str, str_len, &box, r_info);
  }
  *r_width = (float(BLI_rcti_size_x(&box)) * xa);
  *r_height = (float(BLI_rcti_size_y(&box)) * ya);
}

float blf_font_width(FontBLF *font, const char *str, const size_t str_len, ResultBLF *r_info)
{
  float xa;
  rcti box;

  if (font->flags & BLF_ASPECT) {
    xa = font->aspect[0];
  }
  else {
    xa = 1.0f;
  }

  if (font->flags & BLF_WORD_WRAP) {
    blf_font_boundbox__wrap(font, str, str_len, &box, r_info);
  }
  else {
    blf_font_boundbox(font, str, str_len, &box, r_info);
  }
  return float(BLI_rcti_size_x(&box)) * xa;
}

float blf_font_height(FontBLF *font, const char *str, const size_t str_len, ResultBLF *r_info)
{
  float ya;
  rcti box;

  if (font->flags & BLF_ASPECT) {
    ya = font->aspect[1];
  }
  else {
    ya = 1.0f;
  }

  if (font->flags & BLF_WORD_WRAP) {
    blf_font_boundbox__wrap(font, str, str_len, &box, r_info);
  }
  else {
    blf_font_boundbox(font, str, str_len, &box, r_info);
  }
  return float(BLI_rcti_size_y(&box)) * ya;
}

float blf_font_fixed_width(FontBLF *font)
{
  const GlyphCacheBLF *gc = blf_glyph_cache_acquire(font);
  float width = (gc) ? float(gc->fixed_width) : font->size / 2.0f;
  blf_glyph_cache_release(font);
  return width;
}

int blf_font_glyph_advance(FontBLF *font, const char *str)
{
  GlyphCacheBLF *gc = blf_glyph_cache_acquire(font);
  const uint charcode = BLI_str_utf8_as_unicode_safe(str);
  const GlyphBLF *g = blf_glyph_ensure(font, gc, charcode);

  if (UNLIKELY(g == nullptr)) {
    blf_glyph_cache_release(font);
    return 0;
  }

  const int glyph_advance = ft_pix_to_int(g->advance_x);

  blf_glyph_cache_release(font);
  return glyph_advance;
}

void blf_font_boundbox_foreach_glyph(FontBLF *font,
                                     const char *str,
                                     const size_t str_len,
                                     BLF_GlyphBoundsFn user_fn,
                                     void *user_data)
{
  if (str_len == 0 || str[0] == 0) {
    /* Early exit. */
    return;
  }

  const GlyphBLF *g = nullptr;
  ft_pix pen_x = 0;
  size_t i = 0;

  GlyphCacheBLF *gc = blf_glyph_cache_acquire(font);

  while ((i < str_len) && str[i]) {
    const size_t i_curr = i;
    g = blf_glyph_from_utf8_and_step(font, gc, g, str, str_len, &i, &pen_x);

    if (UNLIKELY(g == nullptr || g->advance_x == 0)) {
      /* Ignore combining characters like diacritical marks. */
      continue;
    }
    rcti bounds;
    bounds.xmin = ft_pix_to_int_floor(pen_x) + ft_pix_to_int_floor(g->box_xmin);
    bounds.xmax = ft_pix_to_int_floor(pen_x) + ft_pix_to_int_ceil(g->box_xmax);
    bounds.ymin = ft_pix_to_int_floor(g->box_ymin);
    bounds.ymax = ft_pix_to_int_ceil(g->box_ymax);

    if (user_fn(str, i_curr, &bounds, user_data) == false) {
      break;
    }
    pen_x += g->advance_x;
  }

  blf_glyph_cache_release(font);
}

struct CursorPositionForeachGlyph_Data {
  /** Horizontal position to test. */
  int location_x;
  /** Write the character offset here. */
  size_t r_offset;
};

static bool blf_cursor_position_foreach_glyph(const char * /*str*/,
                                              const size_t str_step_ofs,
                                              const rcti *bounds,
                                              void *user_data)
{
  CursorPositionForeachGlyph_Data *data = static_cast<CursorPositionForeachGlyph_Data *>(
      user_data);
  if (data->location_x < (bounds->xmin + bounds->xmax) / 2) {
    data->r_offset = str_step_ofs;
    return false;
  }
  return true;
}

size_t blf_str_offset_from_cursor_position(FontBLF *font,
                                           const char *str,
                                           size_t str_len,
                                           int location_x)
{
  /* Do not early exit if location_x <= 0, as this can result in an incorrect
   * offset for RTL text. Instead of offset of character responsible for first
   * glyph you'd get offset of first character, which could be the last glyph. */
  if (!str || !str[0] || !str_len) {
    return 0;
  }

  CursorPositionForeachGlyph_Data data{};
  data.location_x = location_x;
  data.r_offset = size_t(-1);

  blf_font_boundbox_foreach_glyph(font, str, str_len, blf_cursor_position_foreach_glyph, &data);

  if (data.r_offset == size_t(-1)) {
    /* We are to the right of the string, so return position of null terminator. */
    data.r_offset = BLI_strnlen(str, str_len);
  }
  else if (BLI_str_utf8_char_width_or_error(&str[data.r_offset]) == 0) {
    /* This is a combining character, so move to previous visible valid char. */
    int offset = int(data.r_offset);
    BLI_str_cursor_step_prev_utf8(str, int(str_len), &offset);
    data.r_offset = size_t(offset);
  }

  return data.r_offset;
}

struct StrOffsetToGlyphBounds_Data {
  size_t str_offset;
  rcti bounds;
};

static bool blf_str_offset_foreach_glyph(const char * /*str*/,
                                         const size_t str_step_ofs,
                                         const rcti *bounds,
                                         void *user_data)
{
  StrOffsetToGlyphBounds_Data *data = static_cast<StrOffsetToGlyphBounds_Data *>(user_data);
  if (data->str_offset == str_step_ofs) {
    data->bounds = *bounds;
    return false;
  }
  return true;
}

void blf_str_offset_to_glyph_bounds(FontBLF *font,
                                    const char *str,
                                    size_t str_offset,
                                    rcti *r_glyph_bounds)
{
  StrOffsetToGlyphBounds_Data data{};
  data.str_offset = str_offset;
  data.bounds = {0};

  blf_font_boundbox_foreach_glyph(font, str, str_offset + 1, blf_str_offset_foreach_glyph, &data);
  *r_glyph_bounds = data.bounds;
}

int blf_str_offset_to_cursor(FontBLF *font,
                             const char *str,
                             const size_t str_len,
                             const size_t str_offset,
                             const int cursor_width)
{
  if (!str || !str[0]) {
    return 0;
  }

  /* Right edge of the previous character, if available. */
  rcti prev = {0};
  if (str_offset > 0) {
    blf_str_offset_to_glyph_bounds(font, str, str_offset - 1, &prev);
  }

  /* Left edge of the next character, if available. */
  rcti next = {0};
  if (str_offset < strlen(str)) {
    blf_str_offset_to_glyph_bounds(font, str, str_offset, &next);
  }

  if ((prev.xmax == prev.xmin) && next.xmax) {
    /* Nothing (or a space) to the left, so align to right character. */
    return next.xmin - (cursor_width / 2);
  }
  if ((prev.xmax != prev.xmin) && !next.xmax) {
    /* End of string, so align to last character. */
    return prev.xmax - (cursor_width / 2);
  }
  if (prev.xmax && next.xmax) {
    /* Between two characters, so use the center. */
    if (next.xmin >= prev.xmax || next.xmin == next.xmax) {
      return ((prev.xmax + next.xmin) - cursor_width) / 2;
    }
    /* A nicer center if reversed order - RTL. */
    return ((next.xmax + prev.xmin) - cursor_width) / 2;
  }
  if (!str_offset) {
    /* Start of string. */
    return 0 - cursor_width;
  }
  return int(blf_font_width(font, str, str_len, nullptr));
}

blender::Vector<blender::Bounds<int>> blf_str_selection_boxes(
    FontBLF *font, const char *str, size_t str_len, size_t sel_start, size_t sel_length)
{
  blender::Vector<blender::Bounds<int>> boxes;
  const int start = blf_str_offset_to_cursor(font, str, str_len, sel_start, 0);
  const int end = blf_str_offset_to_cursor(font, str, str_len, sel_start + sel_length, 0);
  boxes.append(blender::Bounds(start, end));
  return boxes;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Text Evaluation: Word-Wrap with Callback
 * \{ */

/**
 * Generic function to add word-wrap support for other existing functions.
 *
 * Wraps on spaces and respects newlines.
 * Intentionally ignores non-unix newlines, tabs and more advanced text formatting.
 *
 * \note If we want rich text - we better have a higher level API to handle that
 * (color, bold, switching fonts... etc).
 */
static void blf_font_wrap_apply(FontBLF *font,
                                const char *str,
                                const size_t str_len,
                                const int max_pixel_width,
                                BLFWrapMode mode,
                                ResultBLF *r_info,
                                void (*callback)(FontBLF *font,
                                                 GlyphCacheBLF *gc,
                                                 const char *str,
                                                 const size_t str_len,
                                                 ft_pix pen_y,
                                                 void *userdata),
                                void *userdata)
{
  GlyphBLF *g = nullptr;
  const GlyphBLF *g_prev = nullptr;
  ft_pix pen_x = 0;
  ft_pix pen_y = 0;
  size_t i = 0;
  int lines = 0;
  ft_pix pen_x_next = 0;

  /* Size of characters not shown at the end of the wrapped line. */
  size_t clip_bytes = 0;

  ft_pix line_height = blf_font_height_max_ft_pix(font);

  GlyphCacheBLF *gc = blf_glyph_cache_acquire(font);

  struct WordWrapVars {
    ft_pix wrap_width;
    size_t start, last[2];
  } wrap = {max_pixel_width != -1 ? ft_pix_from_int(max_pixel_width) : INT_MAX, 0, {0, 0}};

  // printf("%s wrapping (%d, %d) `%s`:\n", __func__, str_len, strlen(str), str);
  while ((i < str_len) && str[i]) {

    /* Wrap variables. */
    const size_t i_curr = i;
    bool do_draw = false;

    g = blf_glyph_from_utf8_and_step(font, gc, g_prev, str, str_len, &i, &pen_x);

    const ft_pix advance_x = g ? g->advance_x : 0;
    const uint codepoint = BLI_str_utf8_as_unicode_safe(&str[i_curr]);
    const uint codepoint_prev = g_prev ? g_prev->c : 0;

    /**
     * Implementation Detail (UTF8).
     *
     * Take care with single byte offsets here,
     * since this is UTF8 we can't be sure a single byte is a single character.
     *
     * This is _only_ done when we know for sure the character is ASCII (newline or a space).
     */
    pen_x_next = pen_x + advance_x;

    if (UNLIKELY((pen_x_next >= wrap.wrap_width) && (wrap.start != wrap.last[0]))) {
      do_draw = true;
    }
    else if (UNLIKELY((int(mode) & int(BLFWrapMode::HardLimit)) &&
                      (pen_x_next >= wrap.wrap_width) && (advance_x != 0)))
    {
      wrap.last[0] = i_curr;
      wrap.last[1] = i_curr;
      do_draw = true;
      clip_bytes = 0;
    }
    else if (UNLIKELY(((i < str_len) && str[i]) == 0)) {
      /* Need check here for trailing newline, else we draw it. */
      wrap.last[0] = i + ((codepoint != '\n') ? 1 : 0);
      wrap.last[1] = i;
      do_draw = true;
      clip_bytes = 0;
    }
    else if (UNLIKELY(codepoint == '\n')) {
      wrap.last[0] = i_curr + 1;
      wrap.last[1] = i;
      do_draw = true;
      clip_bytes = 1;
    }
    else if (UNLIKELY(((int(mode) & int(BLFWrapMode::Minimal)) == int(BLFWrapMode::Minimal)) &&
                      codepoint != ' ' && (g_prev ? g_prev->c == ' ' : false)))
    {
      wrap.last[0] = i_curr;
      wrap.last[1] = i_curr;
      clip_bytes = 1;
    }
    else if (UNLIKELY(int(mode) & int(BLFWrapMode::Path))) {
      if (ELEM(codepoint, SEP, ' ', '?', '&', '=')) {
        /* Break and leave at the end of line. */
        wrap.last[0] = i;
        wrap.last[1] = i;
        clip_bytes = 0;
      }
      else if (ELEM(codepoint, '-', '_', '.', '%')) {
        /* Break and move to the next line. */
        wrap.last[0] = i_curr;
        wrap.last[1] = i_curr;
        clip_bytes = 0;
      }
    }
    else if (UNLIKELY((int(mode) & int(BLFWrapMode::Typographical)) &&
                      !BLI_str_utf32_char_is_breaking_space(codepoint) &&
                      BLI_str_utf32_char_is_breaking_space(codepoint_prev)))
    {
      /* Optional break after space, removing it. */
      wrap.last[0] = i_curr;
      wrap.last[1] = i_curr;
      clip_bytes = BLI_str_utf8_from_unicode_len(codepoint_prev);
    }
    else if (UNLIKELY((int(mode) & int(BLFWrapMode::Typographical)) &&
                      BLI_str_utf32_char_is_optional_break_after(codepoint, codepoint_prev)))
    {
      /* Optional break after various characters, keeping it. */
      wrap.last[0] = i;
      wrap.last[1] = i;
      clip_bytes = 0;
    }
    else if (UNLIKELY((int(mode) & int(BLFWrapMode::Typographical)) &&
                      BLI_str_utf32_char_is_optional_break_before(codepoint, codepoint_prev)))
    {
      /* Optional break before various characters. */
      wrap.last[0] = i_curr;
      wrap.last[1] = i_curr;
      clip_bytes = 0;
    }

    if (UNLIKELY(do_draw)) {
#if 0
      printf("(%03d..%03d)  `%.*s`\n",
             wrap.start,
             wrap.last[0],
             (wrap.last[0] - wrap.start) - 1,
             &str[wrap.start]);
#endif

      callback(
          font, gc, &str[wrap.start], (wrap.last[0] - wrap.start) - clip_bytes, pen_y, userdata);
      wrap.start = wrap.last[0];
      i = wrap.last[1];
      pen_x = 0;
      pen_y -= line_height;
      g_prev = nullptr;
      lines += 1;
      continue;
    }

    pen_x = pen_x_next;
    g_prev = g;
  }

  // printf("done! lines: %d, width, %d\n", lines, pen_x_next);

  if (r_info) {
    r_info->lines = lines;
    /* Width of last line only (with wrapped lines). */
    r_info->width = ft_pix_to_int(pen_x_next);
  }

  blf_glyph_cache_release(font);
}

/** Utility for #blf_font_draw__wrap. */
static void blf_font_draw__wrap_cb(FontBLF *font,
                                   GlyphCacheBLF *gc,
                                   const char *str,
                                   const size_t str_len,
                                   ft_pix pen_y,
                                   void * /*userdata*/)
{
  blf_font_draw_ex(font, gc, str, str_len, nullptr, pen_y);
}
void blf_font_draw__wrap(FontBLF *font, const char *str, const size_t str_len, ResultBLF *r_info)
{
  blf_font_wrap_apply(font,
                      str,
                      str_len,
                      font->wrap_width,
                      font->wrap_mode,
                      r_info,
                      blf_font_draw__wrap_cb,
                      nullptr);
}

/** Utility for #blf_font_boundbox__wrap. */
static void blf_font_boundbox_wrap_cb(FontBLF *font,
                                      GlyphCacheBLF *gc,
                                      const char *str,
                                      const size_t str_len,
                                      ft_pix pen_y,
                                      void *userdata)
{
  rcti *box = static_cast<rcti *>(userdata);
  rcti box_single;

  blf_font_boundbox_ex(font, gc, str, str_len, &box_single, nullptr, pen_y);
  BLI_rcti_union(box, &box_single);
}
void blf_font_boundbox__wrap(
    FontBLF *font, const char *str, const size_t str_len, rcti *r_box, ResultBLF *r_info)
{
  r_box->xmin = 32000;
  r_box->xmax = -32000;
  r_box->ymin = 32000;
  r_box->ymax = -32000;

  blf_font_wrap_apply(font,
                      str,
                      str_len,
                      font->wrap_width,
                      font->wrap_mode,
                      r_info,
                      blf_font_boundbox_wrap_cb,
                      r_box);
}

/** Utility for  #blf_font_draw_buffer__wrap. */
static void blf_font_draw_buffer__wrap_cb(FontBLF *font,
                                          GlyphCacheBLF *gc,
                                          const char *str,
                                          const size_t str_len,
                                          const ft_pix pen_y,
                                          void * /*userdata*/)
{
  blf_font_draw_buffer_ex(font, gc, str, str_len, pen_y, nullptr);
}
void blf_font_draw_buffer__wrap(FontBLF *font,
                                const char *str,
                                const size_t str_len,
                                ResultBLF *r_info)
{
  blf_font_wrap_apply(font,
                      str,
                      str_len,
                      font->wrap_width,
                      font->wrap_mode,
                      r_info,
                      blf_font_draw_buffer__wrap_cb,
                      nullptr);
}

/** Wrap a blender::StringRef. */
static void blf_font_string_wrap_cb(FontBLF * /*font*/,
                                    GlyphCacheBLF * /*gc*/,
                                    const char *str,
                                    const size_t str_len,
                                    const ft_pix /*pen_y*/,
                                    void *str_list_ptr)
{
  blender::Vector<blender::StringRef> *list = static_cast<blender::Vector<blender::StringRef> *>(
      str_list_ptr);
  blender::StringRef line(str, str + str_len);
  list->append(line);
}

blender::Vector<blender::StringRef> blf_font_string_wrap(FontBLF *font,
                                                         blender::StringRef str,
                                                         int max_pixel_width,
                                                         BLFWrapMode mode)
{
  blender::Vector<blender::StringRef> list;
  blf_font_wrap_apply(font,
                      str.data(),
                      size_t(str.size()),
                      max_pixel_width,
                      mode,
                      nullptr,
                      blf_font_string_wrap_cb,
                      &list);
  return list;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Font Query: Attributes
 * \{ */

static ft_pix blf_font_height_max_ft_pix(FontBLF *font)
{
  blf_ensure_size(font);
  /* #Metrics::height is rounded to pixel. Force minimum of one pixel. */
  return std::max((ft_pix)font->ft_size->metrics.height, ft_pix_from_int(1));
}

int blf_font_height_max(FontBLF *font)
{
  return ft_pix_to_int(blf_font_height_max_ft_pix(font));
}

static ft_pix blf_font_width_max_ft_pix(FontBLF *font)
{
  blf_ensure_size(font);
  /* #Metrics::max_advance is rounded to pixel. Force minimum of one pixel. */
  return std::max((ft_pix)font->ft_size->metrics.max_advance, ft_pix_from_int(1));
}

int blf_font_width_max(FontBLF *font)
{
  return ft_pix_to_int(blf_font_width_max_ft_pix(font));
}

int blf_font_descender(FontBLF *font)
{
  blf_ensure_size(font);
  return ft_pix_to_int((ft_pix)font->ft_size->metrics.descender);
}

int blf_font_ascender(FontBLF *font)
{
  blf_ensure_size(font);
  return ft_pix_to_int((ft_pix)font->ft_size->metrics.ascender);
}

bool blf_font_bounds_max(FontBLF *font, rctf *r_bounds)
{
  if (!blf_ensure_face(font)) {
    return false;
  }

  r_bounds->xmin = float(font->face->bbox.xMin) / float(font->face->units_per_EM) * font->size;
  r_bounds->xmax = float(font->face->bbox.xMax) / float(font->face->units_per_EM) * font->size;
  r_bounds->ymin = float(font->face->bbox.yMin) / float(font->face->units_per_EM) * font->size;
  r_bounds->ymax = float(font->face->bbox.yMax) / float(font->face->units_per_EM) * font->size;
  return true;
}

char *blf_display_name(FontBLF *font)
{
  if (!blf_ensure_face(font) || !font->face->family_name) {
    return nullptr;
  }
  return BLI_sprintfN("%s %s", font->face->family_name, font->face->style_name);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Font Subsystem Init/Exit
 * \{ */

int blf_font_init()
{
  memset(&g_batch, 0, sizeof(g_batch));
  int err = FT_Init_FreeType(&ft_lib);
  if (err == FT_Err_Ok) {
    /* Create a FreeType cache manager. */
    err = FTC_Manager_New(ft_lib,
                          BLF_CACHE_MAX_FACES,
                          BLF_CACHE_MAX_SIZES,
                          BLF_CACHE_BYTES,
                          blf_cache_face_requester,
                          nullptr,
                          &ftc_manager);
    if (err == FT_Err_Ok) {
      /* Create a character-map cache to speed up glyph index lookups. */
      err = FTC_CMapCache_New(ftc_manager, &ftc_charmap_cache);
    }
  }
  return err;
}

void blf_font_exit()
{
  if (ftc_manager) {
    FTC_Manager_Done(ftc_manager);
  }
  if (ft_lib) {
    FT_Done_FreeType(ft_lib);
  }
  blf_batch_draw_exit();
}

void BLF_cache_flush_set_fn(void (*cache_flush_fn)())
{
  blf_draw_cache_flush = cache_flush_fn;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Font New/Free
 * \{ */

static void blf_font_fill(FontBLF *font)
{
  font->aspect[0] = 1.0f;
  font->aspect[1] = 1.0f;
  font->aspect[2] = 1.0f;
  font->pos[0] = 0;
  font->pos[1] = 0;
  font->angle = 0.0f;

  /* Use an easily identifiable bright color (yellow)
   * so its clear when #BLF_color calls are missing. */
  font->color[0] = 255;
  font->color[1] = 255;
  font->color[2] = 0;
  font->color[3] = 255;

  font->clip_rec.xmin = 0;
  font->clip_rec.xmax = 0;
  font->clip_rec.ymin = 0;
  font->clip_rec.ymax = 0;
  font->flags = BLF_NONE;
  font->size = 0;
  font->char_weight = 400;
  font->char_slant = 0.0f;
  font->char_width = 1.0f;
  font->char_spacing = 0.0f;

  font->kerning_cache = nullptr;
  font->tex_size_max = -1;

  font->buf_info.fbuf = nullptr;
  font->buf_info.cbuf = nullptr;
  font->buf_info.dims[0] = 0;
  font->buf_info.dims[1] = 0;
  font->buf_info.col_init[0] = 0;
  font->buf_info.col_init[1] = 0;
  font->buf_info.col_init[2] = 0;
  font->buf_info.col_init[3] = 0;
}

/**
 * NOTE(@Harley): that the data the following function creates is not yet used.
 * But do not remove it as it will be used in the near future.
 */
static void blf_font_metrics(FT_Face face, FontMetrics *metrics)
{
  /* Members with non-zero defaults. */
  metrics->weight = 400;
  metrics->width = 1.0f;

  const TT_OS2 *os2_table = (const TT_OS2 *)FT_Get_Sfnt_Table(face, FT_SFNT_OS2);
  if (os2_table) {
    /* The default (resting) font weight. */
    if (os2_table->usWeightClass >= 1 && os2_table->usWeightClass <= 1000) {
      metrics->weight = short(os2_table->usWeightClass);
    }

    /* Width value is one of integers 1-9 with known values. */
    if (os2_table->usWidthClass >= 1 && os2_table->usWidthClass <= 9) {
      switch (os2_table->usWidthClass) {
        case 1:
          metrics->width = 0.5f;
          break;
        case 2:
          metrics->width = 0.625f;
          break;
        case 3:
          metrics->width = 0.75f;
          break;
        case 4:
          metrics->width = 0.875f;
          break;
        case 5:
          metrics->width = 1.0f;
          break;
        case 6:
          metrics->width = 1.125f;
          break;
        case 7:
          metrics->width = 1.25f;
          break;
        case 8:
          metrics->width = 1.5f;
          break;
        case 9:
          metrics->width = 2.0f;
          break;
      }
    }

    metrics->strikeout_position = short(os2_table->yStrikeoutPosition);
    metrics->strikeout_thickness = short(os2_table->yStrikeoutSize);
    metrics->subscript_size = short(os2_table->ySubscriptYSize);
    metrics->subscript_xoffset = short(os2_table->ySubscriptXOffset);
    metrics->subscript_yoffset = short(os2_table->ySubscriptYOffset);
    metrics->superscript_size = short(os2_table->ySuperscriptYSize);
    metrics->superscript_xoffset = short(os2_table->ySuperscriptXOffset);
    metrics->superscript_yoffset = short(os2_table->ySuperscriptYOffset);
    metrics->family_class = short(os2_table->sFamilyClass);
    metrics->selection_flags = short(os2_table->fsSelection);
    metrics->first_charindex = short(os2_table->usFirstCharIndex);
    metrics->last_charindex = short(os2_table->usLastCharIndex);
    if (os2_table->version > 1) {
      metrics->cap_height = short(os2_table->sCapHeight);
      metrics->x_height = short(os2_table->sxHeight);
    }
  }

  /* The Post table usually contains a slant value, but in counter-clockwise degrees. */
  const TT_Postscript *post_table = (const TT_Postscript *)FT_Get_Sfnt_Table(face, FT_SFNT_POST);
  if (post_table) {
    if (post_table->italicAngle != 0) {
      metrics->slant = float(post_table->italicAngle) / -65536.0f;
    }
  }

  /* Metrics copied from those gathered by FreeType. */
  metrics->units_per_EM = short(face->units_per_EM);
  metrics->ascender = short(face->ascender);
  metrics->descender = short(face->descender);
  metrics->line_height = short(face->height);
  metrics->max_advance_width = short(face->max_advance_width);
  metrics->max_advance_height = short(face->max_advance_height);
  metrics->underline_position = short(face->underline_position);
  metrics->underline_thickness = short(face->underline_thickness);
  metrics->num_glyphs = int(face->num_glyphs);

  if (metrics->cap_height == 0) {
    /* Calculate or guess cap height if it is not set in the font. */
    FT_UInt gi = FT_Get_Char_Index(face, uint('H'));
    if (gi && FT_Load_Glyph(face, gi, FT_LOAD_NO_SCALE | FT_LOAD_NO_BITMAP) == FT_Err_Ok) {
      metrics->cap_height = short(face->glyph->metrics.height);
    }
    else {
      metrics->cap_height = short(float(metrics->units_per_EM) * 0.7f);
    }
  }

  if (metrics->x_height == 0) {
    /* Calculate or guess x-height if it is not set in the font. */
    FT_UInt gi = FT_Get_Char_Index(face, uint('x'));
    if (gi && FT_Load_Glyph(face, gi, FT_LOAD_NO_SCALE | FT_LOAD_NO_BITMAP) == FT_Err_Ok) {
      metrics->x_height = short(face->glyph->metrics.height);
    }
    else {
      metrics->x_height = short(float(metrics->units_per_EM) * 0.5f);
    }
  }

  FT_UInt gi = FT_Get_Char_Index(face, uint('o'));
  if (gi && FT_Load_Glyph(face, gi, FT_LOAD_NO_SCALE | FT_LOAD_NO_BITMAP) == FT_Err_Ok) {
    metrics->o_proportion = float(face->glyph->metrics.width) / float(face->glyph->metrics.height);
  }

  if (metrics->ascender == 0) {
    /* Set a sane value for ascender if not set in the font. */
    metrics->ascender = short(float(metrics->units_per_EM) * 0.8f);
  }

  if (metrics->descender == 0) {
    /* Set a sane value for descender if not set in the font. */
    metrics->descender = metrics->ascender - metrics->units_per_EM;
  }

  if (metrics->weight == 400 && face->style_flags & FT_STYLE_FLAG_BOLD) {
    /* Normal weight yet this is an bold font, so set a sane weight value. */
    metrics->weight = 700;
  }

  if (metrics->slant == 0.0f && face->style_flags & FT_STYLE_FLAG_ITALIC) {
    /* No slant yet this is an italic font, so set a sane slant value. */
    metrics->slant = 8.0f;
  }

  if (metrics->underline_position == 0) {
    metrics->underline_position = short(float(metrics->units_per_EM) * -0.2f);
  }

  if (metrics->underline_thickness == 0) {
    metrics->underline_thickness = short(float(metrics->units_per_EM) * 0.07f);
  }

  if (metrics->strikeout_position == 0) {
    metrics->strikeout_position = short(float(metrics->x_height) * 0.6f);
  }

  if (metrics->strikeout_thickness == 0) {
    metrics->strikeout_thickness = metrics->underline_thickness;
  }

  if (metrics->subscript_size == 0) {
    metrics->subscript_size = short(float(metrics->units_per_EM) * 0.6f);
  }

  if (metrics->subscript_yoffset == 0) {
    metrics->subscript_yoffset = short(float(metrics->units_per_EM) * 0.075f);
  }

  if (metrics->superscript_size == 0) {
    metrics->superscript_size = short(float(metrics->units_per_EM) * 0.6f);
  }

  if (metrics->superscript_yoffset == 0) {
    metrics->superscript_yoffset = short(float(metrics->units_per_EM) * 0.35f);
  }

  metrics->valid = true;
}

/**
 * Extra FontBLF setup needed after it gets a Face. Called from
 * both blf_ensure_face and from the blf_cache_face_requester callback.
 */
static bool blf_setup_face(FontBLF *font)
{
  font->face_flags = font->face->face_flags;

  if (FT_HAS_MULTIPLE_MASTERS(font) && !font->variations) {
    FT_Get_MM_Var(font->face, &(font->variations));
  }

  if (!font->metrics.valid) {
    blf_font_metrics(font->face, &font->metrics);
    font->char_weight = font->metrics.weight;
    font->char_slant = font->metrics.slant;
    font->char_width = font->metrics.width;
    font->char_spacing = font->metrics.spacing;
  }

  if (FT_IS_FIXED_WIDTH(font)) {
    font->flags |= BLF_MONOSPACED;
  }

  if (FT_HAS_KERNING(font) && !font->kerning_cache) {
    /* Create kerning cache table and fill with value indicating "unset". */
    font->kerning_cache = MEM_mallocN<KerningCacheBLF>(__func__);
    for (uint i = 0; i < KERNING_CACHE_TABLE_SIZE; i++) {
      for (uint j = 0; j < KERNING_CACHE_TABLE_SIZE; j++) {
        font->kerning_cache->ascii_table[i][j] = KERNING_ENTRY_UNSET;
      }
    }
  }

  return true;
}

bool blf_ensure_face(FontBLF *font)
{
  if (font->face) {
    return true;
  }

  if (font->flags & BLF_BAD_FONT) {
    return false;
  }

  FT_Error err;

  if (font->flags & BLF_CACHED) {
    err = FTC_Manager_LookupFace(ftc_manager, font, &font->face);
  }
  else {
    std::scoped_lock lock(ft_lib_mutex);
    if (font->filepath) {
      err = FT_New_Face(font->ft_lib, font->filepath, 0, &font->face);
    }
    if (font->mem) {
      err = FT_New_Memory_Face(font->ft_lib,
                               static_cast<const FT_Byte *>(font->mem),
                               (FT_Long)font->mem_size,
                               0,
                               &font->face);
    }
    if (!err) {
      font->face->generic.data = font;
    }
  }

  if (err) {
    if (ELEM(err, FT_Err_Unknown_File_Format, FT_Err_Unimplemented_Feature)) {
      printf("Format of this font file is not supported\n");
    }
    else {
      printf("Error encountered while opening font file\n");
    }
    font->flags |= BLF_BAD_FONT;
    return false;
  }

  if (font->face && !(font->face->face_flags & FT_FACE_FLAG_SCALABLE)) {
    printf("Font is not scalable\n");
    return false;
  }

  err = FT_Select_Charmap(font->face, FT_ENCODING_UNICODE);
  if (err) {
    err = FT_Select_Charmap(font->face, FT_ENCODING_APPLE_ROMAN);
  }
  if (err && font->face->num_charmaps > 0) {
    err = FT_Select_Charmap(font->face, font->face->charmaps[0]->encoding);
  }
  if (err) {
    printf("Can't set a character map!\n");
    font->flags |= BLF_BAD_FONT;
    return false;
  }

  if (font->filepath) {
    char *mfile = blf_dir_metrics_search(font->filepath);
    if (mfile) {
      err = FT_Attach_File(font->face, mfile);
      if (err) {
        fprintf(stderr,
                "FT_Attach_File failed to load '%s' with error %d\n",
                font->filepath,
                int(err));
      }
      MEM_freeN(mfile);
    }
  }

  if (!(font->flags & BLF_CACHED)) {
    /* Not cached so point at the face's size for convenience. */
    font->ft_size = font->face->size;
  }

  /* Setup Font details that require having a Face. */
  return blf_setup_face(font);
}

struct FaceDetails {
  char filename[50];
  uint coverage1;
  uint coverage2;
  uint coverage3;
  uint coverage4;
};

/* Details about the fallback fonts we ship, so that we can load only when needed. */
static const FaceDetails static_face_details[] = {
    {"Noto Sans CJK Regular.woff2",
     0,
     TT_UCR_CJK_SYMBOLS | TT_UCR_HIRAGANA | TT_UCR_KATAKANA | TT_UCR_BOPOMOFO | TT_UCR_CJK_MISC |
         TT_UCR_ENCLOSED_CJK_LETTERS_MONTHS | TT_UCR_CJK_COMPATIBILITY |
         TT_UCR_CJK_UNIFIED_IDEOGRAPHS | TT_UCR_CJK_COMPATIBILITY_IDEOGRAPHS,
     TT_UCR_CJK_COMPATIBILITY_FORMS,
     0},
    {"NotoEmoji-VariableFont_wght.woff2", 0x80000003L, 0x241E4ACL, 0x14000000L, 0x4000000L},
    {"NotoSansArabic-VariableFont_wdth,wght.woff2",
     TT_UCR_ARABIC,
     uint(TT_UCR_ARABIC_PRESENTATION_FORMS_A),
     TT_UCR_ARABIC_PRESENTATION_FORMS_B,
     0},
    {"NotoSansArmenian-VariableFont_wdth,wght.woff2", TT_UCR_ARMENIAN, 0, 0, 0},
    {"NotoSansBengali-VariableFont_wdth,wght.woff2", TT_UCR_BENGALI, 0, 0, 0},
    {"NotoSansDevanagari-Regular.woff2", TT_UCR_DEVANAGARI, 0, 0, 0},
    {"NotoSansEthiopic-Regular.woff2", 0, 0, TT_UCR_ETHIOPIC, 0},
    {"NotoSansGeorgian-VariableFont_wdth,wght.woff2", TT_UCR_GEORGIAN, 0, 0, 0},
    {"NotoSansGujarati-Regular.woff2", TT_UCR_GUJARATI, 0, 0, 0},
    {"NotoSansGurmukhi-VariableFont_wdth,wght.woff2", TT_UCR_GURMUKHI, 0, 0, 0},
    {"NotoSansHebrew-Regular.woff2", TT_UCR_HEBREW, 0, 0, 0},
    {"NotoSansJavanese-Regular.woff2", 0x80000003L, 0x2000L, 0, 0},
    {"NotoSansKannada-VariableFont_wdth,wght.woff2", TT_UCR_KANNADA, 0, 0, 0},
    {"NotoSansKhmer-VariableFont_wdth,wght.woff2", 0, 0, TT_UCR_KHMER, 0},
    {"NotoSansMalayalam-VariableFont_wdth,wght.woff2", TT_UCR_MALAYALAM, 0, 0, 0},
    {"NotoSansMath-Regular.woff2", 0, TT_UCR_MATHEMATICAL_OPERATORS, 0, 0},
    {"NotoSansMyanmar-Regular.woff2", 0, 0, TT_UCR_MYANMAR, 0},
    {"NotoSansSymbols-VariableFont_wght.woff2", 0x3L, 0x200E4B4L, 0, 0},
    {"NotoSansSymbols2-Regular.woff2", 0x80000003L, 0x200E3E4L, 0x40020L, 0x580A048L},
    {"NotoSansTamil-VariableFont_wdth,wght.woff2", TT_UCR_TAMIL, 0, 0, 0},
    {"NotoSansTelugu-VariableFont_wdth,wght.woff2", TT_UCR_TELUGU, 0, 0, 0},
    {"NotoSansThai-VariableFont_wdth,wght.woff2", TT_UCR_THAI, 0, 0, 0},
};

/**
 * Create a new font from filename OR memory pointer.
 * For normal operation pass nullptr as FT_Library object. Pass a custom FT_Library if you
 * want to use the font without its lifetime being managed by the FreeType cache subsystem.
 */
static FontBLF *blf_font_new_impl(const char *filepath,
                                  const char *mem_name,
                                  const uchar *mem,
                                  const size_t mem_size,
                                  void *ft_library)
{
  FontBLF *font = MEM_new<FontBLF>(__func__);

  font->mem_name = mem_name ? BLI_strdup(mem_name) : nullptr;
  font->filepath = filepath ? BLI_strdup(filepath) : nullptr;
  if (mem) {
    font->mem = (void *)mem;
    font->mem_size = mem_size;
  }
  blf_font_fill(font);

  if (ft_library && ((FT_Library)ft_library != ft_lib)) {
    /* Pass. */
  }
  else {
    font->flags |= BLF_CACHED;
  }

  font->ft_lib = ft_library ? (FT_Library)ft_library : ft_lib;

  /* If we have static details about this font file, we don't have to load the Face yet. */
  bool face_needed = true;

  if (font->filepath) {
    const char *filename = BLI_path_basename(font->filepath);
    for (int i = 0; i < int(ARRAY_SIZE(static_face_details)); i++) {
      if (BLI_path_cmp(static_face_details[i].filename, filename) == 0) {
        const FaceDetails *static_details = &static_face_details[i];
        font->unicode_ranges[0] = static_details->coverage1;
        font->unicode_ranges[1] = static_details->coverage2;
        font->unicode_ranges[2] = static_details->coverage3;
        font->unicode_ranges[3] = static_details->coverage4;
        face_needed = false;
        break;
      }
    }
  }

  if (face_needed) {
    if (!blf_ensure_face(font)) {
      blf_font_free(font);
      return nullptr;
    }

    /* Save TrueType table with bits to quickly test most unicode block coverage. */
    const TT_OS2 *os2_table = (const TT_OS2 *)FT_Get_Sfnt_Table(font->face, FT_SFNT_OS2);
    if (os2_table) {
      font->unicode_ranges[0] = uint(os2_table->ulUnicodeRange1);
      font->unicode_ranges[1] = uint(os2_table->ulUnicodeRange2);
      font->unicode_ranges[2] = uint(os2_table->ulUnicodeRange3);
      font->unicode_ranges[3] = uint(os2_table->ulUnicodeRange4);
    }
  }

  /* Detect "Last resort" fonts. They have everything. Usually except last 5 bits. */
  if (font->unicode_ranges[0] == 0xffffffffU && font->unicode_ranges[1] == 0xffffffffU &&
      font->unicode_ranges[2] == 0xffffffffU && font->unicode_ranges[3] >= 0x7FFFFFFU)
  {
    font->flags |= BLF_LAST_RESORT;
  }

  return font;
}

FontBLF *blf_font_new_from_filepath(const char *filepath)
{
  return blf_font_new_impl(filepath, nullptr, nullptr, 0, nullptr);
}

FontBLF *blf_font_new_from_mem(const char *mem_name, const uchar *mem, const size_t mem_size)
{
  return blf_font_new_impl(nullptr, mem_name, mem, mem_size, nullptr);
}

void blf_font_attach_from_mem(FontBLF *font, const uchar *mem, const size_t mem_size)
{
  FT_Open_Args open;

  open.flags = FT_OPEN_MEMORY;
  open.memory_base = (const FT_Byte *)mem;
  open.memory_size = (FT_Long)mem_size;
  if (blf_ensure_face(font)) {
    FT_Attach_Stream(font->face, &open);
  }
}

void blf_font_free(FontBLF *font)
{
  blf_glyph_cache_clear(font);

  if (font->kerning_cache) {
    MEM_freeN(font->kerning_cache);
  }

  if (font->variations) {
    FT_Done_MM_Var(font->ft_lib, font->variations);
  }

  if (font->face) {
    std::scoped_lock lock(ft_lib_mutex);
    if (font->flags & BLF_CACHED) {
      FTC_Manager_RemoveFaceID(ftc_manager, font);
    }
    else {
      FT_Done_Face(font->face);
    }
    font->face = nullptr;
  }
  if (font->filepath) {
    MEM_freeN(font->filepath);
  }
  if (font->mem_name) {
    MEM_freeN(font->mem_name);
  }

  MEM_delete(font);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Font Configure
 * \{ */

void blf_ensure_size(FontBLF *font)
{
  if (font->ft_size || !(font->flags & BLF_CACHED)) {
    return;
  }

  FTC_ScalerRec scaler = {nullptr};
  scaler.face_id = font;
  scaler.width = 0;
  scaler.height = round_fl_to_uint(font->size * 64.0f);
  scaler.pixel = 0;
  scaler.x_res = BLF_DPI;
  scaler.y_res = BLF_DPI;
  if (FTC_Manager_LookupSize(ftc_manager, &scaler, &font->ft_size) == FT_Err_Ok) {
    font->ft_size->generic.data = (void *)font;
    font->ft_size->generic.finalizer = blf_size_finalizer;
    return;
  }

  BLI_assert_unreachable();
}

bool blf_font_size(FontBLF *font, float size)
{
  if (!blf_ensure_face(font)) {
    return false;
  }

  /* FreeType uses fixed-point integers in 64ths. */
  FT_UInt ft_size = round_fl_to_uint(size * 64.0f);
  /* Adjust our new size to be on even 64ths. */
  size = float(ft_size) / 64.0f;

  if (font->size != size) {
    if (font->flags & BLF_CACHED) {
      FTC_ScalerRec scaler = {nullptr};
      scaler.face_id = font;
      scaler.width = 0;
      scaler.height = ft_size;
      scaler.pixel = 0;
      scaler.x_res = BLF_DPI;
      scaler.y_res = BLF_DPI;
      if (FTC_Manager_LookupSize(ftc_manager, &scaler, &font->ft_size) != FT_Err_Ok) {
        return false;
      }
      font->ft_size->generic.data = (void *)font;
      font->ft_size->generic.finalizer = blf_size_finalizer;
    }
    else {
      if (FT_Set_Char_Size(font->face, 0, ft_size, BLF_DPI, BLF_DPI) != FT_Err_Ok) {
        return false;
      }
      font->ft_size = font->face->size;
    }
  }

  font->size = size;
  return true;
}

/** \} */
