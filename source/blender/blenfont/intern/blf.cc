/* SPDX-FileCopyrightText: 2009 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup blf
 *
 * Main BlenFont (BLF) API, public functions for font handling.
 *
 * Wraps OpenGL and FreeType.
 */

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <ft2build.h>

#include FT_FREETYPE_H
#include FT_GLYPH_H

#include "MEM_guardedalloc.h"

#include "BLI_fileops.h"
#include "BLI_math.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_threads.h"

#include "BLF_api.h"

#include "IMB_colormanagement.h"

#include "GPU_matrix.h"
#include "GPU_shader.h"

#include "blf_internal.h"
#include "blf_internal_types.h"

#define BLF_RESULT_CHECK_INIT(r_info) \
  if (r_info) { \
    memset(r_info, 0, sizeof(*(r_info))); \
  } \
  ((void)0)

FontBLF *global_font[BLF_MAX_FONT] = {nullptr};

/* XXX: should these be made into global_font_'s too? */

int blf_mono_font = -1;
int blf_mono_font_render = -1;

static FontBLF *blf_get(int fontid)
{
  if (fontid >= 0 && fontid < BLF_MAX_FONT) {
    return global_font[fontid];
  }
  return nullptr;
}

int BLF_init()
{
  for (int i = 0; i < BLF_MAX_FONT; i++) {
    global_font[i] = nullptr;
  }

  return blf_font_init();
}

void BLF_exit()
{
  for (int i = 0; i < BLF_MAX_FONT; i++) {
    FontBLF *font = global_font[i];
    if (font) {
      blf_font_free(font);
      global_font[i] = nullptr;
    }
  }

  blf_font_exit();
}

void BLF_cache_clear()
{
  for (int i = 0; i < BLF_MAX_FONT; i++) {
    FontBLF *font = global_font[i];
    if (font) {
      blf_glyph_cache_clear(font);
    }
  }
}

bool blf_font_id_is_valid(int fontid)
{
  return blf_get(fontid) != nullptr;
}

static int blf_search_by_mem_name(const char *mem_name)
{
  for (int i = 0; i < BLF_MAX_FONT; i++) {
    const FontBLF *font = global_font[i];
    if (font == nullptr || font->mem_name == nullptr) {
      continue;
    }
    if (font && STREQ(font->mem_name, mem_name)) {
      return i;
    }
  }

  return -1;
}

static int blf_search_by_filepath(const char *filepath)
{
  for (int i = 0; i < BLF_MAX_FONT; i++) {
    const FontBLF *font = global_font[i];
    if (font && (BLI_path_cmp(font->filepath, filepath) == 0)) {
      return i;
    }
  }

  return -1;
}

static int blf_search_available()
{
  for (int i = 0; i < BLF_MAX_FONT; i++) {
    if (!global_font[i]) {
      return i;
    }
  }

  return -1;
}

bool BLF_has_glyph(int fontid, uint unicode)
{
  FontBLF *font = blf_get(fontid);
  if (font) {
    return blf_get_char_index(font, unicode) != FT_Err_Ok;
  }
  return false;
}

bool BLF_is_loaded(const char *filepath)
{
  return blf_search_by_filepath(filepath) >= 0;
}

bool BLF_is_loaded_mem(const char *name)
{
  return blf_search_by_mem_name(name) >= 0;
}

int BLF_load(const char *filepath)
{
  /* check if we already load this font. */
  int i = blf_search_by_filepath(filepath);
  if (i >= 0) {
    FontBLF *font = global_font[i];
    font->reference_count++;
    return i;
  }

  return BLF_load_unique(filepath);
}

int BLF_load_unique(const char *filepath)
{
  /* Don't search in the cache!! make a new
   * object font, this is for keep fonts threads safe.
   */
  int i = blf_search_available();
  if (i == -1) {
    printf("Too many fonts!!!\n");
    return -1;
  }

  /* This isn't essential, it will just cause confusing behavior to load a font
   * that appears to succeed, then doesn't show up. */
  if (!BLI_exists(filepath)) {
    printf("Can't find font: %s\n", filepath);
    return -1;
  }

  FontBLF *font = blf_font_new_from_filepath(filepath);

  if (!font) {
    printf("Can't load font: %s\n", filepath);
    return -1;
  }

  font->reference_count = 1;
  global_font[i] = font;
  return i;
}

