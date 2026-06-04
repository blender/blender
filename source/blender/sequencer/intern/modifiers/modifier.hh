/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup sequencer
 */

#include "BLI_math_color.h"
#include "BLI_math_interp.hh"
#include "BLI_math_matrix.hh"
#include "BLI_math_vector.hh"
#include "BLI_task.hh"

#include "DNA_sequence_types.h"

#include "IMB_imbuf.hh"

namespace blender {

struct bContext;
struct ARegionType;
struct ImBuf;
struct Strip;
struct Panel;
struct PanelType;
struct PointerRNA;

namespace ui {
struct Layout;
}  // namespace ui

namespace seq {

struct RenderData;
struct SeqRenderState;

struct ModifierApplyContext {
  ModifierApplyContext(const RenderData &render_data,
                       SeqRenderState &render_state,
                       const Strip &strip,
                       const float3x3 &transform,
                       const float3x3 &transform_comp_result,
                       const float timeline_frame,
                       ImBuf *image)
      : render_data(render_data),
        render_state(render_state),
        strip(strip),
        transform(transform),
        transform_comp_result(transform_comp_result),
        timeline_frame(timeline_frame),
        image(image)
  {
  }
  const RenderData &render_data;
  SeqRenderState &render_state;
  const Strip &strip;

  /* Transformation from strip image local pixel coordinates to the
   * full render area pixel coordinates.This is used to sample
   * modifier masks (since masks are in full render area space). */
  const float3x3 transform;
  /* Transformation to apply when sampling masks in compositor modifier. */
  const float3x3 transform_comp_result;
  /* Timeline frame at which the modifiers are being applied at. */
  const float timeline_frame;
  ImBuf *const image;

