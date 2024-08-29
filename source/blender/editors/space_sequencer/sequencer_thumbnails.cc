/* SPDX-FileCopyrightText: 2021-2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spseq
 */

#include "BLI_math_base.h"
#include "BLI_math_base.hh"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_rect.h"

#include "BKE_context.hh"

#include "IMB_imbuf.hh"

#include "DNA_sequence_types.h"
#include "DNA_space_types.h"
#include "DNA_view2d_types.h"

#include "BIF_glutil.hh"

#include "SEQ_channels.hh"
#include "SEQ_render.hh"
#include "SEQ_sequencer.hh"
#include "SEQ_thumbnail_cache.hh"
#include "SEQ_time.hh"

using namespace blender;

static float thumb_calc_first_timeline_frame(const Scene *scene,
                                             Sequence *seq,
                                             float frame_step,
                                             const rctf *view_area)
{
  int first_drawable_frame = max_iii(
      SEQ_time_left_handle_frame_get(scene, seq), seq->start, view_area->xmin);

  /* First frame should correspond to handle position. */
  if (first_drawable_frame == SEQ_time_left_handle_frame_get(scene, seq)) {
    return SEQ_time_left_handle_frame_get(scene, seq);
  }

  float aligned_frame_offset = int((first_drawable_frame - seq->start) / frame_step) * frame_step;
  return seq->start + aligned_frame_offset;
}

static float thumb_calc_next_timeline_frame(const Scene *scene,
                                            Sequence *seq,
                                            float last_frame,
                                            float frame_step)
{
  float next_frame = last_frame + frame_step;

  /* If handle position was displayed, align next frame with `seq->start`. */
  if (last_frame == SEQ_time_left_handle_frame_get(scene, seq)) {
    next_frame = seq->start + (int((last_frame - seq->start) / frame_step) + 1) * frame_step;
  }

  return next_frame;
}

static void seq_get_thumb_image_dimensions(Sequence *seq,
                                           float pixelx,
                                           float pixely,
                                           float *r_thumb_width,
                                           float thumb_height,
                                           float *r_image_width,
                                           float *r_image_height)
{
  float image_width = seq->strip->stripdata->orig_width;
  float image_height = seq->strip->stripdata->orig_height;

  /* Fix the dimensions to be max SEQ_THUMB_SIZE for x or y. */
  float aspect_ratio = image_width / image_height;
  if (image_width > image_height) {
    image_width = seq::SEQ_THUMB_SIZE;
    image_height = round_fl_to_int(image_width / aspect_ratio);
  }
  else {
    image_height = seq::SEQ_THUMB_SIZE;
    image_width = round_fl_to_int(image_height * aspect_ratio);
  }

  /* Calculate thumb dimensions. */
  aspect_ratio = image_width / image_height;
  float thumb_h_px = thumb_height / pixely;
  float thumb_width = aspect_ratio * thumb_h_px * pixelx;

  *r_thumb_width = thumb_width;
  if (r_image_width && r_image_height) {
    *r_image_width = image_width;
    *r_image_height = image_height;
  }
}

static void make_ibuf_semitransparent(ImBuf *ibuf)
{
  const uchar alpha = 120;
  if (ibuf->byte_buffer.data) {
    uchar *buf = ibuf->byte_buffer.data;
    for (int pixel = ibuf->x * ibuf->y; pixel--; buf += 4) {
      buf[3] = alpha;
    }
  }
  if (ibuf->float_buffer.data) {
    float *buf = ibuf->float_buffer.data;
    for (int pixel = ibuf->x * ibuf->y; pixel--; buf += ibuf->channels) {
      buf[3] = (alpha / 255.0f);
    }
  }
}

/* Signed distance to rounded box, centered at origin.
 * Reference: https://iquilezles.org/articles/distfunctions2d/ */
static float sdf_rounded_box(float2 pos, float2 size, float radius)
{
  float2 q = math::abs(pos) - size + radius;
  return math::min(math::max(q.x, q.y), 0.0f) + math::length(math::max(q, float2(0.0f))) - radius;
}

