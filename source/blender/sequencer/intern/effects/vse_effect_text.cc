/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sequencer
 */

#include <cmath>
#include <mutex>

#include "BKE_lib_id.hh"
#include "BKE_library.hh"
#include "BKE_main.hh"

#include "BLI_map.hh"
#include "BLI_math_base.hh"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_math_vector.hh"
#include "BLI_path_utils.hh"
#include "BLI_rect.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_task.hh"
#include "BLI_vector.hh"

#include "BLF_api.hh"

#include "DNA_packedFile_types.h"
#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"
#include "DNA_space_types.h"
#include "DNA_vfont_types.h"

#include "IMB_imbuf_types.hh"

#include "SEQ_effects.hh"
#include "SEQ_proxy.hh"
#include "SEQ_render.hh"
#include "SEQ_utils.hh"

#include "effects.hh"

namespace blender::seq {

/* -------------------------------------------------------------------- */
/* Sequencer font access.
 *
 * Text strips can access and use fonts from a background thread
 * (when depsgraph evaluation copies the scene, or when prefetch renders
 * frames with text strips in a background thread).
 *
 * To not interfere with what might be happening on the main thread, all
 * fonts used by the sequencer are made unique via #BLF_load_unique
 * #BLF_load_mem_unique, and there's a mutex to guard against
 * sequencer itself possibly using the fonts from several threads.
 */

struct SeqFontMap {
  /* File path -> font ID mapping for file-based fonts. */
  Map<std::string, int> path_to_file_font_id;
  /* Datablock name -> font ID mapping for memory (datablock) fonts. */
  Map<std::string, int> name_to_mem_font_id;

