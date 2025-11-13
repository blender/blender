/* SPDX-FileCopyrightText: 2021-2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spseq
 */

#include "BKE_context.hh"

#include "BLI_array.hh"

#include "IMB_imbuf.hh"

#include "DNA_sequence_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"

#include "GPU_batch.hh"
#include "GPU_batch_presets.hh"
#include "GPU_matrix.hh"
#include "GPU_shader_shared.hh"
#include "GPU_texture.hh"
#include "GPU_uniform_buffer.hh"

#include "IMB_colormanagement.hh"

#include "SEQ_render.hh"
#include "SEQ_thumbnail_cache.hh"

#include "WM_api.hh"

#include "sequencer_intern.hh"
#include "sequencer_strips_batch.hh"

namespace blender::ed::vse {

/* Information for one thumbnail picture in the timeline. Note that a single
 * strip could have multiple thumbnails. */
struct SeqThumbInfo {
  ImBuf *ibuf;
  /* Strip coordinates in timeline space (X: frames, Y: channels). */
  float left_handle, right_handle, bottom, top;
  /* Thumbnail coordinates in timeline space. */
  float x1, x2, y1, y2;
  /* Horizontal cropping of thumbnail image, in pixels. Often a thumbnail
   * does not have to be cropped, in which case these are 0 and ibuf->x-1. */
  float cropx_min, cropx_max;
  bool is_muted;
};

static float thumb_calc_first_timeline_frame(const Strip *strip,
                                             float left_handle,
                                             float frame_step,
                                             const rctf *view_area)
{
  int first_drawable_frame = max_iii(left_handle, strip->start, view_area->xmin);

  /* First frame should correspond to handle position. */
  if (first_drawable_frame == left_handle) {
    return left_handle;
  }

  float aligned_frame_offset = int((first_drawable_frame - strip->start) / frame_step) *
                               frame_step;
  return strip->start + aligned_frame_offset;
}

static float thumb_calc_next_timeline_frame(const Strip *strip,
                                            float left_handle,
                                            float last_frame,
                                            float frame_step)
{
  float next_frame = last_frame + frame_step;

  /* If handle position was displayed, align next frame with `strip->start`. */
  if (last_frame == left_handle) {
    next_frame = strip->start + (int((last_frame - strip->start) / frame_step) + 1) * frame_step;
  }

  return next_frame;
}

static void strip_get_thumb_image_dimensions(const Strip *strip,
                                             float pixelx,
                                             float pixely,
                                             float *r_thumb_width,
                                             float thumb_height,
                                             float *r_image_width,
                                             float *r_image_height)
{
  float image_width = strip->data->stripdata->orig_width;
  float image_height = strip->data->stripdata->orig_height;

  /* Fix the dimensions to be max SEQ_THUMB_SIZE for x or y. */
  float aspect_ratio = image_width / image_height;
  if (image_width > image_height) {
    image_width = seq::THUMB_SIZE;
    image_height = round_fl_to_int(image_width / aspect_ratio);
  }
  else {
    image_height = seq::THUMB_SIZE;
    image_width = round_fl_to_int(image_height * aspect_ratio);
  }

  /* Calculate thumb dimensions. */
  aspect_ratio = image_width / image_height;
  float thumb_h_px = thumb_height / pixely;
  float thumb_width = aspect_ratio * thumb_h_px * pixelx;

  *r_thumb_width = thumb_width;
  *r_image_width = image_width;
  *r_image_height = image_height;
}

static void get_seq_strip_thumbnails(const View2D *v2d,
                                     const bContext *C,
                                     Scene *scene,
                                     const StripDrawContext &strip,
                                     float pixelx,
                                     float pixely,
                                     bool is_muted,
                                     Vector<SeqThumbInfo> &r_thumbs)
{
  if (!seq::strip_can_have_thumbnail(scene, strip.strip)) {
    return;
  }

  /* No thumbnails if height of the strip is too small. */
  const float thumb_height = strip.strip_content_top - strip.bottom;
  if (thumb_height / pixely <= 20 * UI_SCALE_FAC) {
    return;
  }

  float thumb_width, image_width, image_height;
  strip_get_thumb_image_dimensions(
      strip.strip, pixelx, pixely, &thumb_width, thumb_height, &image_width, &image_height);

  const float crop_x_multiplier = 1.0f / pixelx / (thumb_height / image_height / pixely);

  float upper_thumb_bound = min_ff(strip.right_handle, strip.content_end);
  if (strip.strip->type == STRIP_TYPE_IMAGE) {
    upper_thumb_bound = strip.right_handle;
  }

  float timeline_frame = thumb_calc_first_timeline_frame(
      strip.strip, strip.left_handle, thumb_width, &v2d->cur);

  /* Start going over the strip length. */
  while (timeline_frame < upper_thumb_bound) {
    float thumb_x_end = timeline_frame + thumb_width;
    bool clipped = false;

    /* Reached end of view, no more thumbnails needed. */
    if (timeline_frame > v2d->cur.xmax) {
      break;
    }

    /* Set the clipping bound to show the left handle moving over thumbs and not shift thumbs. */
    float cut_off = 0.0f;
    if (strip.left_handle > timeline_frame && strip.left_handle < thumb_x_end) {
      cut_off = strip.left_handle - timeline_frame;
      clipped = true;
    }

    /* Clip if full thumbnail cannot be displayed. */
    if (thumb_x_end > upper_thumb_bound) {
      thumb_x_end = upper_thumb_bound;
      clipped = true;
    }

    float cropx_min = cut_off * crop_x_multiplier;
    float cropx_max = (thumb_x_end - timeline_frame) * crop_x_multiplier;
    if (cropx_max < 1.0f) {
      break;
    }

    /* Get the thumbnail image. */
    ImBuf *ibuf = seq::thumbnail_cache_get(C, scene, strip.strip, timeline_frame);
    if (ibuf == nullptr) {
      break;
    }

    SeqThumbInfo thumb = {};
    thumb.ibuf = ibuf;
    thumb.cropx_min = 0;
    thumb.cropx_max = ibuf->x - 1;
    if (clipped) {
      thumb.cropx_min = clamp_f(cropx_min, 0, ibuf->x - 1);
      thumb.cropx_max = clamp_f(cropx_max - 1 * 0, 0, ibuf->x - 1);
    }
    thumb.left_handle = strip.left_handle;
    thumb.right_handle = strip.right_handle;
    thumb.is_muted = is_muted;
    thumb.bottom = strip.bottom;
    thumb.top = strip.top;
    thumb.x1 = timeline_frame + cut_off;
    thumb.x2 = thumb_x_end;
    thumb.y1 = strip.bottom;
    thumb.y2 = strip.strip_content_top;
    r_thumbs.append(thumb);

    timeline_frame = thumb_calc_next_timeline_frame(
        strip.strip, strip.left_handle, timeline_frame, thumb_width);
  }
}

struct ThumbsDrawBatch {
  StripsDrawBatch &strips_batch_;
  Array<SeqStripThumbData> thumbs_;
  gpu::UniformBuf *ubo_thumbs_ = nullptr;
  gpu::Shader *shader_ = nullptr;
  gpu::Batch *batch_ = nullptr;
  gpu::Texture *atlas_ = nullptr;
  int binding_context_ = 0;
  int binding_thumbs_ = 0;
  int binding_image_ = 0;
  int thumbs_count_ = 0;