void BLF_metrics_attach(int fontid, uchar *mem, int mem_size)
{
  FontBLF *font = blf_get(fontid);

  if (font) {
    blf_font_attach_from_mem(font, mem, mem_size);
  }
}

int BLF_load_mem(const char *name, const uchar *mem, int mem_size)
{
  int i = blf_search_by_mem_name(name);
  if (i >= 0) {
    // font = global_font[i]; /* UNUSED */
    return i;
  }
  return BLF_load_mem_unique(name, mem, mem_size);
}

int BLF_load_mem_unique(const char *name, const uchar *mem, int mem_size)
{
  /*
   * Don't search in the cache, make a new object font!
   * this is to keep the font thread safe.
   */
  int i = blf_search_available();
  if (i == -1) {
    printf("Too many fonts!!!\n");
    return -1;
  }

  if (!mem_size) {
    printf("Can't load font: %s from memory!!\n", name);
    return -1;
  }

  FontBLF *font = blf_font_new_from_mem(name, mem, mem_size);
  if (!font) {
    printf("Can't load font: %s from memory!!\n", name);
    return -1;
  }

  font->reference_count = 1;
  global_font[i] = font;
  return i;
}

void BLF_unload(const char *filepath)
{
  for (int i = 0; i < BLF_MAX_FONT; i++) {
    FontBLF *font = global_font[i];
    if (font == nullptr || font->filepath == nullptr) {
      continue;
    }

    if (BLI_path_cmp(font->filepath, filepath) == 0) {
      BLI_assert(font->reference_count > 0);
      font->reference_count--;

      if (font->reference_count == 0) {
        blf_font_free(font);
        global_font[i] = nullptr;
      }
    }
  }
}

void BLF_unload_id(int fontid)
{
  FontBLF *font = blf_get(fontid);
  if (font) {
    BLI_assert(font->reference_count > 0);
    font->reference_count--;

    if (font->reference_count == 0) {
      blf_font_free(font);
      global_font[fontid] = nullptr;
    }
  }
}

void BLF_unload_all()
{
  for (int i = 0; i < BLF_MAX_FONT; i++) {
    FontBLF *font = global_font[i];
    if (font) {
      blf_font_free(font);
      global_font[i] = nullptr;
    }
  }
  blf_mono_font = -1;
  blf_mono_font_render = -1;
  BLF_default_set(-1);
}

void BLF_enable(int fontid, int option)
{
  FontBLF *font = blf_get(fontid);

  if (font) {
    font->flags |= option;
  }
}

void BLF_disable(int fontid, int option)
{
  FontBLF *font = blf_get(fontid);

  if (font) {
    font->flags &= ~option;
  }
}

void BLF_aspect(int fontid, float x, float y, float z)
{
  FontBLF *font = blf_get(fontid);

  if (font) {
    font->aspect[0] = x;
    font->aspect[1] = y;
    font->aspect[2] = z;
  }
}

void BLF_matrix(int fontid, const float m[16])
{
  FontBLF *font = blf_get(fontid);

  if (font) {
    memcpy(font->m, m, sizeof(font->m));
  }
}