  /* Font access mutex. Recursive since it is locked from
   * text strip rendering, which can call into loading from within. */
  std::recursive_mutex mutex;
};

static SeqFontMap g_font_map;

void fontmap_clear()
{
  for (const auto &item : g_font_map.path_to_file_font_id.items()) {
    BLF_unload_id(item.value);
  }
  g_font_map.path_to_file_font_id.clear();
  for (const auto &item : g_font_map.name_to_mem_font_id.items()) {
    BLF_unload_id(item.value);
  }
  g_font_map.name_to_mem_font_id.clear();
}

static int strip_load_font_file(const std::string &path)
{
  std::lock_guard lock(g_font_map.mutex);
  int fontid = g_font_map.path_to_file_font_id.add_or_modify(
      path,
      [&](int *fontid) {
        /* New path: load font. */
        *fontid = BLF_load_unique(path.c_str());
        return *fontid;
      },
      [&](int *fontid) {
        /* Path already in cache: add reference to already loaded font,
         * or load a new one in case that
         * font id was unloaded behind our backs. */
        if (*fontid >= 0) {
          if (BLF_is_loaded_id(*fontid)) {
            BLF_addref_id(*fontid);
          }
          else {
            *fontid = BLF_load_unique(path.c_str());
          }
        }
        return *fontid;
      });
  return fontid;
}

static int strip_load_font_mem(const std::string &name, const uchar *data, int data_size)
{
  std::lock_guard lock(g_font_map.mutex);
  int fontid = g_font_map.name_to_mem_font_id.add_or_modify(
      name,
      [&](int *fontid) {
        /* New name: load font. */
        *fontid = BLF_load_mem_unique(name.c_str(), data, data_size);
        return *fontid;
      },
      [&](int *fontid) {
        /* Name already in cache: add reference to already loaded font,
         * or (if we're on the main thread) load a new one in case that
         * font id was unloaded behind our backs. */
        if (*fontid >= 0) {
          if (BLF_is_loaded_id(*fontid)) {
            BLF_addref_id(*fontid);
          }
          else {
            *fontid = BLF_load_mem_unique(name.c_str(), data, data_size);
          }
        }
        return *fontid;
      });
  return fontid;
}

static void strip_unload_font(int fontid)
{
  std::lock_guard lock(g_font_map.mutex);
  bool unloaded = BLF_unload_id(fontid);
  /* If that was the last usage of the font and it got unloaded: remove
   * it from our maps. */
  if (unloaded) {
    g_font_map.path_to_file_font_id.remove_if([&](auto item) { return item.value == fontid; });
    g_font_map.name_to_mem_font_id.remove_if([&](auto item) { return item.value == fontid; });
  }
}

/* -------------------------------------------------------------------- */
/** \name Text Effect
 * \{ */

bool effects_can_render_text(const Strip *strip)
{
  /* `data->text[0] == 0` is ignored on purpose in order to make it possible to edit. */

  TextVars *data = static_cast<TextVars *>(strip->effectdata);
  if (data->text_size < 1.0f ||
      ((data->color[3] == 0.0f) &&
       (data->shadow_color[3] == 0.0f || (data->flag & SEQ_TEXT_SHADOW) == 0) &&
       (data->outline_color[3] == 0.0f || data->outline_width <= 0.0f ||
        (data->flag & SEQ_TEXT_OUTLINE) == 0)))
  {
    return false;
  }
  return true;
}

static void init_text_effect(Strip *strip)
{
  MEM_SAFE_FREE(strip->effectdata);
  TextVars *data = MEM_callocN<TextVars>("textvars");
  strip->effectdata = data;

  data->text_font = nullptr;
  data->text_blf_id = -1;
  data->text_size = 60.0f;

  copy_v4_fl(data->color, 1.0f);
  data->shadow_color[3] = 0.7f;
  data->shadow_angle = DEG2RADF(65.0f);
  data->shadow_offset = 0.04f;
  data->shadow_blur = 0.0f;
  data->box_color[0] = 0.2f;
  data->box_color[1] = 0.2f;
  data->box_color[2] = 0.2f;
  data->box_color[3] = 0.7f;
  data->box_margin = 0.01f;
  data->box_roundness = 0.0f;
  data->outline_color[3] = 0.7f;
  data->outline_width = 0.05f;

  data->text_ptr = BLI_strdup("Text");
  data->text_len_bytes = strlen(data->text_ptr);

  data->loc[0] = 0.5f;
  data->loc[1] = 0.5f;
  data->anchor_x = SEQ_TEXT_ALIGN_X_CENTER;
  data->anchor_y = SEQ_TEXT_ALIGN_Y_CENTER;
  data->align = SEQ_TEXT_ALIGN_X_CENTER;
  data->wrap_width = 1.0f;
}

static void text_font_unload(TextVars *data, const bool do_id_user)
{
  if (data == nullptr) {
    return;
  }

  /* Unlink the VFont */
  if (do_id_user && data->text_font != nullptr) {
    id_us_min(&data->text_font->id);
    data->text_font = nullptr;
  }

  /* Unload the font. */
  if (data->text_blf_id >= 0) {
    strip_unload_font(data->text_blf_id);
    data->text_blf_id = -1;
  }
}

void effect_text_font_set(Strip *strip, VFont *font)
{
  if (strip == nullptr || strip->type != STRIP_TYPE_TEXT) {
    return;
  }
  TextVars *data = static_cast<TextVars *>(strip->effectdata);
  text_font_unload(data, true);

  id_us_plus(&font->id);
  data->text_blf_id = STRIP_FONT_NOT_LOADED;
  data->text_font = font;
}

static void text_font_load(TextVars *data, const bool do_id_user)
{
  VFont *vfont = data->text_font;
  if (vfont == nullptr) {
    return;
  }

  if (do_id_user) {
    id_us_plus(&vfont->id);
  }

  if (vfont->packedfile != nullptr) {
    PackedFile *pf = vfont->packedfile;
    /* Create a name that's unique between library data-blocks to avoid loading
     * a font per strip which will load fonts many times.
     *
     * WARNING: this isn't fool proof!
     * The #VFont may be renamed which will cause this to load multiple times,
     * in practice this isn't so likely though. */
    char name[MAX_ID_FULL_NAME];
    BKE_id_full_name_get(name, &vfont->id, 0);

    data->text_blf_id = strip_load_font_mem(name, static_cast<const uchar *>(pf->data), pf->size);
  }
  else {
    char filepath[FILE_MAX];
    STRNCPY(filepath, vfont->filepath);

    BLI_path_abs(filepath, ID_BLEND_PATH_FROM_GLOBAL(&vfont->id));
    data->text_blf_id = strip_load_font_file(filepath);
  }
}

static void free_text_effect(Strip *strip, const bool do_id_user)
{
  TextVars *data = static_cast<TextVars *>(strip->effectdata);
  text_font_unload(data, do_id_user);

  if (data) {
    MEM_SAFE_FREE(data->text_ptr);
    MEM_delete(data->runtime);
    MEM_freeN(data);
    strip->effectdata = nullptr;
  }
}

static void load_text_effect(Strip *strip)
{
  TextVars *data = static_cast<TextVars *>(strip->effectdata);
  text_font_load(data, false);
}

static void copy_text_effect(Strip *dst, const Strip *src, const int flag)
{
  dst->effectdata = MEM_dupallocN(src->effectdata);
  TextVars *data = static_cast<TextVars *>(dst->effectdata);
  data->text_ptr = BLI_strdup_null(data->text_ptr);

  data->runtime = nullptr;
  data->text_blf_id = -1;
  text_font_load(data, (flag & LIB_ID_CREATE_NO_USER_REFCOUNT) == 0);
}

static int num_inputs_text()
{
  return 0;
}

static StripEarlyOut early_out_text(const Strip *strip, float /*fac*/)
{
  if (!effects_can_render_text(strip)) {
    return StripEarlyOut::UseInput1;
  }
  return StripEarlyOut::NoInput;
}

/* Simplified version of gaussian blur specifically for text shadow blurring:
 * - Data is only the alpha channel,
 * - Skips blur outside of shadow rectangle. */
static void text_gaussian_blur_x(const Span<float> gaussian,
                                 int half_size,
                                 int start_line,
                                 int width,
                                 int height,
                                 const uchar *rect,
                                 uchar *dst,
                                 const rcti &shadow_rect)
{
  dst += int64_t(start_line) * width;
  for (int y = start_line; y < start_line + height; y++) {
    for (int x = 0; x < width; x++) {
      float accum(0.0f);
      if (x >= shadow_rect.xmin && x <= shadow_rect.xmax) {
        float accum_weight = 0.0f;
        int xmin = math::max(x - half_size, shadow_rect.xmin);
        int xmax = math::min(x + half_size, shadow_rect.xmax);
        for (int nx = xmin, index = (xmin - x) + half_size; nx <= xmax; nx++, index++) {
          float weight = gaussian[index];
          int offset = y * width + nx;
          accum += rect[offset] * weight;
          accum_weight += weight;
        }
        accum *= (1.0f / accum_weight);
      }

      *dst = accum;
      dst++;
    }
  }
}

static void text_gaussian_blur_y(const Span<float> gaussian,
                                 int half_size,
                                 int start_line,
                                 int width,
                                 int height,
                                 const uchar *rect,
                                 uchar *dst,
                                 const rcti &shadow_rect)
{
  dst += int64_t(start_line) * width;
  for (int y = start_line; y < start_line + height; y++) {
    for (int x = 0; x < width; x++) {
      float accum(0.0f);
      if (x >= shadow_rect.xmin && x <= shadow_rect.xmax) {
        float accum_weight = 0.0f;
        int ymin = math::max(y - half_size, shadow_rect.ymin);
        int ymax = math::min(y + half_size, shadow_rect.ymax);
        for (int ny = ymin, index = (ymin - y) + half_size; ny <= ymax; ny++, index++) {
          float weight = gaussian[index];
          int offset = ny * width + x;
          accum += rect[offset] * weight;
          accum_weight += weight;
        }
        accum *= (1.0f / accum_weight);
      }
      *dst = accum;
      dst++;
    }
  }
}

static void clamp_rect(int width, int height, rcti &r_rect)
{
  r_rect.xmin = math::clamp(r_rect.xmin, 0, width - 1);
  r_rect.xmax = math::clamp(r_rect.xmax, 0, width - 1);
  r_rect.ymin = math::clamp(r_rect.ymin, 0, height - 1);
  r_rect.ymax = math::clamp(r_rect.ymax, 0, height - 1);
}

static void initialize_shadow_alpha(int width,
                                    int height,
                                    int2 offset,
                                    const rcti &shadow_rect,
                                    const uchar *input,
                                    Array<uchar> &r_shadow_mask)
{
  const IndexRange shadow_y_range(shadow_rect.ymin, shadow_rect.ymax - shadow_rect.ymin + 1);
  threading::parallel_for(shadow_y_range, 8, [&](const IndexRange y_range) {
    for (const int64_t y : y_range) {
      const int64_t src_y = math::clamp<int64_t>(y + offset.y, 0, height - 1);
      for (int x = shadow_rect.xmin; x <= shadow_rect.xmax; x++) {
        int src_x = math::clamp(x - offset.x, 0, width - 1);
        size_t src_offset = width * src_y + src_x;
        size_t dst_offset = width * y + x;
        r_shadow_mask[dst_offset] = input[src_offset * 4 + 3];
      }
    }
  });
}

static void composite_shadow(int width,
                             const rcti &shadow_rect,
                             const float4 &shadow_color,
                             const Array<uchar> &shadow_mask,
                             uchar *output)
{
  const IndexRange shadow_y_range(shadow_rect.ymin, shadow_rect.ymax - shadow_rect.ymin + 1);
  threading::parallel_for(shadow_y_range, 8, [&](const IndexRange y_range) {
    for (const int64_t y : y_range) {
      size_t offset = y * width + shadow_rect.xmin;
      uchar *dst = output + offset * 4;
      for (int x = shadow_rect.xmin; x <= shadow_rect.xmax; x++, offset++, dst += 4) {
        uchar a = shadow_mask[offset];
        if (a == 0) {
          /* Fully transparent, leave output pixel as is. */
          continue;
        }
        float4 col1 = load_premul_pixel(dst);
        float4 col2 = shadow_color * (a * (1.0f / 255.0f));
        /* Blend under the output. */
        float fac = 1.0f - col1.w;
        float4 col = col1 + fac * col2;
        store_premul_pixel(col, dst);
      }
    }
  });
}

static void draw_text_shadow(
    const RenderData *context, const TextVars *data, int line_height, const rcti &rect, ImBuf *out)
{
  const int width = context->rectx;
  const int height = context->recty;
  /* Blur value of 1.0 applies blur kernel that is half of text line height. */
  const float blur_amount = line_height * 0.5f * data->shadow_blur;
  bool do_blur = blur_amount >= 1.0f;

  Array<uchar> shadow_mask(size_t(width) * height, 0);

  const int2 offset = int2(cosf(data->shadow_angle) * line_height * data->shadow_offset,
                           sinf(data->shadow_angle) * line_height * data->shadow_offset);

  rcti shadow_rect = rect;
  BLI_rcti_translate(&shadow_rect, offset.x, -offset.y);
  BLI_rcti_pad(&shadow_rect, 1, 1);
  clamp_rect(width, height, shadow_rect);

  /* Initialize shadow by copying existing text/outline alpha. */
  initialize_shadow_alpha(width, height, offset, shadow_rect, out->byte_buffer.data, shadow_mask);

  if (do_blur) {
    /* Create blur kernel weights. */
    const int half_size = int(blur_amount + 0.5f);
    Array<float> gaussian = make_gaussian_blur_kernel(blur_amount, half_size);

    BLI_rcti_pad(&shadow_rect, half_size + 1, half_size + 1);
    clamp_rect(width, height, shadow_rect);

    /* Horizontal blur: blur shadow_mask into blur_buffer. */
    Array<uchar> blur_buffer(size_t(width) * height, NoInitialization());
    IndexRange blur_y_range(shadow_rect.ymin, shadow_rect.ymax - shadow_rect.ymin + 1);
    threading::parallel_for(blur_y_range, 8, [&](const IndexRange y_range) {
      const int y_first = y_range.first();
      const int y_size = y_range.size();
      text_gaussian_blur_x(gaussian,
                           half_size,
                           y_first,
                           width,
                           y_size,
                           shadow_mask.data(),
                           blur_buffer.data(),
                           shadow_rect);
    });

    /* Vertical blur: blur blur_buffer into shadow_mask. */
    threading::parallel_for(blur_y_range, 8, [&](const IndexRange y_range) {
      const int y_first = y_range.first();
      const int y_size = y_range.size();
      text_gaussian_blur_y(gaussian,
                           half_size,
                           y_first,
                           width,
                           y_size,
                           blur_buffer.data(),
                           shadow_mask.data(),
                           shadow_rect);
    });
  }

  /* Composite shadow under regular output. */
  float4 color = data->shadow_color;
  color.x *= color.w;
  color.y *= color.w;
  color.z *= color.w;
  composite_shadow(width, shadow_rect, color, shadow_mask, out->byte_buffer.data);
}

/* Text outline calculation is done by Jump Flooding Algorithm (JFA).
 * This is similar to inpaint/jump_flooding in Compositor, also to
 * "The Quest for Very Wide Outlines", Ben Golus 2020
 * https://bgolus.medium.com/the-quest-for-very-wide-outlines-ba82ed442cd9 */

constexpr uint16_t JFA_INVALID = 0xFFFF;

struct JFACoord {
  uint16_t x;
  uint16_t y;
};

static void jump_flooding_pass(Span<JFACoord> input,
                               MutableSpan<JFACoord> output,
                               int2 size,
                               IndexRange x_range,
                               IndexRange y_range,
                               int step_size)
{
  threading::parallel_for(y_range, 8, [&](const IndexRange sub_y_range) {
    for (const int64_t y : sub_y_range) {
      size_t index = y * size.x;
      for (const int64_t x : x_range) {
        float2 coord = float2(x, y);

        /* For each pixel, sample 9 pixels at +/- step size pattern,
         * and output coordinate of closest to the boundary. */
        JFACoord closest_texel{JFA_INVALID, JFA_INVALID};
        float minimum_squared_distance = std::numeric_limits<float>::max();
        for (int dy = -step_size; dy <= step_size; dy += step_size) {
          int yy = y + dy;
          if (yy < 0 || yy >= size.y) {
            continue;
          }
          for (int dx = -step_size; dx <= step_size; dx += step_size) {
            int xx = x + dx;
            if (xx < 0 || xx >= size.x) {
              continue;
            }
            JFACoord val = input[size_t(yy) * size.x + xx];
            if (val.x == JFA_INVALID) {
              continue;
            }

            float squared_distance = math::distance_squared(float2(val.x, val.y), coord);
            if (squared_distance < minimum_squared_distance) {
              minimum_squared_distance = squared_distance;
              closest_texel = val;
            }
          }
        }

        output[index + x] = closest_texel;
      }
    }
  });
}

static void text_draw(const char *text_ptr, const TextVarsRuntime *runtime, float color[4])
{
  const bool use_fallback = BLF_is_builtin(runtime->font);
  if (!use_fallback) {
    BLF_enable(runtime->font, BLF_NO_FALLBACK);
  }

  for (const LineInfo &line : runtime->lines) {
    for (const CharInfo &character : line.characters) {
      BLF_position(runtime->font, character.position.x, character.position.y, 0.0f);
      BLF_buffer_col(runtime->font, color);
      BLF_draw_buffer(runtime->font, text_ptr + character.offset, character.byte_length);
    }
  }

  if (!use_fallback) {
    BLF_disable(runtime->font, BLF_NO_FALLBACK);
  }
}

static rcti draw_text_outline(const RenderData *context,
                              const TextVars *data,
                              const TextVarsRuntime *runtime,
                              ImBuf *out)
{
  /* Outline width of 1.0 maps to half of text line height. */
  const int outline_width = int(runtime->line_height * 0.5f * data->outline_width);
  if (outline_width < 1 || data->outline_color[3] <= 0.0f ||
      ((data->flag & SEQ_TEXT_OUTLINE) == 0))
  {
    return runtime->text_boundbox;
  }

  const int2 size = int2(context->rectx, context->recty);

  /* Draw white text into temporary buffer. */
  const size_t pixel_count = size_t(size.x) * size.y;
  Array<uchar4> tmp_buf(pixel_count, uchar4(0));
  BLF_buffer(runtime->font,
             nullptr,
             (uchar *)tmp_buf.data(),
             size.x,
             size.y,
             out->byte_buffer.colorspace);

  text_draw(data->text_ptr, runtime, float4(1.0f));

  rcti outline_rect = runtime->text_boundbox;
  BLI_rcti_pad(&outline_rect, outline_width + 1, outline_width + 1);
  outline_rect.xmin = clamp_i(outline_rect.xmin, 0, size.x - 1);
  outline_rect.xmax = clamp_i(outline_rect.xmax, 0, size.x - 1);
  outline_rect.ymin = clamp_i(outline_rect.ymin, 0, size.y - 1);
  outline_rect.ymax = clamp_i(outline_rect.ymax, 0, size.y - 1);
  const IndexRange rect_x_range(outline_rect.xmin, outline_rect.xmax - outline_rect.xmin + 1);
  const IndexRange rect_y_range(outline_rect.ymin, outline_rect.ymax - outline_rect.ymin + 1);

  /* Initialize JFA: invalid values for empty regions, pixel coordinates
   * for opaque regions. */
  Array<JFACoord> boundary(pixel_count, NoInitialization());
  threading::parallel_for(IndexRange(size.y), 16, [&](const IndexRange y_range) {
    for (const int y : y_range) {
      size_t index = size_t(y) * size.x;
      for (int x = 0; x < size.x; x++, index++) {
        bool is_opaque = tmp_buf[index].w >= 128;
        JFACoord coord;
        coord.x = is_opaque ? x : JFA_INVALID;
        coord.y = is_opaque ? y : JFA_INVALID;
        boundary[index] = coord;
      }
    }
  });

  /* Do jump flooding calculations. */
  JFACoord invalid_coord{JFA_INVALID, JFA_INVALID};
  Array<JFACoord> initial_flooded_result(pixel_count, invalid_coord);
  jump_flooding_pass(boundary, initial_flooded_result, size, rect_x_range, rect_y_range, 1);

  Array<JFACoord> *result_to_flood = &initial_flooded_result;
  Array<JFACoord> intermediate_result(pixel_count, invalid_coord);
  Array<JFACoord> *result_after_flooding = &intermediate_result;

  int step_size = power_of_2_max_i(outline_width) / 2;

  while (step_size != 0) {
    jump_flooding_pass(
        *result_to_flood, *result_after_flooding, size, rect_x_range, rect_y_range, step_size);
    std::swap(result_to_flood, result_after_flooding);
    step_size /= 2;
  }

  /* Premultiplied outline color. */
  float4 color = data->outline_color;
  color.x *= color.w;
  color.y *= color.w;
  color.z *= color.w;

  const float text_color_alpha = data->color[3];

  /* We have distances to the closest opaque parts of the image now. Composite the
   * outline into the output image. */

  threading::parallel_for(rect_y_range, 8, [&](const IndexRange y_range) {
    for (const int y : y_range) {
      size_t index = size_t(y) * size.x + rect_x_range.start();
      uchar *dst = out->byte_buffer.data + index * 4;
      for (int x = rect_x_range.start(); x < rect_x_range.one_after_last(); x++, index++, dst += 4)
      {
        JFACoord closest_texel = (*result_to_flood)[index];
        if (closest_texel.x == JFA_INVALID) {
          /* Outside of outline, leave output pixel as is. */
          continue;
        }

        /* Fade out / anti-alias the outline over one pixel towards outline distance. */
        float distance = math::distance(float2(x, y), float2(closest_texel.x, closest_texel.y));
        float alpha = math::clamp(outline_width - distance + 1.0f, 0.0f, 1.0f);

        /* Do not put outline inside the text shape:
         * - When overall text color is fully opaque, we want to make
         *   outline fully transparent only where text is fully opaque.
         *   This ensures that combined anti-aliased pixels at text boundary
         *   are properly fully opaque.
         * - However when text color is fully transparent, we want to
         *   Use opposite alpha of text, to anti-alias the inner edge of
         *   the outline.
         * In between those two, interpolate the alpha modulation factor. */
        float text_alpha = tmp_buf[index].w * (1.0f / 255.0f);
        float mul_opaque_text = text_alpha >= 1.0f ? 0.0f : 1.0f;
        float mul_transparent_text = 1.0f - text_alpha;
        float mul = math::interpolate(mul_transparent_text, mul_opaque_text, text_color_alpha);
        alpha *= mul;

        float4 col1 = color;
        col1 *= alpha;

        /* Blend over the output. */
        float mfac = 1.0f - col1.w;
        float4 col2 = load_premul_pixel(dst);
        float4 col = col1 + mfac * col2;
        store_premul_pixel(col, dst);
      }
    }
  });
  BLF_buffer(
      runtime->font, nullptr, out->byte_buffer.data, size.x, size.y, out->byte_buffer.colorspace);

  return outline_rect;
}

/* Similar to #IMB_rectfill_area but blends the given color under the
 * existing image. Also can do rounded corners. Only works on byte buffers. */
static void fill_rect_alpha_under(
    const ImBuf *ibuf, const float col[4], int x1, int y1, int x2, int y2, float corner_radius)
{
  const int width = ibuf->x;
  const int height = ibuf->y;
  x1 = math::clamp(x1, 0, width);
  x2 = math::clamp(x2, 0, width);
  y1 = math::clamp(y1, 0, height);
  y2 = math::clamp(y2, 0, height);
  if (x1 > x2) {
    std::swap(x1, x2);
  }
  if (y1 > y2) {
    std::swap(y1, y2);
  }
  if (x1 == x2 || y1 == y2) {
    return;
  }

  corner_radius = math::clamp(corner_radius, 0.0f, math::min(x2 - x1, y2 - y1) / 2.0f);

  float4 premul_col_base;
  straight_to_premul_v4_v4(premul_col_base, col);

  threading::parallel_for(IndexRange::from_begin_end(y1, y2), 16, [&](const IndexRange y_range) {
    for (const int y : y_range) {
      uchar *dst = ibuf->byte_buffer.data + (size_t(width) * y + x1) * 4;
      float origin_x = 0.0f, origin_y = 0.0f;
      for (int x = x1; x < x2; x++) {
        float4 pix = load_premul_pixel(dst);
        float fac = 1.0f - pix.w;

        float4 premul_col = premul_col_base;
        bool is_corner = false;
        if (x < x1 + corner_radius && y < y1 + corner_radius) {
          is_corner = true;
          origin_x = x1 + corner_radius - 1;
          origin_y = y1 + corner_radius - 1;
        }
        else if (x >= x2 - corner_radius && y < y1 + corner_radius) {
          is_corner = true;
          origin_x = x2 - corner_radius;
          origin_y = y1 + corner_radius - 1;
        }
        else if (x < x1 + corner_radius && y >= y2 - corner_radius) {
          is_corner = true;
          origin_x = x1 + corner_radius - 1;
          origin_y = y2 - corner_radius;
        }
        else if (x >= x2 - corner_radius && y >= y2 - corner_radius) {
          is_corner = true;
          origin_x = x2 - corner_radius;
          origin_y = y2 - corner_radius;
        }
        if (is_corner) {
          /* If we are inside rounded corner, evaluate a superellipse and
           * modulate color with that. Superellipse instead of just a circle
           * since the curvature between flat and rounded area looks a bit
           * nicer. */
          constexpr float curve_pow = 2.1f;
          float r = powf(powf(abs(x - origin_x), curve_pow) + powf(abs(y - origin_y), curve_pow),
                         1.0f / curve_pow);
          float alpha = math::clamp(corner_radius - r, 0.0f, 1.0f);
          premul_col *= alpha;
        }

        float4 dst_fl = fac * premul_col + pix;
        store_premul_pixel(dst_fl, dst);
        dst += 4;
      }
    }
  });
}

static int text_effect_line_size_get(const RenderData *context, const Strip *strip)
{
  TextVars *data = static_cast<TextVars *>(strip->effectdata);

  /* Used to calculate boundbox. Render scale compensation is not needed there. */
  if (context == nullptr) {
    return data->text_size;
  }

  /* Compensate for preview render size. */
  const float size_scale = seq::get_render_scale_factor(*context);
  return size_scale * data->text_size;
}

int text_effect_font_init(const RenderData *context, const Strip *strip, FontFlags font_flags)
{
  TextVars *data = static_cast<TextVars *>(strip->effectdata);
  int font = blf_mono_font_render;

  /* In case font got unloaded behind our backs: mark it as needing a load. */
  if (data->text_blf_id >= 0 && !BLF_is_loaded_id(data->text_blf_id)) {
    data->text_blf_id = STRIP_FONT_NOT_LOADED;
  }

  if (data->text_blf_id == STRIP_FONT_NOT_LOADED) {
    data->text_blf_id = -1;

    text_font_load(data, false);
  }

  if (data->text_blf_id >= 0) {
    font = data->text_blf_id;
  }

  BLF_size(font, text_effect_line_size_get(context, strip));
  BLF_enable(font, font_flags);
  return font;
}

static Vector<CharInfo> build_character_info(const TextVars *data, int font)
{
  Vector<CharInfo> characters;
  const int len_max = data->text_len_bytes;
  int byte_offset = 0;
  int char_index = 0;

  const bool use_fallback = BLF_is_builtin(font);
  if (!use_fallback) {
    BLF_enable(font, BLF_NO_FALLBACK);
  }

  while (byte_offset <= len_max) {
    const char *str = data->text_ptr + byte_offset;
    const int char_length = BLI_str_utf8_size_safe(str);

    CharInfo char_info;
    char_info.index = char_index;
    char_info.offset = byte_offset;
    char_info.byte_length = char_length;
    char_info.advance_x = BLF_glyph_advance(font, str);
    characters.append(char_info);

    byte_offset += char_length;
    char_index++;
  }

  if (!use_fallback) {
    BLF_disable(font, BLF_NO_FALLBACK);
  }

  return characters;
}

static int wrap_width_get(const TextVars *data, const int2 image_size)
{
  if (data->wrap_width == 0.0f) {
    return std::numeric_limits<int>::max();
  }
  return data->wrap_width * image_size.x;
}

/* Lines must contain CharInfo for newlines and \0, as UI must know where they begin. */
static void apply_word_wrapping(const TextVars *data,
                                TextVarsRuntime *runtime,
                                const int2 image_size,
                                Vector<CharInfo> &characters)
{
  const int wrap_width = wrap_width_get(data, image_size);

  float2 char_position{0.0f, 0.0f};
  CharInfo *last_space = nullptr;

  /* First pass: Find characters where line has to be broken. */
  for (CharInfo &character : characters) {
    char ch = data->text_ptr[character.offset];
    if (ch == ' ') {
      character.position = char_position;
      last_space = &character;
    }
    if (ch == '\n') {
      char_position.x = 0;
      last_space = nullptr;
    }
    if (ch != '\0' && char_position.x > wrap_width && last_space != nullptr) {
      last_space->do_wrap = true;
      char_position -= last_space->position + last_space->advance_x;
    }
    char_position.x += character.advance_x;
  }

  /* Second pass: Fill lines with characters. */
  char_position = {0.0f, 0.0f};
  runtime->lines.append(LineInfo());
  for (CharInfo &character : characters) {
    character.position = char_position;
    runtime->lines.last().characters.append(character);
    runtime->lines.last().width = char_position.x;

    char_position.x += character.advance_x;

    if (character.do_wrap || data->text_ptr[character.offset] == '\n') {
      runtime->lines.append(LineInfo());
      char_position.x = 0;
      char_position.y -= runtime->line_height;
    }
  }
}

static int text_box_width_get(const Vector<LineInfo> &lines)
{
  int width_max = 0;

  for (const LineInfo &line : lines) {
    width_max = std::max(width_max, line.width);
  }
  return width_max;
}

static float2 horizontal_alignment_offset_get(const TextVars *data,
                                              float line_width,
                                              int width_max)
{
  const float line_offset = (width_max - line_width);

  if (data->align == SEQ_TEXT_ALIGN_X_RIGHT) {
    return {line_offset, 0.0f};
  }
  if (data->align == SEQ_TEXT_ALIGN_X_CENTER) {
    return {line_offset / 2.0f, 0.0f};
  }

  return {0.0f, 0.0f};
}

static float2 anchor_offset_get(const TextVars *data, int width_max, int text_height)
{
  float2 anchor_offset;

  switch (data->anchor_x) {
    case SEQ_TEXT_ALIGN_X_LEFT:
      anchor_offset.x = 0;
      break;
    case SEQ_TEXT_ALIGN_X_CENTER:
      anchor_offset.x = -width_max / 2.0f;
      break;
    case SEQ_TEXT_ALIGN_X_RIGHT:
      anchor_offset.x = -width_max;
      break;
  }
  switch (data->anchor_y) {
    case SEQ_TEXT_ALIGN_Y_TOP:
      anchor_offset.y = 0;
      break;
    case SEQ_TEXT_ALIGN_Y_CENTER:
      anchor_offset.y = text_height / 2.0f;
      break;
    case SEQ_TEXT_ALIGN_Y_BOTTOM:
      anchor_offset.y = text_height;
      break;
  }

  return anchor_offset;
}

static void calc_boundbox(const TextVars *data, TextVarsRuntime *runtime, const int2 image_size)
{
  const int text_height = runtime->lines.size() * runtime->line_height;

  int width_max = text_box_width_get(runtime->lines);

  /* Add width to empty text, so there is something to draw or select. */
  if (width_max == 0) {
    width_max = text_height * 2;
  }

  const float2 image_center{data->loc[0] * image_size.x, data->loc[1] * image_size.y};
  const float2 anchor = anchor_offset_get(data, width_max, text_height);

  runtime->text_boundbox.xmin = anchor.x + image_center.x;
  runtime->text_boundbox.xmax = anchor.x + image_center.x + width_max;
  runtime->text_boundbox.ymin = anchor.y + image_center.y - text_height;
  runtime->text_boundbox.ymax = runtime->text_boundbox.ymin + text_height;
}

static void apply_text_alignment(const TextVars *data,
                                 TextVarsRuntime *runtime,
                                 const int2 image_size)
{
  const int width_max = text_box_width_get(runtime->lines);
  const int text_height = runtime->lines.size() * runtime->line_height;

  const float2 image_center{data->loc[0] * image_size.x, data->loc[1] * image_size.y};
  const float2 line_height_offset{0.0f,
                                  float(-runtime->line_height - BLF_descender(runtime->font))};
  const float2 anchor = anchor_offset_get(data, width_max, text_height);

  for (LineInfo &line : runtime->lines) {
    const float2 alignment_x = horizontal_alignment_offset_get(data, line.width, width_max);
    const float2 alignment = math::round(image_center + line_height_offset + alignment_x + anchor);

    for (CharInfo &character : line.characters) {
      character.position += alignment;
    }
  }
}

TextVarsRuntime *text_effect_calc_runtime(const Strip *strip, int font, const int2 image_size)
{
  TextVars *data = static_cast<TextVars *>(strip->effectdata);
  TextVarsRuntime *runtime = MEM_new<TextVarsRuntime>(__func__);

  runtime->font = font;
  runtime->line_height = BLF_height_max(font);
  runtime->font_descender = BLF_descender(font);
  runtime->character_count = BLI_strlen_utf8(data->text_ptr);

  Vector<CharInfo> characters_temp = build_character_info(data, font);
  apply_word_wrapping(data, runtime, image_size, characters_temp);
  apply_text_alignment(data, runtime, image_size);
  calc_boundbox(data, runtime, image_size);
  return runtime;
}

static ImBuf *do_text_effect(const RenderData *context,
                             SeqRenderState * /*state*/,
                             Strip *strip,
                             float /*timeline_frame*/,
                             float /*fac*/,
                             ImBuf * /*ibuf1*/,
                             ImBuf * /*ibuf2*/)
{
  /* NOTE: text rasterization only fills in part of output image,
   * need to clear it. */
  ImBuf *out = prepare_effect_imbufs(context, nullptr, nullptr, false);
  TextVars *data = static_cast<TextVars *>(strip->effectdata);

  const FontFlags font_flags = ((data->flag & SEQ_TEXT_BOLD) ? BLF_BOLD : BLF_NONE) |
                               ((data->flag & SEQ_TEXT_ITALIC) ? BLF_ITALIC : BLF_NONE);

  /* Guard against parallel accesses to the fonts map. */
  std::lock_guard lock(g_font_map.mutex);

  const int font = text_effect_font_init(context, strip, font_flags);

  if (data->runtime != nullptr) {
    MEM_delete(data->runtime);
  }

  TextVarsRuntime *runtime = text_effect_calc_runtime(strip, font, {out->x, out->y});
  data->runtime = runtime;

  rcti outline_rect = draw_text_outline(context, data, runtime, out);
  BLF_buffer(font, nullptr, out->byte_buffer.data, out->x, out->y, out->byte_buffer.colorspace);
  text_draw(data->text_ptr, runtime, data->color);
  BLF_buffer(font, nullptr, nullptr, 0, 0, nullptr);
  BLF_disable(font, font_flags);

  /* Draw shadow. */
  if (data->flag & SEQ_TEXT_SHADOW) {
    draw_text_shadow(context, data, runtime->line_height, outline_rect, out);
  }

  /* Draw box under text. */
  if (data->flag & SEQ_TEXT_BOX) {
    if (out->byte_buffer.data) {
      const int margin = data->box_margin * out->x;
      const int minx = runtime->text_boundbox.xmin - margin;
      const int maxx = runtime->text_boundbox.xmax + margin;
      const int miny = runtime->text_boundbox.ymin - margin;
      const int maxy = runtime->text_boundbox.ymax + margin;
      float corner_radius = data->box_roundness * (maxy - miny) / 2.0f;
      fill_rect_alpha_under(out, data->box_color, minx, miny, maxx, maxy, corner_radius);
    }
  }

  return out;
}

void text_effect_get_handle(EffectHandle &rval)
{
  rval.num_inputs = num_inputs_text;
  rval.init = init_text_effect;
  rval.free = free_text_effect;
  rval.load = load_text_effect;
  rval.copy = copy_text_effect;
  rval.early_out = early_out_text;
  rval.execute = do_text_effect;
}

/** \} */

}  // namespace blender::seq