  ThumbsDrawBatch(StripsDrawBatch &strips_batch, gpu::Texture *atlas)
      : strips_batch_(strips_batch), thumbs_(GPU_SEQ_STRIP_DRAW_DATA_LEN), atlas_(atlas)
  {
    shader_ = GPU_shader_get_builtin_shader(GPU_SHADER_SEQUENCER_THUMBS);
    binding_thumbs_ = GPU_shader_get_ubo_binding(shader_, "thumb_data");
    binding_context_ = GPU_shader_get_ubo_binding(shader_, "context_data");
    binding_image_ = GPU_shader_get_sampler_binding(shader_, "image");

    ubo_thumbs_ = GPU_uniformbuf_create(sizeof(SeqStripThumbData) * GPU_SEQ_STRIP_DRAW_DATA_LEN);

    batch_ = GPU_batch_preset_quad();
  }

  ~ThumbsDrawBatch()
  {
    flush_batch();
    GPU_uniformbuf_unbind(ubo_thumbs_);
    GPU_uniformbuf_free(ubo_thumbs_);
  }

  void add_thumb(
      const SeqThumbInfo &info, float width, const rcti &rect, int tex_width, int tex_height)
  {
    if (thumbs_count_ == GPU_SEQ_STRIP_DRAW_DATA_LEN) {
      flush_batch();
    }

    SeqStripThumbData &res = thumbs_[thumbs_count_];
    thumbs_count_++;

    res.left = strips_batch_.pos_to_pixel_space_x(info.left_handle);
    res.right = strips_batch_.pos_to_pixel_space_x(info.right_handle);
    res.bottom = strips_batch_.pos_to_pixel_space_y(info.bottom);
    res.top = strips_batch_.pos_to_pixel_space_y(info.top);
    res.tint_color = float4(1.0f, 1.0f, 1.0f, info.is_muted ? 0.47f : 1.0f);
    res.x1 = strips_batch_.pos_to_pixel_space_x(info.x1);
    res.x2 = strips_batch_.pos_to_pixel_space_x(info.x2);
    res.y1 = strips_batch_.pos_to_pixel_space_y(info.y1);
    res.y2 = strips_batch_.pos_to_pixel_space_y(info.y2);
    res.u1 = float(rect.xmin) / float(tex_width);
    res.u2 = float(rect.xmin + width) / float(tex_width);
    res.v1 = float(rect.ymin) / float(tex_height);
    res.v2 = float(rect.ymax) / float(tex_height);
  }

