/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 * SPDX-FileCopyrightText: 2003-2026 Blender Authors
 * SPDX-FileCopyrightText: 2005-2006 Peter Schlaile <peter [at] schlaile [dot] de>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sequencer
 */

#include "MEM_guardedalloc.h"

#include "DNA_mask_types.h"
#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"
#include "DNA_space_types.h"
#include "DNA_world_types.h"

#include "BLI_listbase.hh"
#include "BLI_math_geom_c.hh"
#include "BLI_math_matrix.hh"
#include "BLI_path_utils.hh"
#include "BLI_rect.hh"
#include "BLI_task.hh"

#include "BKE_anim_data.hh"
#include "BKE_animsys.hh"
#include "BKE_global.hh"
#include "BKE_image.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_mask.hh"
#include "BKE_movieclip.hh"
#include "BKE_scene.hh"
#include "BKE_scene_runtime.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"
#include "DEG_depsgraph_debug.hh"
#include "DEG_depsgraph_query.hh"

#include "DRW_engine.hh"

#include "GPU_context.hh"

#include "IMB_colormanagement.hh"
#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"
#include "IMB_metadata.hh"

#include "PRF_profile.hh"

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

#include "WM_api.hh"

#include "cache/final_image_cache.hh"
#include "cache/intra_frame_cache.hh"
#include "cache/source_image_cache.hh"
#include "effects/effects.hh"
#include "intern/movie_read.hh"
#include "modifiers/modifier.hh"
#include "multiview.hh"
#include "prefetch.hh"
#include "proxy.hh"
#include "render.hh"
#include "utils.hh"

#include <algorithm>