  /* How much the resulting image should be translated, in pixels.
   * Compositor modifier can have some nodes that translate the output
   * image. */
  float2 result_translation = float2(0, 0);
};

void modifier_apply_stack(ModifierApplyContext &context);

ImBuf *modifier_render_mask_input(const ModifierApplyContext &context,
                                  const StripModifierData &smd);

bool modifier_persistent_uids_are_valid(const Strip &strip);

void draw_mask_input_type_settings(const bContext *C, ui::Layout &layout, PointerRNA *ptr);

bool modifier_ui_poll(const bContext *C, PanelType *pt);

using PanelDrawFn = void (*)(const bContext *, Panel *);

PanelType *modifier_panel_register(ARegionType *region_type,
                                   const eStripModifierType type,
                                   PanelDrawFn draw);

float4 load_pixel_premul(const uchar *ptr);
float4 load_pixel_premul(const float *ptr);
void store_pixel_premul(const float4 pix, uchar *ptr);
void store_pixel_premul(const float4 pix, float *ptr);
float4 load_pixel_raw(const uchar *ptr);
float4 load_pixel_raw(const float *ptr);
void store_pixel_raw(const float4 pix, uchar *ptr);
void store_pixel_raw(const float4 pix, float *ptr);

/* Mask sampler for #apply_modifier_op: no mask is present. */
struct MaskSamplerNone {
  void begin_row(int64_t /*y*/) {}
  void apply_mask(const float4 /*input*/, float4 & /*result*/) {}
  float load_mask_min()
  {
    return 0.0f;
  }
};

/* Mask sampler for #apply_modifier_op: floating point mask,
 * same size as input, no transform. */
struct MaskSamplerDirectFloat {
  MaskSamplerDirectFloat(const ImBuf *mask) : mask(mask)
  {
    BLI_assert(mask && mask->float_data());
  }
  void begin_row(int64_t y)
  {
    BLI_assert(y >= 0 && y < mask->y);
    ptr = mask->float_data() + y * mask->x * 4;
  }
  void apply_mask(const float4 input, float4 &result)
  {
    float3 m(this->ptr);
    result.x = math::interpolate(input.x, result.x, m.x);
    result.y = math::interpolate(input.y, result.y, m.y);
    result.z = math::interpolate(input.z, result.z, m.z);
    this->ptr += 4;
  }
  float load_mask_min()
  {
    float r = std::min({this->ptr[0], this->ptr[1], this->ptr[2]});
    this->ptr += 4;
    return r;
  }
  const float *ptr = nullptr;
  const ImBuf *mask;
};

/* Mask sampler for #apply_modifier_op: byte mask,
 * same size as input, no transform. */
struct MaskSamplerDirectByte {
  MaskSamplerDirectByte(const ImBuf *mask) : mask(mask)
  {
    BLI_assert(mask && mask->byte_data());
  }
  void begin_row(int64_t y)
  {
    BLI_assert(y >= 0 && y < mask->y);
    ptr = mask->byte_data() + y * mask->x * 4;
  }
  void apply_mask(const float4 input, float4 &result)
  {
    float3 m;
    rgb_uchar_to_float(m, this->ptr);
    result.x = math::interpolate(input.x, result.x, m.x);
    result.y = math::interpolate(input.y, result.y, m.y);
    result.z = math::interpolate(input.z, result.z, m.z);
    this->ptr += 4;
  }
  float load_mask_min()
  {
    float r = float(std::min({this->ptr[0], this->ptr[1], this->ptr[2]})) * (1.0f / 255.0f);
    this->ptr += 4;
    return r;
  }
  const uchar *ptr = nullptr;
  const ImBuf *mask;
};

/* Mask sampler for #apply_modifier_op: floating point mask,
 * sample mask with a transform. */
struct MaskSamplerTransformedFloat {
  MaskSamplerTransformedFloat(const ImBuf *mask, const float3x3 &transform)
      : mask(mask), transform(transform)
  {
    BLI_assert(mask && mask->float_data());
    start_uv = transform.location().xy();
    add_x = transform.x_axis().xy();
    add_y = transform.y_axis().xy();
  }
  void begin_row(int64_t y)
  {
    this->cur_y = y;
    this->cur_x = 0;
    /* Sample at pixel centers. */
    this->cur_uv_row = this->start_uv + (y + 0.5f) * this->add_y + 0.5f * this->add_x;
  }
  void apply_mask(const float4 input, float4 &result)
  {
    float2 uv = this->cur_uv_row + this->cur_x * this->add_x - 0.5f;
    float4 m;
    math::interpolate_bilinear_border_fl(
        this->mask->float_data(), m, this->mask->x, this->mask->y, 4, uv.x, uv.y);
    result.x = math::interpolate(input.x, result.x, m.x);
    result.y = math::interpolate(input.y, result.y, m.y);
    result.z = math::interpolate(input.z, result.z, m.z);
    this->cur_x++;
  }
  float load_mask_min()
  {
    float2 uv = this->cur_uv_row + this->cur_x * this->add_x - 0.5f;
    float4 m;
    math::interpolate_bilinear_border_fl(
        this->mask->float_data(), m, this->mask->x, this->mask->y, 4, uv.x, uv.y);
    float r = std::min({m.x, m.y, m.z});
    this->cur_x++;
    return r;
  }
  int64_t cur_x = 0, cur_y = 0;
  const ImBuf *mask;
  const float3x3 transform;
  float2 start_uv, add_x, add_y;
  float2 cur_uv_row;
};

/* Mask sampler for #apply_modifier_op: byte mask,
 * sample mask with a transform. */
struct MaskSamplerTransformedByte {
  MaskSamplerTransformedByte(const ImBuf *mask, const float3x3 &transform)
      : mask(mask), transform(transform)
  {
    BLI_assert(mask && mask->byte_data());
    start_uv = transform.location().xy();
    add_x = transform.x_axis().xy();
    add_y = transform.y_axis().xy();
  }
  void begin_row(int64_t y)
  {
    this->cur_y = y;
    this->cur_x = 0;
    /* Sample at pixel centers. */
    this->cur_uv_row = this->start_uv + (y + 0.5f) * this->add_y + 0.5f * this->add_x;
  }
  void apply_mask(const float4 input, float4 &result)
  {
    float2 uv = this->cur_uv_row + this->cur_x * this->add_x - 0.5f;
    uchar4 mb = math::interpolate_bilinear_border_byte(
        this->mask->byte_data(), this->mask->x, this->mask->y, uv.x, uv.y);
    float3 m;
    rgb_uchar_to_float(m, mb);
    result.x = math::interpolate(input.x, result.x, m.x);
    result.y = math::interpolate(input.y, result.y, m.y);
    result.z = math::interpolate(input.z, result.z, m.z);
    this->cur_x++;
  }
  float load_mask_min()
  {
    float2 uv = this->cur_uv_row + this->cur_x * this->add_x - 0.5f;
    uchar4 m = math::interpolate_bilinear_border_byte(
        this->mask->byte_data(), this->mask->x, this->mask->y, uv.x, uv.y);
    float r = float(std::min({m.x, m.y, m.z})) * (1.0f / 255.0f);
    this->cur_x++;
    return r;
  }
  int64_t cur_x = 0, cur_y = 0;
  const ImBuf *mask;
  const float3x3 transform;
  float2 start_uv, add_x, add_y;
  float2 cur_uv_row;
};

/* Given `T` that implements an `apply` function:
 *
 *    template <typename ImageT, typename MaskSampler>
 *    void apply(ImageT* image, MaskSampler &mask, int image_x, IndexRange y_range);
 *
 * this function calls the apply() function in parallel
 * chunks of the image to process, and with needed
 * uchar or float ImageT types, and with appropriate MaskSampler
 * instantiated, depending on whether the mask exists, data type
 * of the mask, and whether it needs a transformation or can be
 * sampled directly.
 *
 * Both input and mask images are expected to have
 * 4 (RGBA) color channels. Input is modified. */
template<typename T>
void apply_modifier_op(T &op, ImBuf *ibuf, const ImBuf *mask, const float3x3 &mask_transform)
{
  if (ibuf == nullptr) {
    return;
  }
  BLI_assert_msg(ibuf->channels == 0 || ibuf->channels == 4,
                 "Sequencer only supports 4 channel images");
  BLI_assert_msg(mask == nullptr || mask->channels == 0 || mask->channels == 4,
                 "Sequencer only supports 4 channel images");
  const bool direct_mask_sampling = mask == nullptr || (mask->x == ibuf->x && mask->y == ibuf->y &&
                                                        math::is_identity(mask_transform));
  const int image_x = ibuf->x;
  uchar *image_byte = ibuf->byte_data_for_write();
  float *image_float = ibuf->float_data_for_write();
  threading::parallel_for(IndexRange(ibuf->y), 16, [&](IndexRange y_range) {
    const uchar *mask_byte = mask ? mask->byte_data() : nullptr;
    const float *mask_float = mask ? mask->float_data() : nullptr;

    /* Instantiate the needed processing function based on image/mask
     * data types. */
    if (image_byte) {
      if (mask_byte) {
        if (direct_mask_sampling) {
          MaskSamplerDirectByte sampler(mask);
          op.apply(image_byte, sampler, image_x, y_range);
        }
        else {
          MaskSamplerTransformedByte sampler(mask, mask_transform);
          op.apply(image_byte, sampler, image_x, y_range);
        }
      }
      else if (mask_float) {
        if (direct_mask_sampling) {
          MaskSamplerDirectFloat sampler(mask);
          op.apply(image_byte, sampler, image_x, y_range);
        }
        else {
          MaskSamplerTransformedFloat sampler(mask, mask_transform);
          op.apply(image_byte, sampler, image_x, y_range);
        }
      }
      else {
        MaskSamplerNone sampler;
        op.apply(image_byte, sampler, image_x, y_range);
      }
    }
    else if (image_float) {
      if (mask_byte) {
        if (direct_mask_sampling) {
          MaskSamplerDirectByte sampler(mask);
          op.apply(image_float, sampler, image_x, y_range);
        }
        else {
          MaskSamplerTransformedByte sampler(mask, mask_transform);
          op.apply(image_float, sampler, image_x, y_range);
        }
      }
      else if (mask_float) {
        if (direct_mask_sampling) {
          MaskSamplerDirectFloat sampler(mask);
          op.apply(image_float, sampler, image_x, y_range);
        }
        else {
          MaskSamplerTransformedFloat sampler(mask, mask_transform);
          op.apply(image_float, sampler, image_x, y_range);
        }
      }
      else {
        MaskSamplerNone sampler;
        op.apply(image_float, sampler, image_x, y_range);
      }
    }
  });
}

}  // namespace seq
}  // namespace blender