void BLF_position(int fontid, float x, float y, float z)
{
  FontBLF *font = blf_get(fontid);

  if (font) {
    float xa, ya, za;
    float remainder;

    if (font->flags & BLF_ASPECT) {
      xa = font->aspect[0];
      ya = font->aspect[1];
      za = font->aspect[2];
    }
    else {
      xa = 1.0f;
      ya = 1.0f;
      za = 1.0f;
    }

    remainder = x - floorf(x);
    if (remainder > 0.4f && remainder < 0.6f) {
      if (remainder < 0.5f) {
        x -= 0.1f * xa;
      }
      else {
        x += 0.1f * xa;
      }
    }

    remainder = y - floorf(y);
    if (remainder > 0.4f && remainder < 0.6f) {
      if (remainder < 0.5f) {
        y -= 0.1f * ya;
      }
      else {
        y += 0.1f * ya;
      }
    }

    remainder = z - floorf(z);
    if (remainder > 0.4f && remainder < 0.6f) {
      if (remainder < 0.5f) {
        z -= 0.1f * za;
      }
      else {
        z += 0.1f * za;
      }
    }

    font->pos[0] = round_fl_to_int(x);
    font->pos[1] = round_fl_to_int(y);
    font->pos[2] = round_fl_to_int(z);
  }
}

void BLF_size(int fontid, float size)
{
  FontBLF *font = blf_get(fontid);

  if (font) {
    blf_font_size(font, size);
  }
}

#if BLF_BLUR_ENABLE
void BLF_blur(int fontid, int size)
{
  FontBLF *font = blf_get(fontid);

  if (font) {
    font->blur = size;
  }
}
#endif

void BLF_color4ubv(int fontid, const uchar rgba[4])
{
  FontBLF *font = blf_get(fontid);

  if (font) {
    font->color[0] = rgba[0];
    font->color[1] = rgba[1];
    font->color[2] = rgba[2];
    font->color[3] = rgba[3];
  }
}

void BLF_color3ubv_alpha(int fontid, const uchar rgb[3], uchar alpha)
{
  FontBLF *font = blf_get(fontid);

  if (font) {
    font->color[0] = rgb[0];
    font->color[1] = rgb[1];
    font->color[2] = rgb[2];
    font->color[3] = alpha;
  }
}

void BLF_color3ubv(int fontid, const uchar rgb[3])
{
  BLF_color3ubv_alpha(fontid, rgb, 255);
}

void BLF_color4ub(int fontid, uchar r, uchar g, uchar b, uchar alpha)
{
  FontBLF *font = blf_get(fontid);

  if (font) {
    font->color[0] = r;
    font->color[1] = g;
    font->color[2] = b;
    font->color[3] = alpha;
  }
}

void BLF_color3ub(int fontid, uchar r, uchar g, uchar b)
{
  FontBLF *font = blf_get(fontid);

  if (font) {
    font->color[0] = r;
    font->color[1] = g;
    font->color[2] = b;
    font->color[3] = 255;
  }
}

void BLF_color4fv(int fontid, const float rgba[4])
{
  FontBLF *font = blf_get(fontid);

  if (font) {
    rgba_float_to_uchar(font->color, rgba);
  }
}

void BLF_color4f(int fontid, float r, float g, float b, float a)
{
  const float rgba[4] = {r, g, b, a};
  BLF_color4fv(fontid, rgba);
}

void BLF_color3fv_alpha(int fontid, const float rgb[3], float alpha)
{
  float rgba[4];
  copy_v3_v3(rgba, rgb);
  rgba[3] = alpha;
  BLF_color4fv(fontid, rgba);
}

void BLF_color3f(int fontid, float r, float g, float b)
{
  const float rgba[4] = {r, g, b, 1.0f};
  BLF_color4fv(fontid, rgba);
}

void BLF_batch_draw_begin()
{
  BLI_assert(g_batch.enabled == false);
  g_batch.enabled = true;
}

void BLF_batch_draw_flush()
{
  if (g_batch.enabled) {
    blf_batch_draw();
  }
}

void BLF_batch_draw_end()
{
  BLI_assert(g_batch.enabled == true);
  blf_batch_draw(); /* Draw remaining glyphs */
  g_batch.enabled = false;
}

