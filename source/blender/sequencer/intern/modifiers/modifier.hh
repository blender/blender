/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup sequencer
 */

#include "BLI_math_vector.hh"
#include "BLI_task.hh"

#include "IMB_imbuf.hh"

struct bContext;
struct ARegionType;
struct Strip;
struct uiLayout;
struct Panel;
struct PanelType;
struct PointerRNA;

namespace blender::seq {

bool modifier_persistent_uids_are_valid(const Strip &strip);

void draw_mask_input_type_settings(const bContext *C, uiLayout *layout, PointerRNA *ptr);

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
void apply_and_advance_mask(const float4 input, float4 &result, const uchar *&mask);
void apply_and_advance_mask(const float4 input, float4 &result, const float *&mask);
void apply_and_advance_mask(const float4 input, float4 &result, const void *&mask);

/* Given `T` that implements an `apply` function:
 *
 *    template <typename ImageT, typename MaskT>
 *    void apply(ImageT* image, const MaskT* mask, IndexRange size);
 *
 * this function calls the apply() function in parallel
 * chunks of the image to process, and with needed
 * uchar, float or void types (void is used for mask, when there is
 * no masking). Both input and mask images are expected to have
 * 4 (RGBA) color channels. Input is modified. */
template<typename T> void apply_modifier_op(T &op, ImBuf *ibuf, const ImBuf *mask)
{
  if (ibuf == nullptr) {
    return;
  }
  BLI_assert_msg(ibuf->channels == 0 || ibuf->channels == 4,
                 "Sequencer only supports 4 channel images");
  BLI_assert_msg(mask == nullptr || mask->channels == 0 || mask->channels == 4,
                 "Sequencer only supports 4 channel images");

  threading::parallel_for(IndexRange(size_t(ibuf->x) * ibuf->y), 32 * 1024, [&](IndexRange range) {
    uchar *image_byte = ibuf->byte_buffer.data;
    float *image_float = ibuf->float_buffer.data;
    const uchar *mask_byte = mask ? mask->byte_buffer.data : nullptr;
    const float *mask_float = mask ? mask->float_buffer.data : nullptr;
    const void *mask_none = nullptr;
    int64_t offset = range.first() * 4;

    /* Instantiate the needed processing function based on image/mask
     * data types. */
    if (image_byte) {
      if (mask_byte) {
        op.apply(image_byte + offset, mask_byte + offset, range);
      }
      else if (mask_float) {
        op.apply(image_byte + offset, mask_float + offset, range);
      }
      else {
        op.apply(image_byte + offset, mask_none, range);
      }
    }
    else if (image_float) {
      if (mask_byte) {
        op.apply(image_float + offset, mask_byte + offset, range);
      }
      else if (mask_float) {
        op.apply(image_float + offset, mask_float + offset, range);
      }
      else {
        op.apply(image_float + offset, mask_none, range);
      }
    }
  });
}

}  // namespace blender::seq