  void flush_batch()
  {
    if (thumbs_count_ == 0) {
      return;
    }

    GPU_uniformbuf_update(ubo_thumbs_, thumbs_.data());

    GPU_shader_bind(shader_);
    GPU_uniformbuf_bind(ubo_thumbs_, binding_thumbs_);
    GPU_uniformbuf_bind(strips_batch_.get_ubo_context(), binding_context_);
    GPU_texture_bind(atlas_, binding_image_);

    GPU_batch_set_shader(batch_, shader_);
    GPU_batch_draw_instance_range(batch_, 0, thumbs_count_);
    thumbs_count_ = 0;
  }
};

void draw_strip_thumbnails(const TimelineDrawContext &ctx,
                           StripsDrawBatch &strips_batch,
                           const Vector<StripDrawContext> &strips)
{
  /* Nothing to do if we're not showing thumbnails overall. */
  if ((ctx.sseq->flag & SEQ_SHOW_OVERLAY) == 0 ||
      (ctx.sseq->timeline_overlay.flag & SEQ_TIMELINE_SHOW_THUMBNAILS) == 0)
  {
    return;
  }

  /* Gather information for all thumbnails. */
  Vector<SeqThumbInfo> thumbs;
  for (const StripDrawContext &strip : strips) {
    get_seq_strip_thumbnails(
        ctx.v2d, ctx.C, ctx.scene, strip, ctx.pixelx, ctx.pixely, strip.is_muted, thumbs);
  }
  if (thumbs.is_empty()) {
    return;
  }

  ColorManagedViewSettings *view_settings;
  ColorManagedDisplaySettings *display_settings;
  IMB_colormanagement_display_settings_from_ctx(ctx.C, &view_settings, &display_settings);

  /* Arrange thumbnail images into a texture atlas, using a simple
   * "add to current row until end, then start a new row". Thumbnail
   * images are most often same height (but varying width due to horizontal
   * cropping), so this simple algorithm works well enough. */
  constexpr int ATLAS_WIDTH = 4096;
  constexpr int ATLAS_MAX_HEIGHT = 4096;
  int cur_row_x = 0;
  int cur_row_y = 0;
  int cur_row_height = 0;
  Vector<rcti> rects;
  rects.reserve(thumbs.size());
  for (const SeqThumbInfo &info : thumbs) {
    int cropx_min = int(info.cropx_min);
    int cropx_max = int(math::ceil(info.cropx_max));
    int width = cropx_max - cropx_min + 1;
    int height = info.ibuf->y;
    cur_row_height = math::max(cur_row_height, height);

    /* If this thumb would not fit onto current row, start a new row. */
    if (cur_row_x + width > ATLAS_WIDTH) {
      cur_row_y += cur_row_height + 1; /* +1 empty pixel for bilinear filter. */
      cur_row_height = height;
      cur_row_x = 0;
      if (cur_row_y > ATLAS_MAX_HEIGHT) {
        break;
      }
    }

    /* Record our rect. */
    rcti rect{cur_row_x, cur_row_x + width, cur_row_y, cur_row_y + height};
    rects.append(rect);

    /* Advance to next item inside row. */
    cur_row_x += width + 1; /* +1 empty pixel for bilinear filter. */
  }

  /* Create the atlas GPU texture. */
  const int tex_width = ATLAS_WIDTH;
  const int tex_height = cur_row_y + cur_row_height;
  Array<uchar> tex_data(tex_width * tex_height * 4, 0);
  for (int64_t i = 0; i < rects.size(); i++) {
    /* Copy one thumbnail into atlas. */
    const rcti &rect = rects[i];
    SeqThumbInfo &info = thumbs[i];

    void *cache_handle = nullptr;
    uchar *display_buffer = IMB_display_buffer_acquire(
        info.ibuf, view_settings, display_settings, &cache_handle);
    if (display_buffer != nullptr && info.ibuf != nullptr) {
      int cropx_min = int(info.cropx_min);
      int cropx_max = int(math::ceil(info.cropx_max));
      int width = cropx_max - cropx_min + 1;
      int height = info.ibuf->y;
      const uchar *src = display_buffer + cropx_min * 4;
      uchar *dst = &tex_data[(rect.ymin * ATLAS_WIDTH + rect.xmin) * 4];
      for (int y = 0; y < height; y++) {
        memcpy(dst, src, width * 4);
        src += info.ibuf->x * 4;
        dst += ATLAS_WIDTH * 4;
      }
    }
    IMB_display_buffer_release(cache_handle);

    /* Release thumb image reference. */
    IMB_freeImBuf(info.ibuf);
    info.ibuf = nullptr;
  }
  gpu::Texture *atlas = GPU_texture_create_2d("thumb_atlas",
                                              tex_width,
                                              tex_height,
                                              1,
                                              gpu::TextureFormat::UNORM_8_8_8_8,
                                              GPU_TEXTURE_USAGE_SHADER_READ,
                                              nullptr);
  GPU_texture_update(atlas, GPU_DATA_UBYTE, tex_data.data());
  GPU_texture_filter_mode(atlas, true);
  GPU_texture_extend_mode(atlas, GPU_SAMPLER_EXTEND_MODE_CLAMP_TO_BORDER);

  /* Draw all thumbnails. */
  GPU_matrix_push_projection();
  wmOrtho2_region_pixelspace(ctx.region);

  ThumbsDrawBatch batch(strips_batch, atlas);
  for (int64_t i = 0; i < rects.size(); i++) {
    const rcti &rect = rects[i];
    const SeqThumbInfo &info = thumbs[i];
    batch.add_thumb(info, info.cropx_max - info.cropx_min + 1, rect, tex_width, tex_height);
  }
  batch.flush_batch();

  GPU_matrix_pop_projection();

  GPU_texture_unbind(atlas);
  GPU_texture_free(atlas);
}

}  // namespace blender::ed::vse