namespace blender::seq {

static SeqResult seq_render_strip_stack(const RenderData *context,
                                        SeqRenderState *state,
                                        ListBaseT<SeqTimelineChannel> *channels,
                                        ListBaseT<Strip> *seqbasep,
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
  if (ibuf->float_data() != nullptr) {
    IMB_colormanagement_assign_float_colorspace(ibuf, scene->sequencer_colorspace_settings.name);
  }
}

static void ensure_ibuf_is_color_space(ImBuf *ibuf, bool make_float, const char *to_colorspace)
{
  BLI_assert(ibuf != nullptr);
  /* No pixels: nothing to do. */
  if (ibuf->float_data() == nullptr && ibuf->byte_data() == nullptr) {
    return;
  }

  if (ibuf->float_data() == nullptr) {
    /* Input image contains byte pixels. */
    /* Not requested to become float and already in the needed colorspace: nothing to do. */
    const char *from_colorspace = IMB_colormanagement_get_byte_colorspace(ibuf);
    if (!make_float && STREQ(from_colorspace, to_colorspace)) {
      return;
    }

    /* Turn into a float and convert colorspace. */
    IMB_alloc_float_pixels(ibuf, 4, false);
    IMB_colormanagement_transform_byte_to_float(ibuf->float_data_for_write(),
                                                ibuf->byte_data(),
                                                ibuf->x,
                                                ibuf->y,
                                                ibuf->channels,
                                                from_colorspace,
                                                to_colorspace);
    IMB_colormanagement_assign_float_colorspace(ibuf, to_colorspace);
    IMB_free_byte_pixels(ibuf);
  }
  else {
    /* Input image contains float pixels. */
    const char *from_colorspace = IMB_colormanagement_get_float_colorspace(ibuf);
    /* Unknown input color space, can't perform conversion. */
    if (from_colorspace == nullptr || from_colorspace[0] == '\0') {
      return;
    }

    /* Discard byte pixels if there are any. */
    if (ibuf->byte_data() != nullptr) {
      IMB_free_byte_pixels(ibuf);
    }
    /* Note: we do not use predivide to more closely match what
     * compositor does, and to better preserve cases of pure emissive
     * colors (alpha=0, RGB non black). */
    IMB_colormanagement_transform_float(ibuf->float_data_for_write(),
                                        ibuf->x,
                                        ibuf->y,
                                        ibuf->channels,
                                        from_colorspace,
                                        to_colorspace,
                                        false);
    IMB_colormanagement_assign_float_colorspace(ibuf, to_colorspace);
  }
}

void ensure_ibuf_is_sequencer_space(const Scene *scene, ImBuf *ibuf, bool make_float)
{
  const char *to_colorspace = scene->sequencer_colorspace_settings.name;
  ensure_ibuf_is_color_space(ibuf, make_float, to_colorspace);
}

void ensure_ibuf_is_linear_space(ImBuf *ibuf, bool make_float)
{
  /* Not requested to make float, and only have byte pixels: do nothing. */
  if (!make_float && !ibuf->float_data()) {
    return;
  }

  const char *to_colorspace = IMB_colormanagement_role_colorspace_name_get(
      COLOR_ROLE_SCENE_LINEAR);
  ensure_ibuf_is_color_space(ibuf, make_float, to_colorspace);
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
                            Render *render,
                            RenderData *r_context)
{
  r_context->bmain = bmain;
  r_context->depsgraph = depsgraph;
  r_context->scene = scene;
  r_context->rectx = rectx;
  r_context->recty = recty;
  r_context->preview_render_size = preview_render_size;
  r_context->ignore_missing_media = false;
  r_context->render = render;
  r_context->motion_blur_samples = 0;
  r_context->motion_blur_shutter = 0;
  r_context->skip_cache = false;
  r_context->view_id = 0;
  r_context->gpu_offscreen = nullptr;
  r_context->gpu_viewport = nullptr;
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

StripScreenQuad get_strip_screen_quad(const RenderData *context, const Strip *strip)
{
  Scene *scene = context->scene;
  const int x = context->rectx;
  const int y = context->recty;
  const float2 offset{x * 0.5f, y * 0.5f};

  Array<float2> quad = image_transform_quad_get(scene, strip);
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

static bool seq_input_have_to_preprocess(const Strip *strip)
{
  float mul;

  if ((strip->flag & (SEQ_DEINTERLACE | SEQ_FLIPX | SEQ_FLIPY | SEQ_MAKE_FLOAT)) ||
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
 * Effect (except color), mask and scene in strip input strips are rendered in preview resolution.
 * They are already down-scaled. #input_preprocess() does not expect this to happen.
 * Other strip types are rendered with original media resolution, unless proxies are
 * enabled for them. With proxies `is_proxy_image` will be set correctly to true.
 */
static bool seq_need_scale_to_render_size(const Strip *strip, bool is_proxy_image)
{
  if (is_proxy_image) {
    return false;
  }
  if ((strip->is_effect() && strip->type != STRIP_TYPE_COLOR) || strip->type == STRIP_TYPE_MASK ||
      strip->type == STRIP_TYPE_META ||
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
    out->color_mode = in->color_mode;
  }
  else {
    /* Strip is not covering full viewport, which means areas with transparency
     * are introduced for sure. */
    out->color_mode = ImColorMode::RGBA;
  }
}

static void multiply_ibuf(ImBuf *ibuf, const float fmul, const bool multiply_alpha)
{
  BLI_assert_msg(ibuf->channels == 0 || ibuf->channels == 4,
                 "Sequencer only supports 4 channel images");
  const size_t pixel_count = IMB_get_pixel_count(ibuf);
  if (uchar *byte_data = ibuf->byte_data_for_write()) {
    threading::parallel_for(IndexRange(pixel_count), 64 * 1024, [&](IndexRange range) {
      uchar *ptr = byte_data + range.first() * 4;
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

  if (float *float_data = ibuf->float_data_for_write()) {
    threading::parallel_for(IndexRange(pixel_count), 64 * 1024, [&](IndexRange range) {
      float *ptr = float_data + range.first() * 4;
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

static SeqResult input_preprocess(const RenderData *context,
                                  SeqRenderState *state,
                                  Strip *strip,
                                  float timeline_frame,
                                  const SeqResult &input,
                                  const bool is_proxy_image)
{
  PRF_scope_with_name("SeqPreprocess", ProfileCategory::Draw);

  BLI_assert(input.is_valid());

  SeqResult result = input;

  Scene *scene = context->scene;

  /* Deinterlace. */
  if ((strip->flag & SEQ_DEINTERLACE) &&
      !ELEM(strip->type, STRIP_TYPE_MOVIE, STRIP_TYPE_MOVIECLIP))
  {
    PRF_scope_with_name("SeqStripDeinterlace", ProfileCategory::Draw);
    result.image = IMB_makeSingleUser(result.image);
    IMB_filtery(result.image);
  }

  const bool make_float = strip->flag & SEQ_MAKE_FLOAT;

  if (strip->sat != 1.0f) {
    PRF_scope_with_name("SeqStripSaturation", ProfileCategory::Draw);
    result.image = IMB_makeSingleUser(result.image);
    ensure_ibuf_is_sequencer_space(scene, result.image, make_float);
    IMB_saturation(result.image, strip->sat);
  }

  if (make_float) {
    PRF_scope_with_name("SeqStripMakeFloat", ProfileCategory::Draw);
    if (!result.image->float_data()) {
      result.image = IMB_makeSingleUser(result.image);
      ensure_ibuf_is_sequencer_space(scene, result.image, true);
    }
    if (result.image->byte_data()) {
      IMB_free_byte_pixels(result.image);
    }
  }

  float mul = strip->mul;
  if (strip->blend_mode == STRIP_BLEND_REPLACE) {
    mul *= strip->blend_opacity / 100.0f;
  }

  if (mul != 1.0f) {
    PRF_scope_with_name("SeqStripMultiply", ProfileCategory::Draw);
    result.image = IMB_makeSingleUser(result.image);
    ensure_ibuf_is_sequencer_space(scene, result.image, make_float);
    const bool multiply_alpha = (strip->flag & SEQ_MULTIPLY_ALPHA);
    multiply_ibuf(result.image, mul, multiply_alpha);
    if (multiply_alpha && mul < 1.0f) {
      result.image->color_mode = ImColorMode::RGBA;
    }
  }

  const float preview_scale_factor = get_render_scale_factor(*context);
  const bool do_scale_to_render_size = seq_need_scale_to_render_size(strip, is_proxy_image);
  const float image_scale_factor = do_scale_to_render_size ? preview_scale_factor : 1.0f;

  if (strip->modifiers.first) {
    result.image = IMB_makeSingleUser(result.image);
    float3x3 matrix = calc_strip_transform_matrix(scene,
                                                  strip,
                                                  result.image->x,
                                                  result.image->y,
                                                  context->rectx,
                                                  context->recty,
                                                  image_scale_factor,
                                                  preview_scale_factor);
    float3x3 matrix_comp = calc_strip_transform_matrix(
        scene, strip, 0, 0, 0, 0, image_scale_factor, preview_scale_factor);
    matrix_comp = math::invert(matrix_comp);
    ModifierApplyContext mod_context(
        *context, *state, *strip, matrix, matrix_comp, timeline_frame, result);
    modifier_apply_stack(mod_context);
  }

  /* After everything above is done but before transform is applied,
   * remember whether the image was opaque. */
  result.is_opaque_before_transform = !result.image->can_contain_alpha();

  if (sequencer_use_crop(strip) || sequencer_use_transform(strip) ||
      context->rectx != result.image->x || context->recty != result.image->y ||
      (strip->is_effect() && image_scale_factor != 1.0f) || result.translation != float2(0, 0))
  {
    PRF_scope_with_name("SeqStripTransform", ProfileCategory::Draw);

    const int x = context->rectx;
    const int y = context->recty;
    ImBuf *transformed_ibuf = IMB_allocImBuf(
        x, y, result.image->float_data() ? ImBufFlags::FloatData : ImBufFlags::ByteData);

    /* Note: calculate matrix again; modifiers can actually change the image size. */
    float3x3 matrix = calc_strip_transform_matrix(scene,
                                                  strip,
                                                  result.image->x,
                                                  result.image->y,
                                                  context->rectx,
                                                  context->recty,
                                                  image_scale_factor,
                                                  preview_scale_factor);
    matrix *= math::from_location<float3x3>(result.translation);
    matrix = math::invert(matrix);
    sequencer_preprocess_transform_crop(result.image,
                                        transformed_ibuf,
                                        context,
                                        strip,
                                        matrix,
                                        !do_scale_to_render_size,
                                        preview_scale_factor);
    transformed_ibuf->byte_buffer.colorspace = result.image->byte_buffer.colorspace;
    transformed_ibuf->float_buffer.colorspace = result.image->float_buffer.colorspace;
    IMB_metadata_copy(transformed_ibuf, result.image);
    IMB_freeImBuf(result.image);
    result.image = transformed_ibuf;
  }

  if (strip->flag & SEQ_FLIPX) {
    PRF_scope_with_name("SeqStripFlipX", ProfileCategory::Draw);
    result.image = IMB_makeSingleUser(result.image);
    IMB_flipx(result.image);
  }

  if (strip->flag & SEQ_FLIPY) {
    PRF_scope_with_name("SeqStripFlipY", ProfileCategory::Draw);
    result.image = IMB_makeSingleUser(result.image);
    IMB_flipy(result.image);
  }

  return result;
}

static SeqResult seq_render_preprocess_ibuf(const RenderData *context,
                                            SeqRenderState *state,
                                            Strip *strip,
                                            const SeqResult &input,
                                            float timeline_frame,
                                            bool use_preprocess,
                                            const bool is_proxy_image)
{
  BLI_assert(input.is_valid());
  if (input.image->x != context->rectx || input.image->y != context->recty ||
      input.translation != float2(0, 0))
  {
    use_preprocess = true;
  }

  /* Proxies and non-generator effect strips are not stored in cache. */
  const bool is_effect_with_inputs = strip->is_effect_with_inputs() ||
                                     strip->type == STRIP_TYPE_ADJUSTMENT;
  if (!is_proxy_image && !is_effect_with_inputs) {
    Scene *orig_scene = prefetch_get_original_scene(context);
    if (orig_scene->ed->cache_flag & SEQ_CACHE_STORE_RAW) {
      source_image_cache_put(context, strip, timeline_frame, input);
    }
  }

  if (!use_preprocess) {
    return input;
  }

  return input_preprocess(context, state, strip, timeline_frame, input, is_proxy_image);
}

static SeqResult seq_render_effect_strip_impl(const RenderData *context,
                                              SeqRenderState *state,
                                              Strip *strip,
                                              float timeline_frame)
{
  PRF_scope_with_name("SeqRenderFx", ProfileCategory::Draw);

  Scene *scene = context->scene;
  EffectHandle sh = strip_effect_handle_get(strip);
  SeqResult ibuf[2] = {};
  Strip *input[2] = {strip->input1, strip->input2};
  SeqResult out;

  if (!sh.execute) {
    /* effect not supported in this version... */
    out.image = IMB_allocImBuf(context->rectx, context->recty, ImBufFlags::ByteData);
    return out;
  }

  float fac = effect_fader_calc(scene, strip, timeline_frame);

  StripEarlyOut early_out = sh.early_out(strip, fac);

  switch (early_out) {
    case StripEarlyOut::NoInput:
      out = sh.execute(context, state, strip, timeline_frame, fac, {}, {});
      break;
    case StripEarlyOut::DoEffect:
      for (int i = 0; i < 2; i++) {
        /* Speed effect requires time remapping of `timeline_frame` for input(s). */
        if (input[0] && strip->type == STRIP_TYPE_SPEED) {
          float target_frame = strip_speed_effect_target_frame_get(
              scene, strip, timeline_frame, i);

          /* Only convert to int when interpolation is not used. */
          SpeedControlVars *s = reinterpret_cast<SpeedControlVars *>(strip->effectdata);
          if ((s->flags & SEQ_SPEED_USE_INTERPOLATION) != 0) {
            target_frame = std::floor(target_frame);
          }

          intra_frame_cache_set_cur_frame(context->scene,
                                          target_frame,
                                          context->view_id,
                                          context->rectx,
                                          context->recty,
                                          context->render != nullptr);
          ibuf[i] = seq_render_strip(context, state, input[0], target_frame);
        }
        else { /* Other effects. */
          if (input[i]) {
            ibuf[i] = seq_render_strip(context, state, input[i], timeline_frame);
          }
        }
      }

      if (ibuf[0].is_valid() && (ibuf[1].is_valid() || strip->effect_num_inputs_get() == 1)) {
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

  for (int i = 0; i < 2; i++) {
    IMB_freeImBuf(ibuf[i].image);
  }

  if (!out.is_valid()) {
    out.image = IMB_allocImBuf(context->rectx, context->recty, ImBufFlags::ByteData);
  }

  return out;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Individual strip rendering functions
 * \{ */

void ensure_ibuf_is_rgba(ImBuf *ibuf)
{
  if (ibuf == nullptr) {
    return;
  }

  /* Combined layer might be non-4 channels, however the rest
   * of sequencer assumes RGBA everywhere. Convert to 4 channel if needed. */
  if (ibuf->float_data() != nullptr && ibuf->channels != 4) {
    float *dst = MEM_new_array_uninitialized<float>(4 * size_t(ibuf->x) * size_t(ibuf->y),
                                                    __func__);
    IMB_buffer_float_rgba_from_float(dst, ibuf->float_data(), ibuf->channels, ibuf->x, ibuf->y);
    ibuf->assign_float_data(dst);
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

  ImBufFlags flag = ImBufFlags::ByteData | ImBufFlags::Metadata;
  if (strip->alpha_mode == SEQ_ALPHA_PREMUL) {
    flag |= ImBufFlags::AlphaPremul;
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
  ensure_ibuf_is_rgba(ibuf);

  /* We don't need both (speed reasons)! */
  if (ibuf->float_data() != nullptr && ibuf->byte_data() != nullptr) {
    IMB_free_byte_pixels(ibuf);
  }

  return ibuf;
}

bool seq_image_strip_is_multiview_render(const Scene *scene,
                                         const Strip *strip,
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

  ImBuf *ibuf = IMB_allocImBuf(max_ii(width, 1), max_ii(height, 1), ImBufFlags::ByteData);
  float col[4] = {0.85f, 0.0f, 0.75f, 1.0f};
  IMB_rectfill(ibuf, col);
  return ibuf;
}

static ImBuf *seq_render_image_strip(const RenderData *context,
                                     Strip *strip,
                                     int timeline_frame,
                                     bool *r_is_proxy_image)
{
  PRF_scope_with_name("SeqRenderImage", ProfileCategory::Draw);

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
    Array<ImBuf *> ibufs_arr(totviews, nullptr);

    for (int view_id = 0; view_id < totfiles; view_id++) {
      ibufs_arr[view_id] = seq_render_image_strip_view(
          context, strip, filepath, prefix, ext, view_id);
    }

    if (ibufs_arr[0] == nullptr) {
      return nullptr;
    }

    if (strip->views_format == R_IMF_VIEWS_STEREO_3D) {
      IMB_ImBufFromStereo3d(strip->stereo3d_format,
                            ibufs_arr[0],
                            &ibufs_arr[0],  // NOLINT(readability-container-data-pointer)
                            &ibufs_arr[1]);
    }

    /* Return the requested image; release the others. */
    ibuf = ibufs_arr[context->view_id];
    for (ImBuf *ib : ibufs_arr) {
      if (ib != ibuf) {
        IMB_freeImBuf(ib);
      }
    }
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
          filepath, ImBufFlags::Zero, 0, true, strip->data->colorspace_settings.name);
    }
    if (proxy->anim == nullptr) {
      return nullptr;
    }
  }

  int frameno = round_fl_to_int(give_frame_index(context->scene, strip, timeline_frame)) +
                strip->anim_startofs;
  return MOV_decode_frame(proxy->anim, frameno, IMB_PROXY_NONE);
}

/**
 * Render individual view for multi-view or single (default view) for mono-view.
 */
static ImBuf *seq_render_movie_strip_view(const RenderData *context,
                                          Strip *strip,
                                          float timeline_frame,
                                          MovieReader *reader,
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
      ibuf = MOV_decode_frame(reader, frame_index + strip->anim_startofs, psize);
    }

    if (ibuf != nullptr) {
      *r_is_proxy_image = true;
    }
  }

  /* Fetching for requested proxy size failed, try fetching the original instead. */
  if (ibuf == nullptr) {
    ibuf = MOV_decode_frame(reader, frame_index + strip->anim_startofs, IMB_PROXY_NONE);
  }
  if (ibuf == nullptr) {
    return nullptr;
  }

  /* We don't need both (speed reasons)! */
  if (ibuf->float_data() != nullptr && ibuf->byte_data() != nullptr) {
    IMB_free_byte_pixels(ibuf);
  }

  return ibuf;
}

static ImBuf *seq_render_movie_strip(const RenderData *context,
                                     Strip *strip,
                                     float timeline_frame,
                                     bool *r_is_proxy_image)
{
  PRF_scope_with_name("SeqRenderMovie", ProfileCategory::Draw);

  /* Load all the videos. */
  strip_open_anim_file(context->scene, strip, false);

  ImBuf *ibuf = nullptr;
  MovieReader *first_reader = strip->runtime->movie_reader_get();
  const int totfiles = seq_num_files(context->scene, strip->views_format, true);
  bool is_multiview_render = (strip->flag & SEQ_USE_VIEWS) != 0 &&
                             (context->scene->r.scemode & R_MULTIVIEW) != 0 &&
                             totfiles == strip->runtime->movie_readers.size();

  if (is_multiview_render) {
    int totviews = BKE_scene_multiview_num_views_get(&context->scene->r);
    Array<ImBuf *> ibuf_arr(totviews, nullptr);

    int ibuf_view_id = 0;
    for (MovieReader *reader : strip->runtime->movie_readers) {
      if (reader) {
        ibuf_arr[ibuf_view_id] = seq_render_movie_strip_view(
            context, strip, timeline_frame, reader, r_is_proxy_image);
      }
      ibuf_view_id++;
    }

    if (strip->views_format == R_IMF_VIEWS_STEREO_3D) {
      if (ibuf_arr[0] == nullptr) {
        /* Probably proxy hasn't been created yet. */
        return nullptr;
      }

      IMB_ImBufFromStereo3d(strip->stereo3d_format,
                            ibuf_arr[0],
                            &ibuf_arr[0],  // NOLINT(readability-container-data-pointer)
                            &ibuf_arr[1]);
    }

    /* Return the requested image; release the others. */
    ibuf = ibuf_arr[context->view_id];
    for (ImBuf *ib : ibuf_arr) {
      if (ib != ibuf) {
        IMB_freeImBuf(ib);
      }
    }
  }
  else {
    ibuf = seq_render_movie_strip_view(
        context, strip, timeline_frame, first_reader, r_is_proxy_image);
  }

  media_presence_set_missing(context->scene, strip, ibuf == nullptr);
  if (ibuf == nullptr) {
    return create_missing_media_image(
        context, strip->data->stripdata->orig_width, strip->data->stripdata->orig_height);
  }

  if (*r_is_proxy_image == false) {
    if (first_reader) {
      strip->data->stripdata->orig_fps = MOV_get_fps(first_reader);
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
    ibuf = BKE_movieclip_get_stable_ibuf(
        strip->clip, &user, MovieClipPostprocFlag::None, tloc, &tscale, &tangle);
  }
  else {
    ibuf = BKE_movieclip_get_ibuf_flag(
        strip->clip, &user, MovieClipFlag(strip->clip->flag), MovieClipCacheFlag::SkipCache);
  }
  return ibuf;
}

static ImBuf *seq_render_movieclip_strip(const RenderData *context,
                                         Strip *strip,
                                         float frame_index,
                                         bool *r_is_proxy_image)
{
  PRF_scope_with_name("SeqRenderMovieClip", ProfileCategory::Draw);

  ImBuf *ibuf = nullptr;
  MovieClipUser user = {};
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
  if (!mask) {
    return nullptr;
  }

  Mask *mask_temp = id_cast<Mask *>(
      BKE_id_copy_ex(nullptr, &mask->id, nullptr, LIB_ID_COPY_LOCALIZE | LIB_ID_COPY_NO_ANIMDATA));

  BKE_mask_evaluate(mask_temp, mask->sfra + frame_index, true);

  /* anim-data */
  AnimData *adt = BKE_animdata_from_id(&mask->id);
  const AnimationEvalContext anim_eval_context = BKE_animsys_eval_context_construct(
      depsgraph, mask->sfra + frame_index);
  BKE_animsys_evaluate_animdata(&mask_temp->id, adt, &anim_eval_context, ADT_RECALC_ANIM, false);

  MaskRasterHandle *mr_handle = BKE_maskrasterize_handle_new();

  BKE_maskrasterize_handle_init(mr_handle, mask_temp, width, height, true, true, true);

  BKE_id_free(nullptr, &mask_temp->id);

  /* Evaluate mask over the resulting image. */
  ImBuf *ibuf = IMB_allocImBuf(width,
                               height,
                               (make_float ? ImBufFlags::FloatData : ImBufFlags::ByteData) |
                                   ImBufFlags::UninitializedPixels);
  const float x_inv = 1.0f / float(width);
  const float y_inv = 1.0f / float(height);
  const float x_px_ofs = x_inv * 0.5f;
  const float y_px_ofs = y_inv * 0.5f;
  float *dst_float = ibuf->float_data_for_write();
  uchar *dst_byte = ibuf->byte_data_for_write();
  threading::parallel_for(IndexRange(height), 16, [&](const IndexRange y_range) {
    const int64_t pixel_offset = y_range.first() * width * 4;
    float *ptr_float = dst_float + pixel_offset;
    uchar *ptr_byte = dst_byte + pixel_offset;
    for (int64_t y : y_range) {
      float2 coord;
      coord.y = y * y_inv + y_px_ofs;
      for (int x = 0; x < width; x++) {
        coord.x = x * x_inv + x_px_ofs;
        float value = BKE_maskrasterize_handle_sample(mr_handle, coord);
        if (make_float) {
          ptr_float[0] = ptr_float[1] = ptr_float[2] = value;
          ptr_float[3] = 1.0f;
        }
        else {
          ptr_byte[0] = ptr_byte[1] = ptr_byte[2] = uchar(value * 255.0f);
          ptr_byte[3] = 255;
        }
        ptr_float += 4;
        ptr_byte += 4;
      }
    }
  });

  BKE_maskrasterize_handle_free(mr_handle);

  return ibuf;
}

static ImBuf *seq_render_mask_strip(const RenderData *context, Strip *strip, float frame_index)
{
  PRF_scope_with_name("SeqRenderMask", ProfileCategory::Draw);

  bool make_float = (strip->flag & SEQ_MAKE_FLOAT) != 0;

  return seq_render_mask(
      context->depsgraph, context->rectx, context->recty, strip->mask, frame_index, make_float);
}

static ViewLayer *get_view_layer_for_scene_strip(Scene *scene, const Strip *strip)
{
  if (strip->scene_view_layer_name != nullptr) {
    if (ViewLayer *view_layer = BKE_view_layer_find(scene, strip->scene_view_layer_name)) {
      return view_layer;
    }
  }
  return BKE_view_layer_default_render(scene);
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

  /* Rendering from UI (through sequencer_preview_area_draw) can crash in
   * very many cases since other renders (material preview, an actual render etc.)
   * can be started while this sequence preview render is running. The only proper
   * solution is to make the sequencer preview render a proper job, which can be
   * stopped when needed. This would also give a nice progress bar for the preview
   * space so that users know there's something happening.
   *
   * As a result the active scene now only uses OpenGL rendering for the sequencer
   * preview. This is far from nice, but is the only way to prevent crashes at this
   * time.
   */

  Scene *scene = strip->scene;
  BLI_assert(scene != nullptr);

  /* Prevent rendering scene recursively. */
  if (scene == context->scene) {
    return nullptr;
  }

  const bool is_rendering = G.is_rendering;
  const bool is_preview = !context->render && (context->scene->r.seq_prev_type) != OB_RENDER;
  const bool use_gpencil = (strip->flag & SEQ_SCENE_NO_ANNOTATION) == 0;
  double frame = double(scene->r.sfra) + double(frame_index) + double(strip->anim_startofs);

#if 0 /* UNUSED */
  bool have_seq = (scene->r.scemode & R_DOSEQ) && scene->ed && scene->ed->seqbase.first;
#endif
  const bool have_comp = (scene->r.scemode & R_DOCOMP) && scene->compositing_node_group;

  ViewLayer *view_layer = get_view_layer_for_scene_strip(scene, strip);
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

    const bool use_scene_settings = (context->scene->r.seq_flag & R_SEQ_OVERRIDE_SCENE_SETTINGS) !=
                                    0;

    uint draw_flags = V3D_OFSDRAW_NONE;
    draw_flags |= (use_gpencil) ? V3D_OFSDRAW_SHOW_ANNOTATION : 0;
    draw_flags |= (use_scene_settings) ? (V3D_OFSDRAW_OVERRIDE_SCENE_SETTINGS |
                                          V3D_OFSDRAW_NO_WORLD_BACKGROUND_OVERRIDE) :
                                         0;

    View3DShading scene_shading = context->scene->display.shading;

    if (use_scene_settings) {
      /* Allow to render with the scene world color. */
      if (context->scene->world != nullptr) {
        copy_v3_v3(&scene_shading.background_color[0], &context->scene->world->horr);
      }
      else {
        copy_v3_fl(&scene_shading.background_color[0], 0.0f);
      }
      scene_shading.background_type = V3D_SHADING_BACKGROUND_VIEWPORT;
    }

    /* for old scene this can be uninitialized,
     * should probably be added to do_versions at some point if the functionality stays */
    if (context->scene->r.seq_prev_type == 0) {
      context->scene->r.seq_prev_type = OB_SOLID;
    }

    /* opengl offscreen render */
    BKE_scene_graph_update_for_newframe(depsgraph);
    Object *camera_eval = DEG_get_evaluated(depsgraph, camera);
    Scene *scene_eval = DEG_get_evaluated_scene(depsgraph);
    ibuf = view3d_fn(
        /* set for OpenGL render (nullptr when scrubbing) */
        depsgraph,
        scene_eval,
        &scene_shading,
        eDrawType(context->scene->r.seq_prev_type),
        camera_eval,
        width,
        height,
        ImBufFlags::ByteData,
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

    Array<SeqResult> ibufs_arr(totviews);

    if (re == nullptr) {
      re = RE_NewSceneRender(scene);
    }

    const float subframe = frame - floorf(frame);

    RE_display_share(re, context->render);

    RE_RenderFrame(re,
                   context->bmain,
                   scene,
                   have_comp ? nullptr : view_layer,
                   camera,
                   floorf(frame),
                   subframe,
                   false);

    RE_display_free(re);

    /* restore previous state after it was toggled on & off by RE_RenderFrame */
    G.is_rendering = is_rendering;

    for (int view_id = 0; view_id < totviews; view_id++) {
      RenderData localcontext = *context;
      RenderResult rres;

      localcontext.view_id = view_id;

      RE_AcquireResultImage(re, &rres, view_id);

      if (rres.ibuf && rres.ibuf->float_data()) {
        ibufs_arr[view_id].image = IMB_allocImBuf(rres.rectx, rres.recty, ImBufFlags::Zero);
        ibufs_arr[view_id].image->float_buffer = rres.ibuf->float_buffer;
      }
      else if (rres.ibuf && rres.ibuf->byte_data()) {
        ibufs_arr[view_id].image = IMB_allocImBuf(rres.rectx, rres.recty, ImBufFlags::Zero);
        ibufs_arr[view_id].image->byte_buffer = rres.ibuf->byte_buffer;
      }
      else {
        ibufs_arr[view_id].image = IMB_allocImBuf(rres.rectx, rres.recty, ImBufFlags::ByteData);
      }

      if (view_id != context->view_id) {
        Scene *orig_scene = prefetch_get_original_scene(context);
        if (orig_scene->ed->cache_flag & SEQ_CACHE_STORE_RAW) {
          source_image_cache_put(&localcontext, strip, timeline_frame, ibufs_arr[view_id]);
        }
      }

      RE_ReleaseResultImage(re);
    }

    /* Return the requested image; release the others. */
    ibuf = ibufs_arr[context->view_id].image;
    for (SeqResult &res : ibufs_arr) {
      if (res.image != ibuf) {
        IMB_freeImBuf(res.image);
      }
    }
  }

  return ibuf;
}

static SeqResult seq_render_scene_strip(const RenderData *context,
                                        Strip *strip,
                                        float frame_index,
                                        float timeline_frame)
{
  PRF_scope_with_name("SeqRenderScene", ProfileCategory::Draw);

  SeqResult out;
  if (strip->scene == nullptr) {
    out.image = create_missing_media_image(context, context->rectx, context->recty);
    return out;
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

  out.image = seq_render_scene_strip_ex(context, strip, frame_index, timeline_frame);
  if (out.image && !out.image->can_contain_alpha()) {
    out.is_opaque_before_transform = true;
  }

  /* Restore state. */
  scene->r.scemode = orig_data.scemode;
  scene->r.cfra = orig_data.timeline_frame;
  scene->r.subframe = orig_data.subframe;
  scene->r.mode &= orig_data.mode | ~R_NO_CAMERA_SWITCH;

  Depsgraph *depsgraph = BKE_scene_get_depsgraph(scene,
                                                 get_view_layer_for_scene_strip(scene, strip));
  if (is_frame_update && (depsgraph != nullptr)) {
    BKE_scene_graph_update_for_newframe(depsgraph);
  }

  return out;
}

/**
 * Used for meta-strips & scenes with #SEQ_SCENE_STRIPS flag set.
 */
static SeqResult do_render_strip_seqbase(const RenderData *context,
                                         SeqRenderState *state,
                                         Strip *strip,
                                         float frame_index)
{
  SeqResult out;
  ListBaseT<Strip> *seqbase = nullptr;
  ListBaseT<SeqTimelineChannel> *channels = nullptr;
  int offset;

  seqbase = get_seqbase_from_strip(strip, &channels, &offset);

  if (seqbase && !seqbase->is_empty()) {

    frame_index += offset;

    if (strip->flag & SEQ_SCENE_STRIPS && strip->scene) {
      BKE_animsys_evaluate_all_animation(context->bmain, context->depsgraph, frame_index);
    }

    intra_frame_cache_set_cur_frame(context->scene,
                                    frame_index,
                                    context->view_id,
                                    context->rectx,
                                    context->recty,
                                    context->render != nullptr);
    out = seq_render_strip_stack(context,
                                 state,
                                 channels,
                                 seqbase,
                                 /* scene strips don't have their start taken into account */
                                 frame_index,
                                 0);
  }

  return out;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Strip Stack Rendering Functions
 * \{ */

static SeqResult do_render_strip_uncached(const RenderData *context,
                                          SeqRenderState *state,
                                          Strip *strip,
                                          float timeline_frame,
                                          bool *r_is_proxy_image)
{
  SeqResult out;
  float frame_index = give_frame_index(context->scene, strip, timeline_frame);
  if (strip->type == STRIP_TYPE_META) {
    out = do_render_strip_seqbase(context, state, strip, frame_index);
  }
  else if (strip->type == STRIP_TYPE_SCENE) {
    /* Recursive check. */
    if (!state->scenes_in_progress.contains(strip->scene)) {
      state->scenes_in_progress.add(context->scene);

      if (strip->flag & SEQ_SCENE_STRIPS) {
        if (strip->scene && (context->scene != strip->scene)) {
          /* Use the Scene sequence-strip's scene for the context when rendering the
           * scene's sequences (necessary for multi-cam selector among others). */
          RenderData local_context = *context;
          local_context.scene = strip->scene;
          local_context.skip_cache = true;

          out = do_render_strip_seqbase(&local_context, state, strip, frame_index);
        }
      }
      else {
        /* scene can be nullptr after deletions */
        out = seq_render_scene_strip(context, strip, frame_index, timeline_frame);
      }

      /* End recursive check. */
      state->scenes_in_progress.remove(context->scene);
    }
  }
  else if (strip->is_effect()) {
    out = seq_render_effect_strip_impl(context, state, strip, timeline_frame);
  }
  else if (strip->type == STRIP_TYPE_IMAGE) {
    out.image = seq_render_image_strip(context, strip, timeline_frame, r_is_proxy_image);
    if (out.image && !out.image->can_contain_alpha()) {
      out.is_opaque_before_transform = true;
    }
  }
  else if (strip->type == STRIP_TYPE_MOVIE) {
    out.image = seq_render_movie_strip(context, strip, timeline_frame, r_is_proxy_image);
    if (out.image && !out.image->can_contain_alpha()) {
      out.is_opaque_before_transform = true;
    }
  }
  else if (strip->type == STRIP_TYPE_MOVIECLIP) {
    out.image = seq_render_movieclip_strip(
        context, strip, round_fl_to_int(frame_index), r_is_proxy_image);
    if (out.image && !out.image->can_contain_alpha()) {
      out.is_opaque_before_transform = true;
    }

    if (out.image) {
      /* duplicate frame so movie cache wouldn't be confused by sequencer's stuff */
      ImBuf *i = IMB_dupImBuf(out.image);
      IMB_freeImBuf(out.image);
      out.image = i;
    }
  }
  else if (strip->type == STRIP_TYPE_MASK) {
    out.image = seq_render_mask_strip(context, strip, frame_index);
  }

  return out;
}

SeqResult seq_render_strip(const RenderData *context,
                           SeqRenderState *state,
                           Strip *strip,
                           float timeline_frame)
{
  PRF_scope_with_name("SeqRenderStrip", ProfileCategory::Draw);

  bool use_preprocess = false;
  bool is_proxy_image = false;

  SeqResult res = intra_frame_cache_get_preprocessed(context->scene, strip);
  if (res.is_valid()) {
    return res;
  }

  /* Proxies are not stored in cache. */
  if (!can_use_proxy(context, strip, rendersize_to_proxysize(context->preview_render_size))) {
    res = source_image_cache_get(context, strip, timeline_frame);
  }

  if (!res.is_valid()) {
    res = do_render_strip_uncached(context, state, strip, timeline_frame, &is_proxy_image);
  }

  if (res.is_valid()) {
    use_preprocess = seq_input_have_to_preprocess(strip);
    res = seq_render_preprocess_ibuf(
        context, state, strip, res, timeline_frame, use_preprocess, is_proxy_image);
    intra_frame_cache_put_preprocessed(context->scene, strip, res);
  }

  if (!res.is_valid()) {
    res.image = IMB_allocImBuf(context->rectx, context->recty, ImBufFlags::ByteData);
  }

  return res;
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

static SeqResult seq_render_strip_stack_apply_effect(const RenderData *context,
                                                     SeqRenderState *state,
                                                     Strip *strip,
                                                     float timeline_frame,
                                                     const SeqResult &src1,
                                                     const SeqResult &src2)
{
  EffectHandle sh = strip_blend_mode_handle_get(strip);
  BLI_assert(sh.execute != nullptr);
  float fac = strip->blend_opacity / 100.0f;
  bool swap_input = seq_must_swap_input_in_blend_mode(strip);

  SeqResult out = sh.execute(context,
                             state,
                             strip,
                             timeline_frame,
                             fac,
                             swap_input ? src2 : src1,
                             swap_input ? src1 : src2);
  return out;
}

static bool is_opaque_alpha_over(const Strip *strip, const RenderData *context)
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
  for (StripModifierData &smd : strip->modifiers) {
    const bool modifier_enabled = (context->render && !(smd.flag & STRIP_MODIFIER_FLAG_MUTE)) ||
                                  (!context->render &&
                                   (smd.flag & STRIP_MODIFIER_FLAG_SHOW_PREVIEW));
    /* Assume result is not opaque if there is an enabled Mask modifier, which could
     * introduce alpha. */
    if (modifier_enabled && smd.type == eSeqModifierType_Mask) {
      return false;
    }
  }
  return true;
}

static SeqResult seq_render_strip_stack(const RenderData *context,
                                        SeqRenderState *state,
                                        ListBaseT<SeqTimelineChannel> *channels,
                                        ListBaseT<Strip> *seqbasep,
                                        float timeline_frame,
                                        int chanshown)
{
  PRF_scope_with_name("SeqRenderStrips", ProfileCategory::Draw);
  Vector<Strip *> strips = query_rendered_strips_sorted(
      context->scene, channels, seqbasep, timeline_frame, chanshown);
  if (strips.is_empty()) {
    return {};
  }

  OpaqueQuadTracker opaques;

  int64_t i;
  SeqResult out;
  for (i = strips.size() - 1; i >= 0; i--) {
    Strip *strip = strips[i];

    out = intra_frame_cache_get_composite(context->scene, strip);
    if (out.is_valid()) {
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
    if (!out.is_valid() && early_out == StripEarlyOut::DoEffect &&
        is_opaque_alpha_over(strip, context))
    {
      SeqResult test = seq_render_strip(context, state, strip, timeline_frame);
      BLI_assert(test.is_valid());
      if (!test.image->can_contain_alpha() || i == 0) {
        early_out = StripEarlyOut::UseInput2;
      }
      else {
        early_out = StripEarlyOut::DoEffect;
      }
      /* Free the image. It is stored in cache, so this doesn't affect performance. */
      IMB_freeImBuf(test.image);

      /* Check whether the strip (before transform) content was opaque. */
      if (test.is_opaque_before_transform) {
        opaques.add_occluder(context, strip, i);
      }
    }

    switch (early_out) {
      case StripEarlyOut::NoInput:
      case StripEarlyOut::UseInput2:
        out = seq_render_strip(context, state, strip, timeline_frame);
        break;
      case StripEarlyOut::UseInput1:
        if (i == 0) {
          out.image = IMB_allocImBuf(context->rectx, context->recty, ImBufFlags::ByteData);
        }
        break;
      case StripEarlyOut::DoEffect:
        if (i == 0) {
          /* This is an effect at the bottom of the stack, so one of the inputs does not exist yet:
           * create one that is transparent black. Extra optimization for an alpha over strip at
           * the bottom, we can just return it instead of blending with black. */
          SeqResult ibuf2 = seq_render_strip(context, state, strip, timeline_frame);
          const bool use_float = ibuf2.is_valid() && ibuf2.image->float_data();
          SeqResult ibuf1;
          ibuf1.image = IMB_allocImBuf(context->rectx,
                                       context->recty,
                                       use_float ? ImBufFlags::FloatData : ImBufFlags::ByteData);
          seq_imbuf_assign_spaces(context->scene, ibuf1.image);

          out = seq_render_strip_stack_apply_effect(
              context, state, strip, timeline_frame, ibuf1, ibuf2);
          IMB_metadata_copy(out.image, ibuf2.image);

          intra_frame_cache_put_composite(context->scene, strip, out);

          IMB_freeImBuf(ibuf1.image);
          IMB_freeImBuf(ibuf2.image);
        }
        break;
    }

    if (out.is_valid()) {
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
      SeqResult ibuf1 = out;
      SeqResult ibuf2 = seq_render_strip(context, state, strip, timeline_frame);

      out = seq_render_strip_stack_apply_effect(
          context, state, strip, timeline_frame, ibuf1, ibuf2);

      IMB_freeImBuf(ibuf1.image);
      IMB_freeImBuf(ibuf2.image);
    }

    intra_frame_cache_put_composite(context->scene, strip, out);
  }

  return out;
}

ImBuf *render_give_ibuf(const RenderData *context, float timeline_frame, int chanshown)
{
  Scene *scene = context->scene;
  Editing *ed = editing_get(scene);
  ListBaseT<Strip> *seqbasep;
  ListBaseT<SeqTimelineChannel> *channels;

  if (ed == nullptr) {
    return nullptr;
  }

  if ((chanshown < 0) && !ed->metastack.is_empty()) {
    int count = ed->metastack.count();
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

  intra_frame_cache_set_cur_frame(scene,
                                  timeline_frame,
                                  context->view_id,
                                  context->rectx,
                                  context->recty,
                                  context->render != nullptr);

  Scene *orig_scene = prefetch_get_original_scene(context);
  ImBuf *out = nullptr;
  if (!context->skip_cache) {
    out = final_image_cache_get(orig_scene,
                                timeline_frame,
                                context->view_id,
                                chanshown,
                                {context->rectx, context->recty},
                                context->render != nullptr);
  }

  Vector<Strip *> strips = query_rendered_strips_sorted(
      scene, channels, seqbasep, timeline_frame, chanshown);

  /* Make sure we only keep the `anim` data for strips that are in view. */
  relations_free_all_anim_ibufs(context->scene, timeline_frame);

  SeqRenderState state;

  if (!strips.is_empty() && !out) {
    std::scoped_lock lock(seq_render_mutex);
    /* Try to make space before we add any new frames to the cache if it is full.
     * If we do this after we have added the new cache, we risk removing what we just added. */
    evict_caches_if_full(orig_scene);

    out = seq_render_strip_stack(context, &state, channels, seqbasep, timeline_frame, chanshown)
              .image;

    if (out && (orig_scene->ed->cache_flag & SEQ_CACHE_STORE_FINAL_OUT) && !context->skip_cache) {
      final_image_cache_put(orig_scene,
                            timeline_frame,
                            context->view_id,
                            chanshown,
                            {context->rectx, context->recty},
                            context->render != nullptr,
                            out);
    }
  }

  seq_prefetch_start(context, timeline_frame);

  return out;
}

SeqResult seq_render_give_ibuf_seqbase(const RenderData *context,
                                       SeqRenderState *state,
                                       float timeline_frame,
                                       int chan_shown,
                                       ListBaseT<SeqTimelineChannel> *channels,
                                       ListBaseT<Strip> *seqbasep)
{

  return seq_render_strip_stack(context, state, channels, seqbasep, timeline_frame, chan_shown);
}

ImBuf *render_give_ibuf_direct(const RenderData *context, float timeline_frame, Strip *strip)
{
  SeqRenderState state;

  intra_frame_cache_set_cur_frame(context->scene,
                                  timeline_frame,
                                  context->view_id,
                                  context->rectx,
                                  context->recty,
                                  context->render != nullptr);
  ImBuf *ibuf = seq_render_strip(context, &state, strip, timeline_frame).image;
  return ibuf;
}

bool render_is_muted(const ListBaseT<SeqTimelineChannel> *channels, const Strip *strip)
{
  SeqTimelineChannel *channel = channel_get_by_index(channels, strip->channel);
  return strip->flag & SEQ_MUTE || channel->is_muted();
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

bool render_begin_gpu(const RenderData &rd)
{
  if (rd.gpu_context.ghost_context != nullptr) {
    /* Use GPU context from VSE render data. */
    gpu::GPU_activate_secondary_context(rd.gpu_context);
    GPU_render_begin();
    return true;
  }

  if (BLI_thread_is_main()) {
    /* Use main GPU context. */
    DRW_gpu_context_enable();
    return DRW_gpu_context_is_enabled();
  }

  /* Use GPU context from Render. */
  BLI_assert(rd.render != nullptr);
  GHOST_IContext *render_ghost_context = RE_system_gpu_context_get(rd.render);
  if (!render_ghost_context) {
    return false;
  }

  WM_system_gpu_context_activate(render_ghost_context);
  void *render_gpu_context = RE_blender_gpu_context_ensure(rd.render);
  GPU_render_begin();
  GPU_context_active_set(static_cast<GPUContext *>(render_gpu_context));
  return true;
}

void render_end_gpu(const RenderData &rd)
{
  if (rd.gpu_context.ghost_context != nullptr) {
    /* Use GPU context from VSE render data. */
    GPU_render_end();
    gpu::GPU_deactivate_secondary_context(rd.gpu_context);
  }
  else if (BLI_thread_is_main()) {
    /* Use main GPU context. */
    DRW_gpu_context_disable();
  }
  else {
    /* Use GPU context from Render. */
    BLI_assert(rd.render != nullptr);
    GHOST_IContext *render_ghost_context = RE_system_gpu_context_get(rd.render);
    BLI_assert(render_ghost_context != nullptr);
    GPU_context_active_set(nullptr);
    GPU_render_end();
    WM_system_gpu_context_release(render_ghost_context);
  }
}

}  // namespace blender::seq