static void blf_draw_gl__start(const FontBLF *font)
{
  /*
   * The pixmap alignment hack is handle
   * in BLF_position (old ui_rasterpos_safe).
   */

  if ((font->flags & (BLF_ROTATION | BLF_MATRIX | BLF_ASPECT)) == 0) {
    return; /* glyphs will be translated individually and batched. */
  }

  GPU_matrix_push();

  if (font->flags & BLF_MATRIX) {
    GPU_matrix_mul(font->m);
  }

  GPU_matrix_translate_3f(font->pos[0], font->pos[1], font->pos[2]);

  if (font->flags & BLF_ASPECT) {
    GPU_matrix_scale_3fv(font->aspect);
  }

  if (font->flags & BLF_ROTATION) {
    GPU_matrix_rotate_2d(RAD2DEG(font->angle));
  }
}

static void blf_draw_gl__end(const FontBLF *font)
{
  if ((font->flags & (BLF_ROTATION | BLF_MATRIX | BLF_ASPECT)) != 0) {
    GPU_matrix_pop();
  }
}

void BLF_draw_ex(int fontid, const char *str, const size_t str_len, ResultBLF *r_info)
{
  FontBLF *font = blf_get(fontid);

  BLF_RESULT_CHECK_INIT(r_info);

  if (font) {
    blf_draw_gl__start(font);
    if (font->flags & BLF_WORD_WRAP) {
      blf_font_draw__wrap(font, str, str_len, r_info);
    }
    else {
      blf_font_draw(font, str, str_len, r_info);
    }
    blf_draw_gl__end(font);
  }
}
void BLF_draw(int fontid, const char *str, const size_t str_len)
{
  if (str_len == 0 || str[0] == '\0') {
    return;
  }

  /* Avoid bgl usage to corrupt BLF drawing. */
  GPU_bgl_end();

  BLF_draw_ex(fontid, str, str_len, nullptr);
}

int BLF_draw_mono(int fontid, const char *str, const size_t str_len, int cwidth)
{
  if (str_len == 0 || str[0] == '\0') {
    return 0;
  }

  FontBLF *font = blf_get(fontid);
  int columns = 0;

  if (font) {
    blf_draw_gl__start(font);
    columns = blf_font_draw_mono(font, str, str_len, cwidth);
    blf_draw_gl__end(font);
  }

  return columns;
}

void BLF_boundbox_foreach_glyph(
    int fontid, const char *str, size_t str_len, BLF_GlyphBoundsFn user_fn, void *user_data)
{
  FontBLF *font = blf_get(fontid);

  if (font) {
    if (font->flags & BLF_WORD_WRAP) {
      /* TODO: word-wrap support. */
      BLI_assert(0);
    }
    else {
      blf_font_boundbox_foreach_glyph(font, str, str_len, user_fn, user_data);
    }
  }
}

size_t BLF_str_offset_from_cursor_position(int fontid,
                                           const char *str,
                                           size_t str_len,
                                           int location_x)
{
  FontBLF *font = blf_get(fontid);
  if (font) {
    return blf_str_offset_from_cursor_position(font, str, str_len, location_x);
  }
  return 0;
}

bool BLF_str_offset_to_glyph_bounds(int fontid,
                                    const char *str,
                                    size_t str_offset,
                                    rcti *glyph_bounds)
{
  FontBLF *font = blf_get(fontid);
  if (font) {
    blf_str_offset_to_glyph_bounds(font, str, str_offset, glyph_bounds);
    return true;
  }
  return false;
}

size_t BLF_width_to_strlen(
    int fontid, const char *str, const size_t str_len, float width, float *r_width)
{
  FontBLF *font = blf_get(fontid);

  if (font) {
    const float xa = (font->flags & BLF_ASPECT) ? font->aspect[0] : 1.0f;
    size_t ret;
    int width_result;
    ret = blf_font_width_to_strlen(font, str, str_len, width / xa, &width_result);
    if (r_width) {
      *r_width = float(width_result) * xa;
    }
    return ret;
  }

  if (r_width) {
    *r_width = 0.0f;
  }
  return 0;
}

