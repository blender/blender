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

static bool add_thumbnail_at_frame(float timeline_frame,
                                   const bContext *C,
                                   const View2D *v2d,
                                   const StripDrawContext &strip,
                                   Scene *scene,
                                   float thumb_width,
                                   float crop_left,
                                   float crop_right,
                                   float crop_x_multiplier,
                                   float upper_thumb_bound,
                                   float display_offset,
                                   bool is_muted,
                                   Vector<SeqThumbInfo> &r_thumbs)
{
  /* Frame at which the thumb at `timeline_frame` will be drawn. */
  const float display_frame = timeline_frame + display_offset;

  float thumb_x_end = display_frame + thumb_width;
  bool clipped = false;

  /* Reached end of view, no more thumbnails needed. */
  if (display_frame > v2d->cur.xmax) {
    return false;
  }

  /* Clip if full thumbnail cannot be displayed. */
  if (thumb_x_end > upper_thumb_bound) {
    thumb_x_end = upper_thumb_bound;
    clipped = true;
  }

  crop_left = max_ff(crop_left, 0.0f);
  crop_right = max_ff(crop_right, 0.0f);

  if (crop_left > 0.0f || crop_right > 0.0f) {
    clipped = true;
  }

  float cropx_min = crop_left * crop_x_multiplier;
  float cropx_max = min_ff(thumb_width - crop_right, (thumb_x_end - display_frame)) *
                    crop_x_multiplier;
  if (cropx_max - cropx_min < 1.0f) {
    return false;
  }

  /* Get the thumbnail image. */
  ImBuf *ibuf = seq::thumbnail_cache_get(C, scene, strip.strip, timeline_frame);
  if (ibuf == nullptr) {
    /* Thumbnail is not in cache but still other frames have to request for thumbnails. */
    return true;
  }

  SeqThumbInfo thumb = {};
  thumb.ibuf = ibuf;
  thumb.cropx_min = 0;
  thumb.cropx_max = ibuf->x - 1;
  if (clipped) {
    thumb.cropx_min = clamp_f(cropx_min, 0, ibuf->x - 1);
    thumb.cropx_max = clamp_f(cropx_max, 0, ibuf->x - 1);
  }
  thumb.left_handle = strip.left_handle;
  thumb.right_handle = strip.right_handle;
  thumb.is_muted = is_muted;
  thumb.bottom = strip.bottom;
  thumb.top = strip.top;
  thumb.x1 = display_frame + crop_left;
  thumb.x2 = min_ff(thumb_x_end, display_frame + thumb_width - crop_right);
  thumb.y1 = strip.bottom;
  thumb.y2 = strip.strip_content_top;
  r_thumbs.append(thumb);

  return true;
};

static bool is_thumbnail_in_view(const float timeline_frame,
                                 const float thumb_width,
                                 const View2D *v2d)
{
  if (timeline_frame < v2d->cur.xmax && timeline_frame + thumb_width > v2d->cur.xmin) {
    return true;
  }
  return false;
}

static void get_seq_strip_ends_thumbnails(const View2D *v2d,
                                          const bContext *C,
                                          const StripDrawContext &strip,
                                          Scene *scene,
                                          const float thumb_width,
                                          const float crop_x_multiplier,
                                          const float pixelx,
                                          const float upper_thumb_bound,
                                          bool is_muted,
                                          Vector<SeqThumbInfo> &r_thumbs)
{
  const float left_frame = max_ff(strip.content_start, strip.left_handle);
  const float right_frame = strip.is_single_image ? left_frame :
                                                    min_ff(strip.content_end, strip.right_handle);
  const float strip_width = strip.is_single_image ? (strip.right_handle - strip.left_handle) :
                                                    (right_frame - left_frame);
  const float overlap = max_ff(0.0f, 2.0f * thumb_width - strip_width);
  const bool only_right_handle_selected = ((strip.strip->flag & SEQ_RIGHTSEL) &&
                                           !(strip.strip->flag & SEQ_LEFTSEL));
  /* Offset the start of last thumbnail. */
  const float display_offset = (strip.is_single_image ? strip_width : 0.0f) - thumb_width;
  const float gap = 1.5f * pixelx * UI_SCALE_FAC;

  float crop_left = 0.0;
  float crop_right = 0.0;

  if (overlap > 0.0f && only_right_handle_selected) {
    /* Crop left thumbnail from right. */
    crop_right = overlap + gap;
  }
  else if (overlap > 0.0f) {
    /* Crop right thumbnail from left. */
    crop_left = overlap + gap;
  }

  if (is_thumbnail_in_view(left_frame, thumb_width, v2d)) {
    /* Draw left thumbnail. */
    add_thumbnail_at_frame(left_frame,
                           C,
                           v2d,
                           strip,
                           scene,
                           thumb_width,
                           0.0f,
                           crop_right,
                           crop_x_multiplier,
                           upper_thumb_bound,
                           0.0f,
                           is_muted,
                           r_thumbs);
  }

  if (is_thumbnail_in_view(right_frame + display_offset, thumb_width, v2d)) {
    /* Draw right thumbnail. */
    add_thumbnail_at_frame(right_frame,
                           C,
                           v2d,
                           strip,
                           scene,
                           thumb_width,
                           crop_left,
                           0.0f,
                           crop_x_multiplier,
                           upper_thumb_bound,
                           display_offset,
                           is_muted,
                           r_thumbs);
  }
}