static void eval_round_corners_pixel(
    ImBuf *ibuf, float radius, float2 bmin, float2 bmax, float2 pos)
{
  int ix = int(pos.x);
  int iy = int(pos.y);
  if (ix < 0 || ix >= ibuf->x || iy < 0 || iy >= ibuf->y) {
    return;
  }
  float2 center = (bmin + bmax) * 0.5f;
  float2 size = (bmax - bmin) * 0.5f;
  float d = sdf_rounded_box(pos - center, size, radius);
  if (d <= 0.0f) {
    return;
  }
  /* Outside of rounded rectangle, set pixel alpha to zero. */
  if (ibuf->byte_buffer.data != nullptr) {
    int64_t ofs = (int64_t(iy) * ibuf->x + ix) * 4;
    ibuf->byte_buffer.data[ofs + 3] = 0;
  }
  if (ibuf->float_buffer.data != nullptr) {
    int64_t ofs = (int64_t(iy) * ibuf->x + ix) * ibuf->channels;
    ibuf->float_buffer.data[ofs + 3] = 0.0f;
  }
}

static void make_ibuf_round_corners(ImBuf *ibuf, float radius, float2 bmin, float2 bmax)
{
  /* Evaluate radius*radius squares at corners. */
  for (int by = 0; by < radius; by++) {
    for (int bx = 0; bx < radius; bx++) {
      eval_round_corners_pixel(ibuf, radius, bmin, bmax, float2(bmin.x + bx, bmin.y + by));
      eval_round_corners_pixel(ibuf, radius, bmin, bmax, float2(bmax.x - bx, bmin.y + by));
      eval_round_corners_pixel(ibuf, radius, bmin, bmax, float2(bmin.x + bx, bmax.y - by));
      eval_round_corners_pixel(ibuf, radius, bmin, bmax, float2(bmax.x - bx, bmax.y - by));
    }
  }
}