size_t BLF_width_to_rstrlen(
    int fontid, const char *str, const size_t str_len, float width, float *r_width)
{
  FontBLF *font = blf_get(fontid);

  if (font) {
    const float xa = (font->flags & BLF_ASPECT) ? font->aspect[0] : 1.0f;
    size_t ret;
    int width_result;
    ret = blf_font_width_to_rstrlen(font, str, str_len, width / xa, &width_result);
    if (r_width) {
      *r_width = float(width_result) * xa;
    }
    return ret;
  }

  if (r_width) {
    *r_width = 0.0f;
  }
  return 0;
}

void BLF_boundbox_ex(
    int fontid, const char *str, const size_t str_len, rcti *r_box, ResultBLF *r_info)
{
  FontBLF *font = blf_get(fontid);

  BLF_RESULT_CHECK_INIT(r_info);

  if (font) {
    if (font->flags & BLF_WORD_WRAP) {
      blf_font_boundbox__wrap(font, str, str_len, r_box, r_info);
    }
    else {
      blf_font_boundbox(font, str, str_len, r_box, r_info);
    }
  }
}

void BLF_boundbox(int fontid, const char *str, const size_t str_len, rcti *r_box)
{
  BLF_boundbox_ex(fontid, str, str_len, r_box, nullptr);
}

void BLF_width_and_height(
    int fontid, const char *str, const size_t str_len, float *r_width, float *r_height)
{
  FontBLF *font = blf_get(fontid);

  if (font) {
    blf_font_width_and_height(font, str, str_len, r_width, r_height, nullptr);
  }
  else {
    *r_width = *r_height = 0.0f;
  }
}

float BLF_width_ex(int fontid, const char *str, const size_t str_len, ResultBLF *r_info)
{
  FontBLF *font = blf_get(fontid);

  BLF_RESULT_CHECK_INIT(r_info);

  if (font) {
    return blf_font_width(font, str, str_len, r_info);
  }

  return 0.0f;
}

float BLF_width(int fontid, const char *str, const size_t str_len)
{
  return BLF_width_ex(fontid, str, str_len, nullptr);
}

float BLF_fixed_width(int fontid)
{
  FontBLF *font = blf_get(fontid);

  if (font) {
    return blf_font_fixed_width(font);
  }

  return 0.0f;
}

float BLF_height_ex(int fontid, const char *str, const size_t str_len, ResultBLF *r_info)
{
  FontBLF *font = blf_get(fontid);

  BLF_RESULT_CHECK_INIT(r_info);

  if (font) {
    return blf_font_height(font, str, str_len, r_info);
  }

  return 0.0f;
}

float BLF_height(int fontid, const char *str, const size_t str_len)
{
  return BLF_height_ex(fontid, str, str_len, nullptr);
}

int BLF_height_max(int fontid)
{
  FontBLF *font = blf_get(fontid);

  if (font) {
    return blf_font_height_max(font);
  }

  return 0;
}

int BLF_width_max(int fontid)
{
  FontBLF *font = blf_get(fontid);

  if (font) {
    return blf_font_width_max(font);
  }

  return 0;
}

int BLF_descender(int fontid)
{
  FontBLF *font = blf_get(fontid);

  if (font) {
    return blf_font_descender(font);
  }

  return 0;
}

int BLF_ascender(int fontid)
{
  FontBLF *font = blf_get(fontid);

  if (font) {
    return blf_font_ascender(font);
  }

  return 0.0f;
}

void BLF_rotation(int fontid, float angle)
{
  FontBLF *font = blf_get(fontid);

  if (font) {
    font->angle = angle;
  }
}

void BLF_clipping(int fontid, int xmin, int ymin, int xmax, int ymax)
{
  FontBLF *font = blf_get(fontid);

  if (font) {
    font->clip_rec.xmin = xmin;
    font->clip_rec.ymin = ymin;
    font->clip_rec.xmax = xmax;
    font->clip_rec.ymax = ymax;
  }
}

void BLF_wordwrap(int fontid, int wrap_width)
{
  FontBLF *font = blf_get(fontid);

  if (font) {
    font->wrap_width = wrap_width;
  }
}

void BLF_shadow(int fontid, int level, const float rgba[4])
{
  FontBLF *font = blf_get(fontid);

  if (font) {
    font->shadow = level;
    rgba_float_to_uchar(font->shadow_color, rgba);
  }
}

