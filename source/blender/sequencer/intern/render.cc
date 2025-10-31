/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 * SPDX-FileCopyrightText: 2003-2024 Blender Authors
 * SPDX-FileCopyrightText: 2005-2006 Peter Schlaile <peter [at] schlaile [dot] de>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sequencer
 */

#include <ctime>

#include "MEM_guardedalloc.h"

#include "DNA_defaults.h"
#include "DNA_mask_types.h"
#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"
#include "DNA_space_types.h"

#include "BLI_linklist.h"
#include "BLI_listbase.h"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.hh"
#include "BLI_path_utils.hh"
#include "BLI_rect.h"
#include "BLI_task.hh"

#include "BKE_anim_data.hh"
#include "BKE_animsys.h"
#include "BKE_global.hh"
#include "BKE_image.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_mask.h"
#include "BKE_movieclip.h"
#include "BKE_scene.hh"
#include "BKE_scene_runtime.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"
#include "DEG_depsgraph_debug.hh"
#include "DEG_depsgraph_query.hh"

#include "IMB_colormanagement.hh"
#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"
#include "IMB_metadata.hh"

#include "MOV_read.hh"

#include "RE_engine.h"
#include "RE_pipeline.h"

#include "SEQ_channels.hh"
#include "SEQ_effects.hh"
#include "SEQ_iterator.hh"
#include "SEQ_offscreen.hh"
#include "SEQ_proxy.hh"
#include "SEQ_relations.hh"
#include "SEQ_render.hh"
#include "SEQ_sequencer.hh"
#include "SEQ_time.hh"
#include "SEQ_transform.hh"
#include "SEQ_utils.hh"

#include "cache/final_image_cache.hh"
#include "cache/intra_frame_cache.hh"
#include "cache/source_image_cache.hh"
#include "effects/effects.hh"
#include "modifiers/modifier.hh"
#include "multiview.hh"
#include "prefetch.hh"
#include "proxy.hh"
#include "render.hh"
#include "utils.hh"

#include <algorithm>