static void get_seq_strip_thumbnails(const View2D *v2d,
                                     const bContext *C,
                                     Scene *scene,
                                     const StripDrawContext &strip,
                                     float pixelx,
                                     float pixely,
                                     bool is_muted,
                                     bool show_only_at_strip_ends,
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
  if (strip.is_single_image) {
    upper_thumb_bound = strip.right_handle;
  }

  if (show_only_at_strip_ends) {
    get_seq_strip_ends_thumbnails(v2d,
                                  C,
                                  strip,
                                  scene,
                                  thumb_width,
                                  crop_x_multiplier,
                                  pixelx,
                                  upper_thumb_bound,
                                  is_muted,
                                  r_thumbs);
    return;
  }

  int first_drawable_frame = max_iii(strip.left_handle, strip.strip->start, v2d->cur.xmin);
  /* Calculate how many thumbnails should we skip over to get to the first visible thumbnail. */
  float aligned_frame_offset = int((first_drawable_frame - strip.strip->start) / thumb_width) *
                               thumb_width;

  /* If the first frame should correspond to the left handle position,
   * we want to make it slide under the other thumbs when moving
   * the left handle. This is so that we don't shift around the rest of the
   * thumbnails.
   */
  bool draw_next_frame_ontop = first_drawable_frame == strip.left_handle;
  float timeline_frame;
  if (draw_next_frame_ontop) {
    timeline_frame = first_drawable_frame;
  }
  else {
    timeline_frame = strip.strip->start + aligned_frame_offset;
  }

  /* Start going over the strip length. */
  while (timeline_frame < upper_thumb_bound) {

    const bool should_add_next_thumbnail = add_thumbnail_at_frame(timeline_frame,
                                                                  C,
                                                                  v2d,
                                                                  strip,
                                                                  scene,
                                                                  thumb_width,
                                                                  0.0f,
                                                                  0.0f,
                                                                  crop_x_multiplier,
                                                                  upper_thumb_bound,
                                                                  0.0f,
                                                                  is_muted,
                                                                  r_thumbs);

    if (!should_add_next_thumbnail) {
      break;
    }

    if (draw_next_frame_ontop) {
      timeline_frame = strip.strip->start + aligned_frame_offset + thumb_width;
      draw_next_frame_ontop = false;
    }
    else {
      timeline_frame += thumb_width;
    }
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
  const bool show_thumbnails = (ctx.sseq->timeline_overlay.flag &
                                SEQ_TIMELINE_STRIP_END_THUMBNAILS) ||
                               (ctx.sseq->timeline_overlay.flag &
                                SEQ_TIMELINE_CONTINUOUS_THUMBNAILS);
  /* Nothing to do if we're not showing thumbnails overall. */
  if ((ctx.sseq->flag & SEQ_SHOW_OVERLAY) == 0 || !show_thumbnails) {
    return;
  }

  /* Gather information for all thumbnails. */
  Vector<SeqThumbInfo> thumbs;
  /* Thumbnail display mode (Strip ends / Continuous). */
  const bool show_only_at_strip_ends = (ctx.sseq->timeline_overlay.flag &
                                        SEQ_TIMELINE_STRIP_END_THUMBNAILS);

  for (const StripDrawContext &strip : strips) {
    get_seq_strip_thumbnails(ctx.v2d,
                             ctx.C,
                             ctx.scene,
                             strip,
                             ctx.pixelx,
                             ctx.pixely,
                             strip.is_muted,
                             show_only_at_strip_ends,
                             thumbs);
  }
  if (thumbs.is_empty()) {
    return;
  }

  Scene *sequencer_scene = CTX_data_sequencer_scene(ctx.C);
  ColorManagedViewSettings *view_settings = &sequencer_scene->view_settings;
  ColorManagedDisplaySettings *display_settings = &sequencer_scene->display_settings;

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