void draw_seq_strip_thumbnail(View2D *v2d,
                              const bContext *C,
                              Scene *scene,
                              Sequence *seq,
                              float y1,
                              float y2,
                              float y_top,
                              float pixelx,
                              float pixely,
                              float round_radius)
{
  SpaceSeq *sseq = CTX_wm_space_seq(C);
  if ((sseq->flag & SEQ_SHOW_OVERLAY) == 0 ||
      (sseq->timeline_overlay.flag & SEQ_TIMELINE_SHOW_THUMBNAILS) == 0 ||
      !ELEM(seq->type, SEQ_TYPE_MOVIE, SEQ_TYPE_IMAGE))
  {
    return;
  }

  StripElem *se = seq->strip->stripdata;
  if (se->orig_height == 0 || se->orig_width == 0) {
    return;
  }

  /* If width of the strip too small ignore drawing thumbnails. */
  if ((y2 - y1) / pixely <= 20 * UI_SCALE_FAC) {
    return;
  }

  Editing *ed = SEQ_editing_get(scene);
  ListBase *channels = ed ? SEQ_channels_displayed_get(ed) : nullptr;

  float thumb_width, image_width, image_height;
  const float thumb_height = y2 - y1;
  seq_get_thumb_image_dimensions(
      seq, pixelx, pixely, &thumb_width, thumb_height, &image_width, &image_height);

  const float zoom_y = thumb_height / image_height;
  const float crop_x_multiplier = 1.0f / pixelx / (zoom_y / pixely);

  float thumb_y_end = y1 + thumb_height;

  const float seq_left_handle = SEQ_time_left_handle_frame_get(scene, seq);
  const float seq_right_handle = SEQ_time_right_handle_frame_get(scene, seq);

  float upper_thumb_bound = SEQ_time_has_right_still_frames(scene, seq) ?
                                SEQ_time_content_end_frame_get(scene, seq) :
                                seq_right_handle;
  if (seq->type == SEQ_TYPE_IMAGE) {
    upper_thumb_bound = seq_right_handle;
  }

  float timeline_frame = thumb_calc_first_timeline_frame(scene, seq, thumb_width, &v2d->cur);

  /* Start drawing. */
  while (timeline_frame < upper_thumb_bound) {
    float thumb_x_end = timeline_frame + thumb_width;
    bool clipped = false;

    /* Checks to make sure that thumbs are loaded only when in view and within the confines of the
     * strip. Some may not be required but better to have conditions for safety as x1 here is
     * point to start caching from and not drawing. */
    if (timeline_frame > v2d->cur.xmax) {
      break;
    }

    /* Set the clipping bound to show the left handle moving over thumbs and not shift thumbs. */
    float cut_off = 0.0f;
    if (seq_left_handle > timeline_frame && seq_left_handle < thumb_x_end) {
      cut_off = seq_left_handle - timeline_frame;
      clipped = true;
    }

    /* Clip if full thumbnail cannot be displayed. */
    if (thumb_x_end > upper_thumb_bound) {
      thumb_x_end = upper_thumb_bound;
      clipped = true;
    }

    int cropx_min = int(cut_off * crop_x_multiplier);
    int cropx_max = int((thumb_x_end - timeline_frame) * crop_x_multiplier);
    if (cropx_max < 1) {
      break;
    }
    rcti crop;
    BLI_rcti_init(&crop, cropx_min, cropx_max - 1, 0, int(image_height) - 1);

    /* Get the thumbnail image. */
    ImBuf *ibuf = seq::thumbnail_cache_get(C, scene, seq, timeline_frame);
    if (ibuf && clipped) {
      /* Crop it to the part needed by the timeline view. */
      ImBuf *ibuf_cropped = IMB_dupImBuf(ibuf);
      if (crop.xmin < 0 || crop.ymin < 0) {
        crop.xmin = 0;
        crop.ymin = 0;
      }
      if (crop.xmax >= ibuf->x || crop.ymax >= ibuf->y) {
        crop.xmax = ibuf->x - 1;
        crop.ymax = ibuf->y - 1;
      }
      IMB_rect_crop(ibuf_cropped, &crop);
      IMB_freeImBuf(ibuf);
      ibuf = ibuf_cropped;
    }

    /* If there is no image still, abort. */
    if (!ibuf) {
      break;
    }

    /* Transparency on mute. */
    bool muted = channels ? SEQ_render_is_muted(channels, seq) : false;
    if (muted) {
      /* Work on a copy of the thumbnail image, so that transparency
       * is not stored into the thumbnail cache. */
      ImBuf *copy = IMB_dupImBuf(ibuf);
      IMB_freeImBuf(ibuf);
      ibuf = copy;
      make_ibuf_semitransparent(ibuf);
    }

    /* If thumbnail start or end falls within strip corner rounding area,
     * we need to manually set thumbnail pixels that are outside of rounded
     * rectangle to be transparent. Ideally this would be done on the GPU
     * while drawing, but since rendering is done through OCIO shaders that
     * is hard to do. */
    const float xpos = timeline_frame + cut_off;

    const float zoom_x = (thumb_x_end - xpos) / ibuf->x;

    const float radius = ibuf->y * round_radius * pixely / (y2 - y1);
    if (radius > 0.9f) {
      if (xpos < seq_left_handle + round_radius * pixelx ||
          thumb_x_end > seq_right_handle - round_radius * pixelx)
      {
        /* Work on a copy of the thumbnail image, so that corner rounding
         * is not stored into thumbnail cache. */
        ImBuf *copy = IMB_dupImBuf(ibuf);
        IMB_freeImBuf(ibuf);
        ibuf = copy;

        float round_y_top = ibuf->y * (y_top - y1) / (y2 - y1);
        make_ibuf_round_corners(ibuf,
                                radius,
                                float2((seq_left_handle - xpos) / zoom_x, 0),
                                float2((seq_right_handle - xpos) / zoom_x, round_y_top));
      }
    }

    ED_draw_imbuf_ctx_clipping(
        C, ibuf, xpos, y1, true, xpos, y1, thumb_x_end, thumb_y_end, zoom_x, zoom_y);
    IMB_freeImBuf(ibuf);
    timeline_frame = thumb_calc_next_timeline_frame(scene, seq, timeline_frame, thumb_width);
  }
}