namespace blender::seq {

static ImBuf *seq_render_strip_stack(const RenderData *context,
                                     SeqRenderState *state,
                                     ListBase *channels,
                                     ListBase *seqbasep,
                                     float timeline_frame,
                                     int chanshown);

static Mutex seq_render_mutex;
DrawViewFn view3d_fn = nullptr; /* nullptr in background mode */

/* -------------------------------------------------------------------- */
/** \name Color-space utility functions
 * \{ */

void seq_imbuf_assign_spaces(const Scene *scene, ImBuf *ibuf)
{
#if 0
  /* Byte buffer is supposed to be in sequencer working space already. */
  if (ibuf->rect != nullptr) {
    IMB_colormanagement_assign_byte_colorspace(ibuf, scene->sequencer_colorspace_settings.name);
  }
#endif
  if (ibuf->float_buffer.data != nullptr) {
    IMB_colormanagement_assign_float_colorspace(ibuf, scene->sequencer_colorspace_settings.name);
  }
}

void seq_imbuf_to_sequencer_space(const Scene *scene, ImBuf *ibuf, bool make_float)
{
  /* Early output check: if both buffers are nullptr we have nothing to convert. */
  if (ibuf->float_buffer.data == nullptr && ibuf->byte_buffer.data == nullptr) {
    return;
  }
  /* Get common conversion settings. */
  const char *to_colorspace = scene->sequencer_colorspace_settings.name;
  /* Perform actual conversion logic. */
  if (ibuf->float_buffer.data == nullptr) {
    /* We are not requested to give float buffer and byte buffer is already
     * in thee required colorspace. Can skip doing anything here.
     */
    const char *from_colorspace = IMB_colormanagement_get_rect_colorspace(ibuf);
    if (!make_float && STREQ(from_colorspace, to_colorspace)) {
      return;
    }

    IMB_alloc_float_pixels(ibuf, 4, false);
    IMB_colormanagement_transform_byte_to_float(ibuf->float_buffer.data,
                                                ibuf->byte_buffer.data,
                                                ibuf->x,
                                                ibuf->y,
                                                ibuf->channels,
                                                from_colorspace,
                                                to_colorspace);
    /* We don't need byte buffer anymore. */
    IMB_free_byte_pixels(ibuf);
  }
  else {
    const char *from_colorspace = IMB_colormanagement_get_float_colorspace(ibuf);
    /* Unknown input color space, can't perform conversion. */
    if (from_colorspace == nullptr || from_colorspace[0] == '\0') {
      return;
    }
    /* We don't want both byte and float buffers around: they'll either run
     * out of sync or conversion of byte buffer will lose precision in there.
     */
    if (ibuf->byte_buffer.data != nullptr) {
      IMB_free_byte_pixels(ibuf);
    }
    IMB_colormanagement_transform_float(ibuf->float_buffer.data,
                                        ibuf->x,
                                        ibuf->y,
                                        ibuf->channels,
                                        from_colorspace,
                                        to_colorspace,
                                        true);
  }
  seq_imbuf_assign_spaces(scene, ibuf);
}

void render_imbuf_from_sequencer_space(const Scene *scene, ImBuf *ibuf)
{
  const char *from_colorspace = scene->sequencer_colorspace_settings.name;
  const char *to_colorspace = IMB_colormanagement_role_colorspace_name_get(
      COLOR_ROLE_SCENE_LINEAR);

  if (!ibuf->float_buffer.data) {
    return;
  }

  if (to_colorspace && to_colorspace[0] != '\0') {
    IMB_colormanagement_transform_float(ibuf->float_buffer.data,
                                        ibuf->x,
                                        ibuf->y,
                                        ibuf->channels,
                                        from_colorspace,
                                        to_colorspace,
                                        true);
    IMB_colormanagement_assign_float_colorspace(ibuf, to_colorspace);
  }
}

void render_pixel_from_sequencer_space_v4(const Scene *scene, float pixel[4])
{
  const char *from_colorspace = scene->sequencer_colorspace_settings.name;
  const char *to_colorspace = IMB_colormanagement_role_colorspace_name_get(
      COLOR_ROLE_SCENE_LINEAR);

  if (to_colorspace && to_colorspace[0] != '\0') {
    IMB_colormanagement_transform_v4(pixel, from_colorspace, to_colorspace);
  }
  else {
    /* if no color management enables fallback to legacy conversion */
    srgb_to_linearrgb_v4(pixel, pixel);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Rendering utility functions
 * \{ */

void render_new_render_data(Main *bmain,
                            Depsgraph *depsgraph,
                            Scene *scene,
                            int rectx,
                            int recty,
                            eSpaceSeq_Proxy_RenderSize preview_render_size,
                            int for_render,
                            RenderData *r_context)
{
  r_context->bmain = bmain;
  r_context->depsgraph = depsgraph;
  r_context->scene = scene;
  r_context->rectx = rectx;
  r_context->recty = recty;
  r_context->preview_render_size = preview_render_size;
  r_context->ignore_missing_media = false;
  r_context->for_render = for_render;
  r_context->motion_blur_samples = 0;
  r_context->motion_blur_shutter = 0;
  r_context->skip_cache = false;
  r_context->is_proxy_render = false;
  r_context->view_id = 0;
  r_context->gpu_offscreen = nullptr;
  r_context->gpu_viewport = nullptr;
  r_context->task_id = SEQ_TASK_MAIN_RENDER;
  r_context->is_prefetch_render = false;
}

StripElem *render_give_stripelem(const Scene *scene, const Strip *strip, int timeline_frame)
{
  StripElem *se = strip->data->stripdata;

  if (strip->type == STRIP_TYPE_IMAGE) {
    /* only IMAGE strips use the whole array, MOVIE strips use only the first element,
     * all other strips don't use this...
     */

    int frame_index = round_fl_to_int(give_frame_index(scene, strip, timeline_frame));

    if (frame_index == -1 || se == nullptr) {
      return nullptr;
    }

    se += frame_index + strip->anim_startofs;
  }
  return se;
}

Vector<Strip *> seq_shown_strips_get(const Scene *scene,
                                     ListBase *channels,
                                     ListBase *seqbase,
                                     const int timeline_frame,
                                     const int chanshown)
{
  VectorSet strips = query_rendered_strips(scene, channels, seqbase, timeline_frame, chanshown);
  const int strip_count = strips.size();

  if (UNLIKELY(strip_count > MAX_CHANNELS)) {
    BLI_assert_msg(0, "Too many strips, this shouldn't happen");
    return {};
  }

  Vector<Strip *> strips_vec = strips.extract_vector();
  /* Sort strips by channel. */
  std::sort(strips_vec.begin(), strips_vec.end(), [](const Strip *a, const Strip *b) {
    return a->channel < b->channel;
  });
  return strips_vec;
}

StripScreenQuad get_strip_screen_quad(const RenderData *context, const Strip *strip)
{
  Scene *scene = context->scene;
  const int x = context->rectx;
  const int y = context->recty;
  const float2 offset{x * 0.5f, y * 0.5f};

  Array<float2> quad = image_transform_final_quad_get(scene, strip);
  const float scale = get_render_scale_factor(*context);
  return StripScreenQuad{float2(quad[0] * scale + offset),
                         float2(quad[1] * scale + offset),
                         float2(quad[2] * scale + offset),
                         float2(quad[3] * scale + offset)};
}

/* Is quad `a` fully contained (i.e. covered by) quad `b`? For that to happen,
 * all corners of `a` have to be inside `b`. */
static bool is_quad_a_inside_b(const StripScreenQuad &a, const StripScreenQuad &b)
{
  return isect_point_quad_v2(a.v0, b.v0, b.v1, b.v2, b.v3) &&
         isect_point_quad_v2(a.v1, b.v0, b.v1, b.v2, b.v3) &&
         isect_point_quad_v2(a.v2, b.v0, b.v1, b.v2, b.v3) &&
         isect_point_quad_v2(a.v3, b.v0, b.v1, b.v2, b.v3);
}

/* Tracking of "known to be opaque" strip quad coordinates, along with their
 * order index within visible strips during rendering. */

struct OpaqueQuad {
  StripScreenQuad quad;
  int order_index;
};

struct OpaqueQuadTracker {
  Vector<OpaqueQuad, 4> opaques;

  /* Determine if the input strip is completely behind opaque strips that are
   * above it. Current implementation is simple and only checks if strip is
   * completely covered by any other strip. It does not detect case where
   * a strip is not covered by a single strip, but is behind of the union
   * of the strips above. */
  bool is_occluded(const RenderData *context, const Strip *strip, int order_index) const
  {
    StripScreenQuad quad = get_strip_screen_quad(context, strip);
    if (quad.is_empty()) {
      /* Strip size is not initialized/valid, we can't know if it is occluded. */
      return false;
    }
    for (const OpaqueQuad &q : opaques) {
      if (q.order_index > order_index && is_quad_a_inside_b(quad, q.quad)) {
        return true;
      }
    }
    return false;
  }

  void add_occluder(const RenderData *context, const Strip *strip, int order_index)
  {
    StripScreenQuad quad = get_strip_screen_quad(context, strip);
    if (!quad.is_empty()) {
      opaques.append({quad, order_index});
    }
  }
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Preprocessing & Effects
 *
 * Input preprocessing for STRIP_TYPE_IMAGE, STRIP_TYPE_MOVIE, STRIP_TYPE_MOVIECLIP and
 * STRIP_TYPE_SCENE.
 *
 * Do all the things you can't really do afterwards using sequence effects
 * (read: before re-scaling to render resolution has been done).
 *
 * Order is important!
 *
 * - De-interlace.
 * - Crop and transform in image source coordinate space.
 * - Flip X + Flip Y (could be done afterwards, backward compatibility).
 * - Promote image to float data (affects pipeline operations afterwards).
 * - Color balance (is most efficient in the byte -> float
 *   (future: half -> float should also work fine!)
 *   case, if done on load, since we can use lookup tables).
 * - Pre-multiply.
 * \{ */

static bool sequencer_use_transform(const Strip *strip)
{
  const StripTransform *transform = strip->data->transform;

  if (transform->xofs != 0 || transform->yofs != 0 || transform->scale_x != 1 ||
      transform->scale_y != 1 || transform->rotation != 0)
  {
    return true;
  }

  return false;
}

static bool sequencer_use_crop(const Strip *strip)
{
  const StripCrop *crop = strip->data->crop;
  if (crop->left > 0 || crop->right > 0 || crop->top > 0 || crop->bottom > 0) {
    return true;
  }

  return false;
}

static bool seq_input_have_to_preprocess(const RenderData *context,
                                         Strip *strip,
                                         float /*timeline_frame*/)
{
  float mul;

  if (context && context->is_proxy_render) {
    return false;
  }

  if ((strip->flag & (SEQ_FILTERY | SEQ_FLIPX | SEQ_FLIPY | SEQ_MAKE_FLOAT)) ||
      sequencer_use_crop(strip) || sequencer_use_transform(strip))
  {
    return true;
  }

  mul = strip->mul;

  if (strip->blend_mode == STRIP_BLEND_REPLACE) {
    mul *= strip->blend_opacity / 100.0f;
  }

  if (mul != 1.0f) {
    return true;
  }

  if (strip->sat != 1.0f) {
    return true;
  }

  if (strip->modifiers.first) {
    return true;
  }

  return false;
}

/**
 * Effect, mask and scene in strip input strips are rendered in preview resolution.
 * They are already down-scaled. #input_preprocess() does not expect this to happen.
 * Other strip types are rendered with original media resolution, unless proxies are
 * enabled for them. With proxies `is_proxy_image` will be set correctly to true.
 */
static bool seq_need_scale_to_render_size(const Strip *strip, bool is_proxy_image)
{
  if (is_proxy_image) {
    return false;
  }
  if (strip->is_effect() || strip->type == STRIP_TYPE_MASK || strip->type == STRIP_TYPE_META ||
      (strip->type == STRIP_TYPE_SCENE && ((strip->flag & SEQ_SCENE_STRIPS) != 0)))
  {
    return false;
  }
  return true;
}

static float3x3 calc_strip_transform_matrix(const Scene *scene,
                                            const Strip *strip,
                                            const int in_x,
                                            const int in_y,
                                            const int out_x,
                                            const int out_y,
                                            const float image_scale_factor,
                                            const float preview_scale_factor)
{
  const StripTransform *transform = strip->data->transform;

  /* This value is intentionally kept as integer. Otherwise images with odd dimensions would
   * be translated to center of canvas by non-integer value, which would cause it to be
   * interpolated. Interpolation with 0 user defined translation is unwanted behavior. */
  const int3 image_center_offs((out_x - in_x) / 2, (out_y - in_y) / 2, 0);

  const float2 translation(transform->xofs * preview_scale_factor,
                           transform->yofs * preview_scale_factor);
  const float rotation = transform->rotation;
  const float2 scale(transform->scale_x * image_scale_factor,
                     transform->scale_y * image_scale_factor);

  const float2 origin = image_transform_origin_get(scene, strip);
  const float2 pivot(in_x * origin[0], in_y * origin[1]);

  const float3x3 matrix = math::from_loc_rot_scale<float3x3>(
      translation + float2(image_center_offs), rotation, scale);
  const float3x3 mat_pivot = math::from_origin_transform(matrix, pivot);
  return mat_pivot;
}

static void sequencer_image_crop_init(const Strip *strip,
                                      const ImBuf *in,
                                      float crop_scale_factor,
                                      rctf *r_crop)
{
  const StripCrop *c = strip->data->crop;
  const int left = c->left * crop_scale_factor;
  const int right = c->right * crop_scale_factor;
  const int top = c->top * crop_scale_factor;
  const int bottom = c->bottom * crop_scale_factor;

  BLI_rctf_init(r_crop, left, in->x - right, bottom, in->y - top);
}

static bool is_strip_covering_screen(const RenderData *context, const Strip *strip)
{
  /* The check is done by checking whether all corners of viewport fit inside
   * of the transformed strip. If they do not, the strip does not cover
   * whole screen. */
  float x0 = 0.0f;
  float y0 = 0.0f;
  float x1 = float(context->rectx);
  float y1 = float(context->recty);
  float x_aspect = context->scene->r.xasp / context->scene->r.yasp;
  if (x_aspect != 1.0f) {
    float xmid = (x0 + x1) * 0.5f;
    x0 = xmid - (xmid - x0) * x_aspect;
    x1 = xmid + (x1 - xmid) * x_aspect;
  }
  StripScreenQuad quad = get_strip_screen_quad(context, strip);
  if (quad.is_empty()) {
    return false; /* Strip is zero size. */
  }
  StripScreenQuad screen{float2(x0, y0), float2(x1, y0), float2(x0, y1), float2(x1, y1)};

  return is_quad_a_inside_b(screen, quad);
}

/* Automatic filter:
 * - No scale, no rotation and non-fractional position: nearest.
 * - Scale up by more than 2x: cubic mitchell.
 * - Scale down by more than 2x: box.
 * - Otherwise: bilinear. */
static eIMBInterpolationFilterMode get_auto_filter(const StripTransform *transform)
{
  const float sx = fabsf(transform->scale_x);
  const float sy = fabsf(transform->scale_y);
  if (sx > 2.0f && sy > 2.0f) {
    return IMB_FILTER_CUBIC_MITCHELL;
  }
  if (sx < 0.5f && sy < 0.5f) {
    return IMB_FILTER_BOX;
  }
  const float px = transform->xofs;
  const float py = transform->yofs;
  const float rot = transform->rotation;
  if (sx == 1.0f && sy == 1.0f && roundf(px) == px && roundf(py) == py && rot == 0.0f) {
    return IMB_FILTER_NEAREST;
  }
  return IMB_FILTER_BILINEAR;
}

static void sequencer_preprocess_transform_crop(ImBuf *in,
                                                ImBuf *out,
                                                const RenderData *context,
                                                Strip *strip,
                                                const float3x3 &matrix,
                                                const bool scale_crop_values,
                                                const float preview_scale_factor)
{
  /* Proxy image is smaller, so crop values must be corrected by proxy scale factor.
   * Proxy scale factor always matches preview_scale_factor. */
  rctf source_crop;
  const float crop_scale_factor = scale_crop_values ? preview_scale_factor : 1.0f;
  sequencer_image_crop_init(strip, in, crop_scale_factor, &source_crop);

  const StripTransform *transform = strip->data->transform;
  eIMBInterpolationFilterMode filter = IMB_FILTER_NEAREST;
  switch (transform->filter) {
    case SEQ_TRANSFORM_FILTER_AUTO:
      filter = get_auto_filter(strip->data->transform);
      break;
    case SEQ_TRANSFORM_FILTER_NEAREST:
      filter = IMB_FILTER_NEAREST;
      break;
    case SEQ_TRANSFORM_FILTER_BILINEAR:
      filter = IMB_FILTER_BILINEAR;
      break;
    case SEQ_TRANSFORM_FILTER_CUBIC_BSPLINE:
      filter = IMB_FILTER_CUBIC_BSPLINE;
      break;
    case SEQ_TRANSFORM_FILTER_CUBIC_MITCHELL:
      filter = IMB_FILTER_CUBIC_MITCHELL;
      break;
    case SEQ_TRANSFORM_FILTER_BOX:
      filter = IMB_FILTER_BOX;
      break;
  }

  IMB_transform(in, out, IMB_TRANSFORM_MODE_CROP_SRC, filter, matrix, &source_crop);

  if (is_strip_covering_screen(context, strip)) {
    out->planes = in->planes;
  }
  else {
    /* Strip is not covering full viewport, which means areas with transparency
     * are introduced for sure. */
    out->planes = R_IMF_PLANES_RGBA;
  }
}

static void multiply_ibuf(ImBuf *ibuf, const float fmul, const bool multiply_alpha)
{
  BLI_assert_msg(ibuf->channels == 0 || ibuf->channels == 4,
                 "Sequencer only supports 4 channel images");
  const size_t pixel_count = IMB_get_pixel_count(ibuf);
  if (ibuf->byte_buffer.data != nullptr) {
    threading::parallel_for(IndexRange(pixel_count), 64 * 1024, [&](IndexRange range) {
      uchar *ptr = ibuf->byte_buffer.data + range.first() * 4;
      const int imul = int(256.0f * fmul);
      for ([[maybe_unused]] const int64_t i : range) {
        ptr[0] = min_ii((imul * ptr[0]) >> 8, 255);
        ptr[1] = min_ii((imul * ptr[1]) >> 8, 255);
        ptr[2] = min_ii((imul * ptr[2]) >> 8, 255);
        if (multiply_alpha) {
          ptr[3] = min_ii((imul * ptr[3]) >> 8, 255);
        }
        ptr += 4;
      }
    });
  }

  if (ibuf->float_buffer.data != nullptr) {
    threading::parallel_for(IndexRange(pixel_count), 64 * 1024, [&](IndexRange range) {
      float *ptr = ibuf->float_buffer.data + range.first() * 4;
      for ([[maybe_unused]] const int64_t i : range) {
        ptr[0] *= fmul;
        ptr[1] *= fmul;
        ptr[2] *= fmul;
        if (multiply_alpha) {
          ptr[3] *= fmul;
        }
        ptr += 4;
      }
    });
  }
}

static ImBuf *input_preprocess(const RenderData *context,
                               SeqRenderState *state,
                               Strip *strip,
                               float timeline_frame,
                               ImBuf *ibuf,
                               const bool is_proxy_image)
{
  Scene *scene = context->scene;

  /* Deinterlace. */
  if ((strip->flag & SEQ_FILTERY) && !ELEM(strip->type, STRIP_TYPE_MOVIE, STRIP_TYPE_MOVIECLIP)) {
    ibuf = IMB_makeSingleUser(ibuf);
    IMB_filtery(ibuf);
  }

  if (strip->sat != 1.0f) {
    ibuf = IMB_makeSingleUser(ibuf);
    IMB_saturation(ibuf, strip->sat);
  }

  if (strip->flag & SEQ_MAKE_FLOAT) {
    if (!ibuf->float_buffer.data) {
      ibuf = IMB_makeSingleUser(ibuf);
      seq_imbuf_to_sequencer_space(scene, ibuf, true);
    }

    if (ibuf->byte_buffer.data) {
      IMB_free_byte_pixels(ibuf);
    }
  }

  float mul = strip->mul;
  if (strip->blend_mode == STRIP_BLEND_REPLACE) {
    mul *= strip->blend_opacity / 100.0f;
  }

  if (mul != 1.0f) {
    ibuf = IMB_makeSingleUser(ibuf);
    const bool multiply_alpha = (strip->flag & SEQ_MULTIPLY_ALPHA);
    multiply_ibuf(ibuf, mul, multiply_alpha);
  }

  const float preview_scale_factor = get_render_scale_factor(*context);
  const bool do_scale_to_render_size = seq_need_scale_to_render_size(strip, is_proxy_image);
  const float image_scale_factor = do_scale_to_render_size ? preview_scale_factor : 1.0f;

  float2 modifier_translation = float2(0, 0);
  if (strip->modifiers.first) {
    ibuf = IMB_makeSingleUser(ibuf);
    float3x3 matrix = calc_strip_transform_matrix(scene,
                                                  strip,
                                                  ibuf->x,
                                                  ibuf->y,
                                                  context->rectx,
                                                  context->recty,
                                                  image_scale_factor,
                                                  preview_scale_factor);
    ModifierApplyContext mod_context(*context, *state, *strip, matrix, ibuf);
    modifier_apply_stack(mod_context, timeline_frame);
    modifier_translation = mod_context.result_translation;
  }

  if (sequencer_use_crop(strip) || sequencer_use_transform(strip) || context->rectx != ibuf->x ||
      context->recty != ibuf->y || modifier_translation != float2(0, 0))
  {
    const int x = context->rectx;
    const int y = context->recty;
    ImBuf *transformed_ibuf = IMB_allocImBuf(
        x, y, 32, ibuf->float_buffer.data ? IB_float_data : IB_byte_data);

    /* Note: calculate matrix again; modifiers can actually change the image size. */
    float3x3 matrix = calc_strip_transform_matrix(scene,
                                                  strip,
                                                  ibuf->x,
                                                  ibuf->y,
                                                  context->rectx,
                                                  context->recty,
                                                  image_scale_factor,
                                                  preview_scale_factor);
    matrix *= math::from_location<float3x3>(modifier_translation);
    matrix = math::invert(matrix);
    sequencer_preprocess_transform_crop(ibuf,
                                        transformed_ibuf,
                                        context,
                                        strip,
                                        matrix,
                                        !do_scale_to_render_size,
                                        preview_scale_factor);

    seq_imbuf_assign_spaces(scene, transformed_ibuf);
    IMB_metadata_copy(transformed_ibuf, ibuf);
    IMB_freeImBuf(ibuf);
    ibuf = transformed_ibuf;
  }

  if (strip->flag & SEQ_FLIPX) {
    ibuf = IMB_makeSingleUser(ibuf);
    IMB_flipx(ibuf);
  }

  if (strip->flag & SEQ_FLIPY) {
    ibuf = IMB_makeSingleUser(ibuf);
    IMB_flipy(ibuf);
  }

  return ibuf;
}

static ImBuf *seq_render_preprocess_ibuf(const RenderData *context,
                                         SeqRenderState *state,
                                         Strip *strip,
                                         ImBuf *ibuf,
                                         float timeline_frame,
                                         bool use_preprocess,
                                         const bool is_proxy_image)
{
  if (context->is_proxy_render == false &&
      (ibuf->x != context->rectx || ibuf->y != context->recty))
  {
    use_preprocess = true;
  }

  /* Proxies and non-generator effect strips are not stored in cache. */
  const bool is_effect_with_inputs = strip->is_effect() &&
                                     (effect_get_num_inputs(strip->type) != 0 ||
                                      (strip->type == STRIP_TYPE_ADJUSTMENT));
  if (!is_proxy_image && !is_effect_with_inputs) {
    Scene *orig_scene = prefetch_get_original_scene(context);
    if (orig_scene->ed->cache_flag & SEQ_CACHE_STORE_RAW) {
      source_image_cache_put(context, strip, timeline_frame, ibuf);
    }
  }

  if (use_preprocess) {
    ibuf = input_preprocess(context, state, strip, timeline_frame, ibuf, is_proxy_image);
  }

  return ibuf;
}

static ImBuf *seq_render_effect_strip_impl(const RenderData *context,
                                           SeqRenderState *state,
                                           Strip *strip,
                                           float timeline_frame)
{
  Scene *scene = context->scene;
  int i;
  EffectHandle sh = strip_effect_handle_get(strip);
  ImBuf *ibuf[2];
  Strip *input[2];
  ImBuf *out = nullptr;

  ibuf[0] = ibuf[1] = nullptr;

  input[0] = strip->input1;
  input[1] = strip->input2;

  if (!sh.execute) {
    /* effect not supported in this version... */
    out = IMB_allocImBuf(context->rectx, context->recty, 32, IB_byte_data);
    return out;
  }

  float fac = effect_fader_calc(scene, strip, timeline_frame);

  StripEarlyOut early_out = sh.early_out(strip, fac);

  switch (early_out) {
    case StripEarlyOut::NoInput:
      out = sh.execute(context, state, strip, timeline_frame, fac, nullptr, nullptr);
      break;
    case StripEarlyOut::DoEffect:
      for (i = 0; i < 2; i++) {
        /* Speed effect requires time remapping of `timeline_frame` for input(s). */
        if (input[0] && strip->type == STRIP_TYPE_SPEED) {
          float target_frame = strip_speed_effect_target_frame_get(
              scene, strip, timeline_frame, i);

          /* Only convert to int when interpolation is not used. */
          SpeedControlVars *s = reinterpret_cast<SpeedControlVars *>(strip->effectdata);
          if ((s->flags & SEQ_SPEED_USE_INTERPOLATION) != 0) {
            target_frame = std::floor(target_frame);
          }

          ibuf[i] = seq_render_strip(context, state, input[0], target_frame);
        }
        else { /* Other effects. */
          if (input[i]) {
            ibuf[i] = seq_render_strip(context, state, input[i], timeline_frame);
          }
        }
      }

      if (ibuf[0] && (ibuf[1] || effect_get_num_inputs(strip->type) == 1)) {
        out = sh.execute(context, state, strip, timeline_frame, fac, ibuf[0], ibuf[1]);
      }
      break;
    case StripEarlyOut::UseInput1:
      if (input[0]) {
        out = seq_render_strip(context, state, input[0], timeline_frame);
      }
      break;
    case StripEarlyOut::UseInput2:
      if (input[1]) {
        out = seq_render_strip(context, state, input[1], timeline_frame);
      }
      break;
  }

  for (i = 0; i < 2; i++) {
    IMB_freeImBuf(ibuf[i]);
  }

  if (out == nullptr) {
    out = IMB_allocImBuf(context->rectx, context->recty, 32, IB_byte_data);
  }

  return out;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Individual strip rendering functions
 * \{ */

static void convert_multilayer_ibuf(ImBuf *ibuf)
{
  /* Load the combined/RGB layer, if this is a multi-layer image. */
  BKE_movieclip_convert_multilayer_ibuf(ibuf);

  /* Combined layer might be non-4 channels, however the rest
   * of sequencer assumes RGBA everywhere. Convert to 4 channel if needed. */
  if (ibuf->float_buffer.data != nullptr && ibuf->channels != 4) {
    float *dst = MEM_malloc_arrayN<float>(4 * size_t(ibuf->x) * size_t(ibuf->y), __func__);
    IMB_buffer_float_from_float_threaded(dst,
                                         ibuf->float_buffer.data,
                                         ibuf->channels,
                                         IB_PROFILE_LINEAR_RGB,
                                         IB_PROFILE_LINEAR_RGB,
                                         false,
                                         ibuf->x,
                                         ibuf->y,
                                         ibuf->x,
                                         ibuf->x);
    IMB_assign_float_buffer(ibuf, dst, IB_TAKE_OWNERSHIP);
    ibuf->channels = 4;
  }
}

/**
 * Render individual view for multi-view or single (default view) for mono-view.
 */
static ImBuf *seq_render_image_strip_view(const RenderData *context,
                                          Strip *strip,
                                          char *filepath,
                                          char *prefix,
                                          const char *ext,
                                          int view_id)
{
  ImBuf *ibuf = nullptr;

  int flag = IB_byte_data | IB_metadata | IB_multilayer;
  if (strip->alpha_mode == SEQ_ALPHA_PREMUL) {
    flag |= IB_alphamode_premul;
  }

  if (prefix[0] == '\0') {
    ibuf = IMB_load_image_from_filepath(filepath, flag, strip->data->colorspace_settings.name);
  }
  else {
    char filepath_view[FILE_MAX];
    BKE_scene_multiview_view_prefix_get(context->scene, filepath, prefix, &ext);
    seq_multiview_name(context->scene, view_id, prefix, ext, filepath_view, FILE_MAX);
    ibuf = IMB_load_image_from_filepath(
        filepath_view, flag, strip->data->colorspace_settings.name);
  }

  if (ibuf == nullptr) {
    return nullptr;
  }
  convert_multilayer_ibuf(ibuf);

  /* We don't need both (speed reasons)! */
  if (ibuf->float_buffer.data != nullptr && ibuf->byte_buffer.data != nullptr) {
    IMB_free_byte_pixels(ibuf);
  }

  /* All sequencer color is done in SRGB space, linear gives odd cross-fades. */
  seq_imbuf_to_sequencer_space(context->scene, ibuf, false);

  return ibuf;
}

static bool seq_image_strip_is_multiview_render(Scene *scene,
                                                Strip *strip,
                                                int totfiles,
                                                const char *filepath,
                                                char *r_prefix,
                                                const char *r_ext)
{
  if (totfiles > 1) {
    BKE_scene_multiview_view_prefix_get(scene, filepath, r_prefix, &r_ext);
    if (r_prefix[0] == '\0') {
      return false;
    }
  }
  else {
    r_prefix[0] = '\0';
  }

  return (strip->flag & SEQ_USE_VIEWS) != 0 && (scene->r.scemode & R_MULTIVIEW) != 0;
}

static ImBuf *create_missing_media_image(const RenderData *context, int width, int height)
{
  if (context->ignore_missing_media) {
    return nullptr;
  }
  if (context->scene == nullptr || context->scene->ed == nullptr ||
      (context->scene->ed->show_missing_media_flag & SEQ_EDIT_SHOW_MISSING_MEDIA) == 0)
  {
    return nullptr;
  }

  ImBuf *ibuf = IMB_allocImBuf(max_ii(width, 1), max_ii(height, 1), 32, IB_byte_data);
  float col[4] = {0.85f, 0.0f, 0.75f, 1.0f};
  IMB_rectfill(ibuf, col);
  return ibuf;
}

static ImBuf *seq_render_image_strip(const RenderData *context,
                                     SeqRenderState *state,
                                     Strip *strip,
                                     int timeline_frame,
                                     bool *r_is_proxy_image)
{
  char filepath[FILE_MAX];
  const char *ext = nullptr;
  char prefix[FILE_MAX];
  ImBuf *ibuf = nullptr;

  StripElem *s_elem = render_give_stripelem(context->scene, strip, timeline_frame);
  if (s_elem == nullptr) {
    return nullptr;
  }

  BLI_path_join(filepath, sizeof(filepath), strip->data->dirpath, s_elem->filename);
  BLI_path_abs(filepath, ID_BLEND_PATH_FROM_GLOBAL(&context->scene->id));

  /* Try to get a proxy image. */
  ibuf = seq_proxy_fetch(context, strip, timeline_frame);
  if (ibuf != nullptr) {
    *r_is_proxy_image = true;
    return ibuf;
  }

  /* Proxy not found, render original. */
  const int totfiles = seq_num_files(context->scene, strip->views_format, true);
  bool is_multiview_render = seq_image_strip_is_multiview_render(
      context->scene, strip, totfiles, filepath, prefix, ext);

  if (is_multiview_render) {
    int totviews = BKE_scene_multiview_num_views_get(&context->scene->r);
    ImBuf **ibufs_arr = MEM_calloc_arrayN<ImBuf *>(totviews, "Sequence Image Views Imbufs");

    for (int view_id = 0; view_id < totfiles; view_id++) {
      ibufs_arr[view_id] = seq_render_image_strip_view(
          context, strip, filepath, prefix, ext, view_id);
    }

    if (ibufs_arr[0] == nullptr) {
      return nullptr;
    }

    if (strip->views_format == R_IMF_VIEWS_STEREO_3D) {
      IMB_ImBufFromStereo3d(strip->stereo3d_format, ibufs_arr[0], &ibufs_arr[0], &ibufs_arr[1]);
    }

    for (int view_id = 0; view_id < totviews; view_id++) {
      RenderData localcontext = *context;
      localcontext.view_id = view_id;

      if (view_id != context->view_id) {
        ibufs_arr[view_id] = seq_render_preprocess_ibuf(
            &localcontext, state, strip, ibufs_arr[view_id], timeline_frame, true, false);
      }
    }

    /* Return the original requested ImBuf. */
    ibuf = ibufs_arr[context->view_id];

    /* Remove the others (decrease their refcount). */
    for (int view_id = 0; view_id < totviews; view_id++) {
      if (ibufs_arr[view_id] != ibuf) {
        IMB_freeImBuf(ibufs_arr[view_id]);
      }
    }

    MEM_freeN(ibufs_arr);
  }
  else {
    ibuf = seq_render_image_strip_view(context, strip, filepath, prefix, ext, context->view_id);
  }

  media_presence_set_missing(context->scene, strip, ibuf == nullptr);
  if (ibuf == nullptr) {
    return create_missing_media_image(context, s_elem->orig_width, s_elem->orig_height);
  }

  s_elem->orig_width = ibuf->x;
  s_elem->orig_height = ibuf->y;

  return ibuf;
}

static ImBuf *seq_render_movie_strip_custom_file_proxy(const RenderData *context,
                                                       Strip *strip,
                                                       int timeline_frame)
{
  char filepath[PROXY_MAXFILE];
  StripProxy *proxy = strip->data->proxy;

  if (proxy->anim == nullptr) {
    if (seq_proxy_get_custom_file_filepath(strip, filepath, context->view_id)) {
      /* Sequencer takes care of colorspace conversion of the result. The input is the best to be
       * kept unchanged for the performance reasons. */
      proxy->anim = openanim(
          filepath, IB_byte_data, 0, true, strip->data->colorspace_settings.name);
    }
    if (proxy->anim == nullptr) {
      return nullptr;
    }
  }

  int frameno = round_fl_to_int(give_frame_index(context->scene, strip, timeline_frame)) +
                strip->anim_startofs;
  return MOV_decode_frame(proxy->anim, frameno, IMB_TC_NONE, IMB_PROXY_NONE);
}

static IMB_Timecode_Type seq_render_movie_strip_timecode_get(Strip *strip)
{
  bool use_timecodes = (strip->flag & SEQ_USE_PROXY) != 0;
  if (!use_timecodes) {
    return IMB_TC_NONE;
  }
  return IMB_Timecode_Type(strip->data->proxy ? IMB_Timecode_Type(strip->data->proxy->tc) :
                                                IMB_TC_NONE);
}

/**
 * Render individual view for multi-view or single (default view) for mono-view.
 */
static ImBuf *seq_render_movie_strip_view(const RenderData *context,
                                          Strip *strip,
                                          float timeline_frame,
                                          StripAnim *sanim,
                                          bool *r_is_proxy_image)
{
  ImBuf *ibuf = nullptr;
  IMB_Proxy_Size psize = rendersize_to_proxysize(context->preview_render_size);
  const int frame_index = round_fl_to_int(give_frame_index(context->scene, strip, timeline_frame));

  if (can_use_proxy(context, strip, psize)) {
    /* Try to get a proxy image.
     * Movie proxies are handled by ImBuf module with exception of `custom file` setting. */
    if (context->scene->ed->proxy_storage != SEQ_EDIT_PROXY_DIR_STORAGE &&
        strip->data->proxy->storage & SEQ_STORAGE_PROXY_CUSTOM_FILE)
    {
      ibuf = seq_render_movie_strip_custom_file_proxy(context, strip, timeline_frame);
    }
    else {
      ibuf = MOV_decode_frame(sanim->anim,
                              frame_index + strip->anim_startofs,
                              seq_render_movie_strip_timecode_get(strip),
                              psize);
    }

    if (ibuf != nullptr) {
      *r_is_proxy_image = true;
    }
  }

  /* Fetching for requested proxy size failed, try fetching the original instead. */
  if (ibuf == nullptr) {
    ibuf = MOV_decode_frame(sanim->anim,
                            frame_index + strip->anim_startofs,
                            seq_render_movie_strip_timecode_get(strip),
                            IMB_PROXY_NONE);
  }
  if (ibuf == nullptr) {
    return nullptr;
  }

  seq_imbuf_to_sequencer_space(context->scene, ibuf, false);

  /* We don't need both (speed reasons)! */
  if (ibuf->float_buffer.data != nullptr && ibuf->byte_buffer.data != nullptr) {
    IMB_free_byte_pixels(ibuf);
  }

  return ibuf;
}

static ImBuf *seq_render_movie_strip(const RenderData *context,
                                     SeqRenderState *state,
                                     Strip *strip,
                                     float timeline_frame,
                                     bool *r_is_proxy_image)
{
  /* Load all the videos. */
  strip_open_anim_file(context->scene, strip, false);

  ImBuf *ibuf = nullptr;
  StripAnim *sanim = static_cast<StripAnim *>(strip->anims.first);
  const int totfiles = seq_num_files(context->scene, strip->views_format, true);
  bool is_multiview_render = (strip->flag & SEQ_USE_VIEWS) != 0 &&
                             (context->scene->r.scemode & R_MULTIVIEW) != 0 &&
                             BLI_listbase_count_is_equal_to(&strip->anims, totfiles);

  if (is_multiview_render) {
    ImBuf **ibuf_arr;
    int totviews = BKE_scene_multiview_num_views_get(&context->scene->r);
    ibuf_arr = MEM_calloc_arrayN<ImBuf *>(totviews, "Sequence Image Views Imbufs");
    int ibuf_view_id;

    for (ibuf_view_id = 0, sanim = static_cast<StripAnim *>(strip->anims.first); sanim;
         sanim = sanim->next, ibuf_view_id++)
    {
      if (sanim->anim) {
        ibuf_arr[ibuf_view_id] = seq_render_movie_strip_view(
            context, strip, timeline_frame, sanim, r_is_proxy_image);
      }
    }

    if (strip->views_format == R_IMF_VIEWS_STEREO_3D) {
      if (ibuf_arr[0] == nullptr) {
        /* Probably proxy hasn't been created yet. */
        MEM_freeN(ibuf_arr);
        return nullptr;
      }

      IMB_ImBufFromStereo3d(strip->stereo3d_format, ibuf_arr[0], &ibuf_arr[0], &ibuf_arr[1]);
    }

    for (int view_id = 0; view_id < totviews; view_id++) {
      RenderData localcontext = *context;
      localcontext.view_id = view_id;

      if (view_id != context->view_id && ibuf_arr[view_id]) {
        ibuf_arr[view_id] = seq_render_preprocess_ibuf(
            &localcontext, state, strip, ibuf_arr[view_id], timeline_frame, true, false);
      }
    }

    /* Return the original requested ImBuf. */
    ibuf = ibuf_arr[context->view_id];

    /* Remove the others (decrease their refcount). */
    for (int view_id = 0; view_id < totviews; view_id++) {
      if (ibuf_arr[view_id] != ibuf) {
        IMB_freeImBuf(ibuf_arr[view_id]);
      }
    }

    MEM_freeN(ibuf_arr);
  }
  else {
    ibuf = seq_render_movie_strip_view(context, strip, timeline_frame, sanim, r_is_proxy_image);
  }

  media_presence_set_missing(context->scene, strip, ibuf == nullptr);
  if (ibuf == nullptr) {
    return create_missing_media_image(
        context, strip->data->stripdata->orig_width, strip->data->stripdata->orig_height);
  }

  if (*r_is_proxy_image == false) {
    if (sanim && sanim->anim) {
      strip->data->stripdata->orig_fps = MOV_get_fps(sanim->anim);
    }
    strip->data->stripdata->orig_width = ibuf->x;
    strip->data->stripdata->orig_height = ibuf->y;
  }

  return ibuf;
}

static ImBuf *seq_get_movieclip_ibuf(Strip *strip, MovieClipUser user)
{
  ImBuf *ibuf = nullptr;
  float tloc[2], tscale, tangle;
  if (strip->clip_flag & SEQ_MOVIECLIP_RENDER_STABILIZED) {
    ibuf = BKE_movieclip_get_stable_ibuf(strip->clip, &user, 0, tloc, &tscale, &tangle);
  }
  else {
    ibuf = BKE_movieclip_get_ibuf_flag(
        strip->clip, &user, strip->clip->flag, MOVIECLIP_CACHE_SKIP);
  }
  return ibuf;
}

static ImBuf *seq_render_movieclip_strip(const RenderData *context,
                                         Strip *strip,
                                         float frame_index,
                                         bool *r_is_proxy_image)
{
  ImBuf *ibuf = nullptr;
  MovieClipUser user = *DNA_struct_default_get(MovieClipUser);
  IMB_Proxy_Size psize = rendersize_to_proxysize(context->preview_render_size);

  if (!strip->clip) {
    return nullptr;
  }

  BKE_movieclip_user_set_frame(&user,
                               frame_index + strip->anim_startofs + strip->clip->start_frame);

  user.render_size = MCLIP_PROXY_RENDER_SIZE_FULL;
  switch (psize) {
    case IMB_PROXY_NONE:
      user.render_size = MCLIP_PROXY_RENDER_SIZE_FULL;
      break;
    case IMB_PROXY_100:
      user.render_size = MCLIP_PROXY_RENDER_SIZE_100;
      break;
    case IMB_PROXY_75:
      user.render_size = MCLIP_PROXY_RENDER_SIZE_75;
      break;
    case IMB_PROXY_50:
      user.render_size = MCLIP_PROXY_RENDER_SIZE_50;
      break;
    case IMB_PROXY_25:
      user.render_size = MCLIP_PROXY_RENDER_SIZE_25;
      break;
  }

  if (strip->clip_flag & SEQ_MOVIECLIP_RENDER_UNDISTORTED) {
    user.render_flag |= MCLIP_PROXY_RENDER_UNDISTORT;
  }

  /* Try to get a proxy image. */
  ibuf = seq_get_movieclip_ibuf(strip, user);

  /* If clip doesn't use proxies, it will fall back to full size render of original file. */
  if (ibuf != nullptr && psize != IMB_PROXY_NONE && BKE_movieclip_proxy_enabled(strip->clip)) {
    *r_is_proxy_image = true;
  }

  /* If proxy is not found, grab full-size frame. */
  if (ibuf == nullptr) {
    user.render_flag |= MCLIP_PROXY_RENDER_USE_FALLBACK_RENDER;
    ibuf = seq_get_movieclip_ibuf(strip, user);
  }

  return ibuf;
}

ImBuf *seq_render_mask(Depsgraph *depsgraph,
                       int width,
                       int height,
                       const Mask *mask,
                       float frame_index,
                       bool make_float)
{
  /* TODO: add option to rasterize to alpha imbuf? */
  ImBuf *ibuf = nullptr;
  float *maskbuf;
  int i;

  if (!mask) {
    return nullptr;
  }

  AnimData *adt;
  Mask *mask_temp;
  MaskRasterHandle *mr_handle;

  mask_temp = (Mask *)BKE_id_copy_ex(
      nullptr, &mask->id, nullptr, LIB_ID_COPY_LOCALIZE | LIB_ID_COPY_NO_ANIMDATA);

  BKE_mask_evaluate(mask_temp, mask->sfra + frame_index, true);

  /* anim-data */
  adt = BKE_animdata_from_id(&mask->id);
  const AnimationEvalContext anim_eval_context = BKE_animsys_eval_context_construct(
      depsgraph, mask->sfra + frame_index);
  BKE_animsys_evaluate_animdata(&mask_temp->id, adt, &anim_eval_context, ADT_RECALC_ANIM, false);

  maskbuf = MEM_malloc_arrayN<float>(size_t(width) * size_t(height), __func__);

  mr_handle = BKE_maskrasterize_handle_new();

  BKE_maskrasterize_handle_init(mr_handle, mask_temp, width, height, true, true, true);

  BKE_id_free(nullptr, &mask_temp->id);

  BKE_maskrasterize_buffer(mr_handle, width, height, maskbuf);

  BKE_maskrasterize_handle_free(mr_handle);

  if (make_float) {
    /* pixels */
    const float *fp_src;
    float *fp_dst;

    ibuf = IMB_allocImBuf(width, height, 32, IB_float_data | IB_uninitialized_pixels);

    fp_src = maskbuf;
    fp_dst = ibuf->float_buffer.data;
    i = width * height;
    while (--i) {
      fp_dst[0] = fp_dst[1] = fp_dst[2] = *fp_src;
      fp_dst[3] = 1.0f;

      fp_src += 1;
      fp_dst += 4;
    }
  }
  else {
    /* pixels */
    const float *fp_src;
    uchar *ub_dst;

    ibuf = IMB_allocImBuf(width, height, 32, IB_byte_data | IB_uninitialized_pixels);

    fp_src = maskbuf;
    ub_dst = ibuf->byte_buffer.data;
    i = width * height;
    while (--i) {
      ub_dst[0] = ub_dst[1] = ub_dst[2] = uchar(*fp_src * 255.0f); /* already clamped */
      ub_dst[3] = 255;

      fp_src += 1;
      ub_dst += 4;
    }
  }

  MEM_freeN(maskbuf);

  return ibuf;
}

static ImBuf *seq_render_mask_strip(const RenderData *context, Strip *strip, float frame_index)
{
  bool make_float = (strip->flag & SEQ_MAKE_FLOAT) != 0;

  return seq_render_mask(
      context->depsgraph, context->rectx, context->recty, strip->mask, frame_index, make_float);
}

static Depsgraph *get_depsgraph_for_scene_strip(Main *bmain, Scene *scene, ViewLayer *view_layer)
{
  Depsgraph *depsgraph = scene->runtime->sequencer.depsgraph;
  if (!depsgraph) {
    /* Create a new depsgraph for the sequencer preview. Use viewport evaluation, because this
     * depsgraph is not used during final render. */
    scene->runtime->sequencer.depsgraph = DEG_graph_new(
        bmain, scene, view_layer, DAG_EVAL_VIEWPORT);
    depsgraph = scene->runtime->sequencer.depsgraph;
    DEG_debug_name_set(depsgraph, "SEQ_SCENE_STRIP");
  }

  if (DEG_get_input_view_layer(depsgraph) != view_layer) {
    DEG_graph_replace_owners(depsgraph, bmain, scene, view_layer);
    DEG_graph_tag_relations_update(depsgraph);
  }

  return depsgraph;
}

static ImBuf *seq_render_scene_strip_ex(const RenderData *context,
                                        Strip *strip,
                                        float frame_index,
                                        float timeline_frame)
{
  ImBuf *ibuf = nullptr;
  Object *camera;

  /* Old info:
   * Hack! This function can be called from do_render_seq(), in that case
   * the strip->scene can already have a Render initialized with same name,
   * so we have to use a default name. (compositor uses scene name to
   * find render).
   * However, when called from within the UI (image preview in sequencer)
   * we do want to use scene Render, that way the render result is defined
   * for display in render/image-window
   *
   * Hmm, don't see, why we can't do that all the time,
   * and since G.is_rendering is uhm, gone... (Peter)
   */

  /* New info:
   * Using the same name for the renders works just fine as the do_render_seq()
   * render is not used while the scene strips are rendered.
   *
   * However rendering from UI (through sequencer_preview_area_draw) can crash in
   * very many cases since other renders (material preview, an actual render etc.)
   * can be started while this sequence preview render is running. The only proper
   * solution is to make the sequencer preview render a proper job, which can be
   * stopped when needed. This would also give a nice progress bar for the preview
   * space so that users know there's something happening.
   *
   * As a result the active scene now only uses OpenGL rendering for the sequencer
   * preview. This is far from nice, but is the only way to prevent crashes at this
   * time.
   *
   * -jahka
   */

  Scene *scene = strip->scene;
  BLI_assert(scene != nullptr);

  /* Prevent rendering scene recursively. */
  if (scene == context->scene) {
    return nullptr;
  }

  const bool is_rendering = G.is_rendering;
  const bool is_preview = !context->for_render && (context->scene->r.seq_prev_type) != OB_RENDER;
  const bool use_gpencil = (strip->flag & SEQ_SCENE_NO_ANNOTATION) == 0;
  double frame = double(scene->r.sfra) + double(frame_index) + double(strip->anim_startofs);

#if 0 /* UNUSED */
  bool have_seq = (scene->r.scemode & R_DOSEQ) && scene->ed && scene->ed->seqbase.first;
#endif
  const bool have_comp = (scene->r.scemode & R_DOCOMP) && scene->compositing_node_group;

  ViewLayer *view_layer = BKE_view_layer_default_render(scene);
  Depsgraph *depsgraph = get_depsgraph_for_scene_strip(context->bmain, scene, view_layer);

  BKE_scene_frame_set(scene, frame);

  if (strip->scene_camera) {
    camera = strip->scene_camera;
  }
  else {
    BKE_scene_camera_switch_update(scene);
    camera = scene->camera;
  }

  if (have_comp == false && camera == nullptr) {
    return nullptr;
  }

  /* Prevent eternal loop. */
  scene->r.scemode &= ~R_DOSEQ;

  /* Temporarily disable camera switching to enforce using `camera`. */
  scene->r.mode |= R_NO_CAMERA_SWITCH;

  if (view3d_fn && is_preview && camera) {
    char err_out[256] = "unknown";
    int width, height;
    BKE_render_resolution(&scene->r, false, &width, &height);
    const char *viewname = BKE_scene_multiview_render_view_name_get(&scene->r, context->view_id);

    uint draw_flags = V3D_OFSDRAW_NONE;
    draw_flags |= (use_gpencil) ? V3D_OFSDRAW_SHOW_ANNOTATION : 0;
    draw_flags |= (context->scene->r.seq_flag & R_SEQ_OVERRIDE_SCENE_SETTINGS) ?
                      V3D_OFSDRAW_OVERRIDE_SCENE_SETTINGS :
                      0;

    /* for old scene this can be uninitialized,
     * should probably be added to do_versions at some point if the functionality stays */
    if (context->scene->r.seq_prev_type == 0) {
      context->scene->r.seq_prev_type = 3 /* == OB_SOLID */;
    }

    /* opengl offscreen render */
    BKE_scene_graph_update_for_newframe(depsgraph);
    Object *camera_eval = DEG_get_evaluated(depsgraph, camera);
    Scene *scene_eval = DEG_get_evaluated_scene(depsgraph);
    ibuf = view3d_fn(
        /* set for OpenGL render (nullptr when scrubbing) */
        depsgraph,
        scene_eval,
        &context->scene->display.shading,
        eDrawType(context->scene->r.seq_prev_type),
        camera_eval,
        width,
        height,
        IB_byte_data,
        eV3DOffscreenDrawFlag(draw_flags),
        scene->r.alphamode,
        viewname,
        context->gpu_offscreen,
        context->gpu_viewport,
        err_out);
    if (ibuf == nullptr) {
      fprintf(stderr, "seq_render_scene_strip failed to get opengl buffer: %s\n", err_out);
    }
  }
  else {
    Render *re = RE_GetSceneRender(scene);
    const int totviews = BKE_scene_multiview_num_views_get(&scene->r);
    ImBuf **ibufs_arr;

    /*
     * XXX: this if can be removed when sequence preview rendering uses the job system
     *
     * Disable rendered preview for sequencer while rendering - invoked render job will
     * conflict with already running render
     *
     * When rendering from command line renderer is called from main thread, in this
     * case it's always safe to render scene here
     */

    if (is_preview && (is_rendering && !G.background)) {
      return ibuf;
    }

    ibufs_arr = MEM_calloc_arrayN<ImBuf *>(totviews, "Sequence Image Views Imbufs");

    if (re == nullptr) {
      re = RE_NewSceneRender(scene);
    }

    const float subframe = frame - floorf(frame);

    RE_RenderFrame(re,
                   context->bmain,
                   scene,
                   have_comp ? nullptr : view_layer,
                   camera,
                   floorf(frame),
                   subframe,
                   false);

    /* restore previous state after it was toggled on & off by RE_RenderFrame */
    G.is_rendering = is_rendering;

    for (int view_id = 0; view_id < totviews; view_id++) {
      RenderData localcontext = *context;
      RenderResult rres;

      localcontext.view_id = view_id;

      RE_AcquireResultImage(re, &rres, view_id);

      /* TODO: Share the pixel data with the original image buffer from the render result using
       * implicit sharing. */
      if (rres.ibuf && rres.ibuf->float_buffer.data) {
        ibufs_arr[view_id] = IMB_allocImBuf(rres.rectx, rres.recty, 32, IB_float_data);
        memcpy(ibufs_arr[view_id]->float_buffer.data,
               rres.ibuf->float_buffer.data,
               sizeof(float[4]) * rres.rectx * rres.recty);

        /* float buffers in the sequencer are not linear */
        seq_imbuf_to_sequencer_space(context->scene, ibufs_arr[view_id], false);
      }
      else if (rres.ibuf && rres.ibuf->byte_buffer.data) {
        ibufs_arr[view_id] = IMB_allocImBuf(rres.rectx, rres.recty, 32, IB_byte_data);
        memcpy(ibufs_arr[view_id]->byte_buffer.data,
               rres.ibuf->byte_buffer.data,
               4 * rres.rectx * rres.recty);
      }
      else {
        ibufs_arr[view_id] = IMB_allocImBuf(rres.rectx, rres.recty, 32, IB_byte_data);
      }

      if (view_id != context->view_id) {
        Scene *orig_scene = prefetch_get_original_scene(context);
        if (orig_scene->ed->cache_flag & SEQ_CACHE_STORE_RAW) {
          source_image_cache_put(&localcontext, strip, timeline_frame, ibufs_arr[view_id]);
        }
      }

      RE_ReleaseResultImage(re);
    }

    /* return the original requested ImBuf */
    ibuf = ibufs_arr[context->view_id];

    /* "remove" the others (decrease their refcount) */
    for (int view_id = 0; view_id < totviews; view_id++) {
      if (ibufs_arr[view_id] != ibuf) {
        IMB_freeImBuf(ibufs_arr[view_id]);
      }
    }
    MEM_freeN(ibufs_arr);
  }

  return ibuf;
}

static ImBuf *seq_render_scene_strip(const RenderData *context,
                                     Strip *strip,
                                     float frame_index,
                                     float timeline_frame)
{
  if (strip->scene == nullptr) {
    return create_missing_media_image(context, context->rectx, context->recty);
  }

  Scene *scene = strip->scene;

  struct {
    int scemode;
    int timeline_frame;
    float subframe;
    int mode;
  } orig_data;

  /* Store state. */
  orig_data.scemode = scene->r.scemode;
  orig_data.timeline_frame = scene->r.cfra;
  orig_data.subframe = scene->r.subframe;
  orig_data.mode = scene->r.mode;

  const bool is_frame_update = (orig_data.timeline_frame != scene->r.cfra) ||
                               (orig_data.subframe != scene->r.subframe);

  ImBuf *ibuf = seq_render_scene_strip_ex(context, strip, frame_index, timeline_frame);

  /* Restore state. */
  scene->r.scemode = orig_data.scemode;
  scene->r.cfra = orig_data.timeline_frame;
  scene->r.subframe = orig_data.subframe;
  scene->r.mode &= orig_data.mode | ~R_NO_CAMERA_SWITCH;

  Depsgraph *depsgraph = BKE_scene_get_depsgraph(scene, BKE_view_layer_default_render(scene));
  if (is_frame_update && (depsgraph != nullptr)) {
    BKE_scene_graph_update_for_newframe(depsgraph);
  }

  return ibuf;
}

/**
 * Used for meta-strips & scenes with #SEQ_SCENE_STRIPS flag set.
 */
static ImBuf *do_render_strip_seqbase(const RenderData *context,
                                      SeqRenderState *state,
                                      Strip *strip,
                                      float frame_index)
{
  ImBuf *ibuf = nullptr;
  ListBase *seqbase = nullptr;
  ListBase *channels = nullptr;
  int offset;

  seqbase = get_seqbase_from_strip(strip, &channels, &offset);

  if (seqbase && !BLI_listbase_is_empty(seqbase)) {

    frame_index += offset;

    if (strip->flag & SEQ_SCENE_STRIPS && strip->scene) {
      BKE_animsys_evaluate_all_animation(context->bmain, context->depsgraph, frame_index);
    }

    intra_frame_cache_set_cur_frame(
        context->scene, frame_index, context->view_id, context->rectx, context->recty);
    ibuf = seq_render_strip_stack(context,
                                  state,
                                  channels,
                                  seqbase,
                                  /* scene strips don't have their start taken into account */
                                  frame_index,
                                  0);
  }

  return ibuf;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Strip Stack Rendering Functions
 * \{ */

static ImBuf *do_render_strip_uncached(const RenderData *context,
                                       SeqRenderState *state,
                                       Strip *strip,
                                       float timeline_frame,
                                       bool *r_is_proxy_image)
{
  ImBuf *ibuf = nullptr;
  float frame_index = give_frame_index(context->scene, strip, timeline_frame);
  if (strip->type == STRIP_TYPE_META) {
    ibuf = do_render_strip_seqbase(context, state, strip, frame_index);
  }
  else if (strip->type == STRIP_TYPE_SCENE) {
    /* Recursive check. */
    if (BLI_linklist_index(state->scene_parents, strip->scene) == -1) {
      LinkNode scene_parent{};
      scene_parent.next = state->scene_parents;
      scene_parent.link = context->scene;
      state->scene_parents = &scene_parent;
      /* End check. */

      if (strip->flag & SEQ_SCENE_STRIPS) {
        if (strip->scene && (context->scene != strip->scene)) {
          /* Use the Scene sequence-strip's scene for the context when rendering the
           * scene's sequences (necessary for multi-cam selector among others). */
          RenderData local_context = *context;
          local_context.scene = strip->scene;
          local_context.skip_cache = true;

          ibuf = do_render_strip_seqbase(&local_context, state, strip, frame_index);
        }
      }
      else {
        /* scene can be nullptr after deletions */
        ibuf = seq_render_scene_strip(context, strip, frame_index, timeline_frame);
      }

      /* Step back in the recursive check list. */
      state->scene_parents = state->scene_parents->next;
    }
  }
  else if (strip->is_effect()) {
    ibuf = seq_render_effect_strip_impl(context, state, strip, timeline_frame);
  }
  else if (strip->type == STRIP_TYPE_IMAGE) {
    ibuf = seq_render_image_strip(context, state, strip, timeline_frame, r_is_proxy_image);
  }
  else if (strip->type == STRIP_TYPE_MOVIE) {
    ibuf = seq_render_movie_strip(context, state, strip, timeline_frame, r_is_proxy_image);
  }
  else if (strip->type == STRIP_TYPE_MOVIECLIP) {
    ibuf = seq_render_movieclip_strip(
        context, strip, round_fl_to_int(frame_index), r_is_proxy_image);

    if (ibuf) {
      /* duplicate frame so movie cache wouldn't be confused by sequencer's stuff */
      ImBuf *i = IMB_dupImBuf(ibuf);
      IMB_freeImBuf(ibuf);
      ibuf = i;

      if (ibuf->float_buffer.data) {
        seq_imbuf_to_sequencer_space(context->scene, ibuf, false);
      }
    }
  }
  else if (strip->type == STRIP_TYPE_MASK) {
    /* ibuf is always new */
    ibuf = seq_render_mask_strip(context, strip, frame_index);
  }

  if (ibuf) {
    seq_imbuf_assign_spaces(context->scene, ibuf);
  }

  return ibuf;
}

ImBuf *seq_render_strip(const RenderData *context,
                        SeqRenderState *state,
                        Strip *strip,
                        float timeline_frame)
{
  bool use_preprocess = false;
  bool is_proxy_image = false;

  ImBuf *ibuf = intra_frame_cache_get_preprocessed(context->scene, strip);
  if (ibuf != nullptr) {
    return ibuf;
  }

  /* Proxies are not stored in cache. */
  if (!can_use_proxy(context, strip, rendersize_to_proxysize(context->preview_render_size))) {
    ibuf = source_image_cache_get(context, strip, timeline_frame);
  }

  if (ibuf == nullptr) {
    ibuf = do_render_strip_uncached(context, state, strip, timeline_frame, &is_proxy_image);
  }

  if (ibuf) {
    use_preprocess = seq_input_have_to_preprocess(context, strip, timeline_frame);
    ibuf = seq_render_preprocess_ibuf(
        context, state, strip, ibuf, timeline_frame, use_preprocess, is_proxy_image);
    intra_frame_cache_put_preprocessed(context->scene, strip, ibuf);
  }

  if (ibuf == nullptr) {
    ibuf = IMB_allocImBuf(context->rectx, context->recty, 32, IB_byte_data);
    seq_imbuf_assign_spaces(context->scene, ibuf);
  }

  return ibuf;
}

static bool seq_must_swap_input_in_blend_mode(Strip *strip)
{
  return ELEM(strip->blend_mode, STRIP_BLEND_ALPHAOVER, STRIP_BLEND_ALPHAUNDER);
}

static StripEarlyOut strip_get_early_out_for_blend_mode(Strip *strip)
{
  EffectHandle sh = strip_blend_mode_handle_get(strip);
  float fac = strip->blend_opacity / 100.0f;
  StripEarlyOut early_out = sh.early_out(strip, fac);

  if (ELEM(early_out, StripEarlyOut::DoEffect, StripEarlyOut::NoInput)) {
    return early_out;
  }

  if (seq_must_swap_input_in_blend_mode(strip)) {
    if (early_out == StripEarlyOut::UseInput2) {
      return StripEarlyOut::UseInput1;
    }
    if (early_out == StripEarlyOut::UseInput1) {
      return StripEarlyOut::UseInput2;
    }
  }
  return early_out;
}

static ImBuf *seq_render_strip_stack_apply_effect(

    const RenderData *context,
    SeqRenderState *state,
    Strip *strip,
    float timeline_frame,
    ImBuf *ibuf1,
    ImBuf *ibuf2)
{
  ImBuf *out;
  EffectHandle sh = strip_blend_mode_handle_get(strip);
  BLI_assert(sh.execute != nullptr);
  float fac = strip->blend_opacity / 100.0f;
  int swap_input = seq_must_swap_input_in_blend_mode(strip);

  if (swap_input) {
    out = sh.execute(context, state, strip, timeline_frame, fac, ibuf2, ibuf1);
  }
  else {
    out = sh.execute(context, state, strip, timeline_frame, fac, ibuf1, ibuf2);
  }

  return out;
}

static bool is_opaque_alpha_over(const Strip *strip)
{
  if (strip->blend_mode != STRIP_BLEND_ALPHAOVER) {
    return false;
  }
  if (strip->blend_opacity < 100.0f) {
    return false;
  }
  if (strip->mul < 1.0f && (strip->flag & SEQ_MULTIPLY_ALPHA) != 0) {
    return false;
  }
  LISTBASE_FOREACH (StripModifierData *, smd, &strip->modifiers) {
    /* Assume result is not opaque if there is an enabled Mask or Compositor modifiers, which could
     * introduce alpha. */
    if ((smd->flag & STRIP_MODIFIER_FLAG_MUTE) == 0 &&
        ELEM(smd->type, eSeqModifierType_Mask, eSeqModifierType_Compositor))
    {
      return false;
    }
  }
  return true;
}

static ImBuf *seq_render_strip_stack(const RenderData *context,
                                     SeqRenderState *state,
                                     ListBase *channels,
                                     ListBase *seqbasep,
                                     float timeline_frame,
                                     int chanshown)
{
  Vector<Strip *> strips = seq_shown_strips_get(
      context->scene, channels, seqbasep, timeline_frame, chanshown);
  if (strips.is_empty()) {
    return nullptr;
  }

  OpaqueQuadTracker opaques;

  int64_t i;
  ImBuf *out = nullptr;
  for (i = strips.size() - 1; i >= 0; i--) {
    Strip *strip = strips[i];

    out = intra_frame_cache_get_composite(context->scene, strip);
    if (out) {
      break;
    }
    if (strip->blend_mode == STRIP_BLEND_REPLACE) {
      out = seq_render_strip(context, state, strip, timeline_frame);
      break;
    }

    StripEarlyOut early_out = strip_get_early_out_for_blend_mode(strip);

    if (early_out == StripEarlyOut::DoEffect && opaques.is_occluded(context, strip, i)) {
      early_out = StripEarlyOut::UseInput1;
    }

    /* "Alpha over" is default for all strips, and it can be optimized in some cases:
     * - If the whole image has no transparency, there's no need to do actual blending.
     * - Likewise, if we are at the bottom of the stack; the input can be used as-is.
     * - If we are rendering a strip that is known to be opaque, we mark it as an occluder,
     *   so that strips below can check if they are completely hidden. */
    if (out == nullptr && early_out == StripEarlyOut::DoEffect && is_opaque_alpha_over(strip)) {
      ImBuf *test = seq_render_strip(context, state, strip, timeline_frame);
      if (ELEM(test->planes, R_IMF_PLANES_BW, R_IMF_PLANES_RGB) || i == 0) {
        early_out = StripEarlyOut::UseInput2;
      }
      else {
        early_out = StripEarlyOut::DoEffect;
      }
      /* Free the image. It is stored in cache, so this doesn't affect performance. */
      IMB_freeImBuf(test);

      /* Check whether the raw (before preprocessing, which can add alpha) strip content
       * was opaque. */
      ImBuf *ibuf_raw = source_image_cache_get(context, strip, timeline_frame);
      if (ibuf_raw != nullptr) {
        if (ibuf_raw->planes != R_IMF_PLANES_RGBA) {
          opaques.add_occluder(context, strip, i);
        }
        IMB_freeImBuf(ibuf_raw);
      }
    }

    switch (early_out) {
      case StripEarlyOut::NoInput:
      case StripEarlyOut::UseInput2:
        out = seq_render_strip(context, state, strip, timeline_frame);
        break;
      case StripEarlyOut::UseInput1:
        if (i == 0) {
          out = IMB_allocImBuf(context->rectx, context->recty, 32, IB_byte_data);
          seq_imbuf_assign_spaces(context->scene, out);
        }
        break;
      case StripEarlyOut::DoEffect:
        if (i == 0) {
          /* This is an effect at the bottom of the stack, so one of the inputs does not exist yet:
           * create one that is transparent black. Extra optimization for an alpha over strip at
           * the bottom, we can just return it instead of blending with black. */
          ImBuf *ibuf2 = seq_render_strip(context, state, strip, timeline_frame);
          const bool use_float = ibuf2 && ibuf2->float_buffer.data;
          ImBuf *ibuf1 = IMB_allocImBuf(
              context->rectx, context->recty, 32, use_float ? IB_float_data : IB_byte_data);
          seq_imbuf_assign_spaces(context->scene, ibuf1);

          out = seq_render_strip_stack_apply_effect(
              context, state, strip, timeline_frame, ibuf1, ibuf2);
          IMB_metadata_copy(out, ibuf2);

          intra_frame_cache_put_composite(context->scene, strip, out);

          IMB_freeImBuf(ibuf1);
          IMB_freeImBuf(ibuf2);
        }
        break;
    }

    if (out) {
      break;
    }
  }

  i++;
  for (; i < strips.size(); i++) {
    Strip *strip = strips[i];

    if (opaques.is_occluded(context, strip, i)) {
      continue;
    }

    if (strip_get_early_out_for_blend_mode(strip) == StripEarlyOut::DoEffect) {
      ImBuf *ibuf1 = out;
      ImBuf *ibuf2 = seq_render_strip(context, state, strip, timeline_frame);

      out = seq_render_strip_stack_apply_effect(
          context, state, strip, timeline_frame, ibuf1, ibuf2);

      IMB_freeImBuf(ibuf1);
      IMB_freeImBuf(ibuf2);
    }

    intra_frame_cache_put_composite(context->scene, strip, out);
  }

  return out;
}

ImBuf *render_give_ibuf(const RenderData *context, float timeline_frame, int chanshown)
{
  Scene *scene = context->scene;
  Editing *ed = editing_get(scene);
  ListBase *seqbasep;
  ListBase *channels;

  if (ed == nullptr) {
    return nullptr;
  }

  if ((chanshown < 0) && !BLI_listbase_is_empty(&ed->metastack)) {
    int count = BLI_listbase_count(&ed->metastack);
    count = max_ii(count + chanshown, 0);
    MetaStack *ms = static_cast<MetaStack *>(BLI_findlink(&ed->metastack, count));
    seqbasep = &ms->old_strip->seqbase;
    channels = &ms->old_strip->channels;
    chanshown = 0;
  }
  else {
    seqbasep = ed->current_strips();
    channels = ed->current_channels();
  }

  intra_frame_cache_set_cur_frame(
      scene, timeline_frame, context->view_id, context->rectx, context->recty);

  Scene *orig_scene = prefetch_get_original_scene(context);
  ImBuf *out = nullptr;
  if (!context->skip_cache && !context->is_proxy_render) {
    out = final_image_cache_get(orig_scene, timeline_frame, context->view_id, chanshown);
  }

  Vector<Strip *> strips = seq_shown_strips_get(
      scene, channels, seqbasep, timeline_frame, chanshown);

  /* Make sure we only keep the `anim` data for strips that are in view. */
  relations_free_all_anim_ibufs(context->scene, timeline_frame);

  SeqRenderState state;

  if (!strips.is_empty() && !out) {
    std::scoped_lock lock(seq_render_mutex);
    /* Try to make space before we add any new frames to the cache if it is full.
     * If we do this after we have added the new cache, we risk removing what we just added. */
    evict_caches_if_full(orig_scene);

    out = seq_render_strip_stack(context, &state, channels, seqbasep, timeline_frame, chanshown);

    if (out && (orig_scene->ed->cache_flag & SEQ_CACHE_STORE_FINAL_OUT) && !context->skip_cache &&
        !context->is_proxy_render)
    {
      final_image_cache_put(orig_scene, timeline_frame, context->view_id, chanshown, out);
    }
  }

  seq_prefetch_start(context, timeline_frame);

  return out;
}

ImBuf *seq_render_give_ibuf_seqbase(const RenderData *context,
                                    SeqRenderState *state,
                                    float timeline_frame,
                                    int chan_shown,
                                    ListBase *channels,
                                    ListBase *seqbasep)
{

  return seq_render_strip_stack(context, state, channels, seqbasep, timeline_frame, chan_shown);
}

ImBuf *render_give_ibuf_direct(const RenderData *context, float timeline_frame, Strip *strip)
{
  SeqRenderState state;

  intra_frame_cache_set_cur_frame(
      context->scene, timeline_frame, context->view_id, context->rectx, context->recty);
  ImBuf *ibuf = seq_render_strip(context, &state, strip, timeline_frame);
  return ibuf;
}

bool render_is_muted(const ListBase *channels, const Strip *strip)
{
  SeqTimelineChannel *channel = channel_get_by_index(channels, strip->channel);
  return strip->flag & SEQ_MUTE || channel_is_muted(channel);
}

/** \} */

float get_render_scale_factor(eSpaceSeq_Proxy_RenderSize render_size, short scene_render_scale)
{
  return render_size == SEQ_RENDER_SIZE_SCENE ? scene_render_scale / 100.0f :
                                                rendersize_to_scale_factor(render_size);
}

float get_render_scale_factor(const RenderData &context)
{
  return get_render_scale_factor(context.preview_render_size, context.scene->r.size);
}

}  // namespace blender::seq