void BLF_shadow_offset(int fontid, int x, int y)
{
  FontBLF *font = blf_get(fontid);

  if (font) {
    font->shadow_x = x;
    font->shadow_y = y;
  }
}

void BLF_buffer(
    int fontid, float *fbuf, uchar *cbuf, int w, int h, int nch, ColorManagedDisplay *display)
{
  FontBLF *font = blf_get(fontid);

  if (font) {
    font->buf_info.fbuf = fbuf;
    font->buf_info.cbuf = cbuf;
    font->buf_info.dims[0] = w;
    font->buf_info.dims[1] = h;
    font->buf_info.ch = nch;
    font->buf_info.display = display;
  }
}

void BLF_buffer_col(int fontid, const float rgba[4])
{
  FontBLF *font = blf_get(fontid);

  if (font) {
    copy_v4_v4(font->buf_info.col_init, rgba);
  }
}

void blf_draw_buffer__start(FontBLF *font)
{
  FontBufInfoBLF *buf_info = &font->buf_info;

  rgba_float_to_uchar(buf_info->col_char, buf_info->col_init);

  if (buf_info->display) {
    copy_v4_v4(buf_info->col_float, buf_info->col_init);
    IMB_colormanagement_display_to_scene_linear_v3(buf_info->col_float, buf_info->display);
  }
  else {
    srgb_to_linearrgb_v4(buf_info->col_float, buf_info->col_init);
  }
}
void blf_draw_buffer__end() {}

void BLF_draw_buffer_ex(int fontid, const char *str, const size_t str_len, ResultBLF *r_info)
{
  FontBLF *font = blf_get(fontid);

  if (font && (font->buf_info.fbuf || font->buf_info.cbuf)) {
    blf_draw_buffer__start(font);
    if (font->flags & BLF_WORD_WRAP) {
      blf_font_draw_buffer__wrap(font, str, str_len, r_info);
    }
    else {
      blf_font_draw_buffer(font, str, str_len, r_info);
    }
    blf_draw_buffer__end();
  }
}
void BLF_draw_buffer(int fontid, const char *str, const size_t str_len)
{
  BLF_draw_buffer_ex(fontid, str, str_len, nullptr);
}

char *BLF_display_name_from_file(const char *filepath)
{
  /* While listing font directories this function can be called simultaneously from a greater
   * number of threads than we want the FreeType cache to keep open at a time. Therefore open
   * with own FT_Library object and use FreeType calls directly to avoid any contention. */
  char *name = nullptr;
  FT_Library ft_library;
  if (FT_Init_FreeType(&ft_library) == FT_Err_Ok) {
    FT_Face face;
    if (FT_New_Face(ft_library, filepath, 0, &face) == FT_Err_Ok) {
      if (face->family_name) {
        name = BLI_sprintfN("%s %s", face->family_name, face->style_name);
      }
      FT_Done_Face(face);
    }
    FT_Done_FreeType(ft_library);
  }
  return name;
}

#ifdef DEBUG
void BLF_state_print(int fontid)
{
  FontBLF *font = blf_get(fontid);
  if (font) {
    printf("fontid %d %p\n", fontid, (void *)font);
    printf("  mem_name:    '%s'\n", font->mem_name ? font->mem_name : "<none>");
    printf("  filepath:    '%s'\n", font->filepath ? font->filepath : "<none>");
    printf("  size:     %f\n", font->size);
    printf("  pos:      %d %d %d\n", UNPACK3(font->pos));
    printf("  aspect:   (%d) %.6f %.6f %.6f\n",
           (font->flags & BLF_ROTATION) != 0,
           UNPACK3(font->aspect));
    printf("  angle:    (%d) %.6f\n", (font->flags & BLF_ASPECT) != 0, font->angle);
    printf("  flag:     %d\n", font->flags);
  }
  else {
    printf("fontid %d (nullptr)\n", fontid);
  }
  fflush(stdout);
}
#endif
