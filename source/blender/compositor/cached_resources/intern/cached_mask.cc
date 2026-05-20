/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <memory>

#include "BLI_math_color.h"
#include "BLI_math_vector_types.hh"

#include "BKE_lib_id.hh"
#include "BKE_mask.hh"

#include "DNA_ID.h"
#include "DNA_mask_types.h"

#include "COM_cached_mask.hh"
#include "COM_context.hh"
#include "COM_result.hh"
#include "COM_utilities.hh"

namespace blender::compositor {

/* --------------------------------------------------------------------
 * Cached Mask Key.
 */

CachedMaskKey::CachedMaskKey(const Domain &domain,
                             float aspect_ratio,
                             bool use_feather,
                             bool srgb_to_linear,
                             int frame,
                             int motion_blur_samples,
                             float motion_blur_shutter)
    : data_size(domain.data_size),
      display_size(domain.display_size),
      data_offset(domain.data_offset),
      aspect_ratio(aspect_ratio),
      use_feather(use_feather),
      srgb_to_linear(srgb_to_linear),
      frame(frame),
      motion_blur_samples(motion_blur_samples),
      motion_blur_shutter(motion_blur_shutter)
{
}

uint64_t CachedMaskKey::hash() const
{
  return get_default_hash(
      get_default_hash(data_size, display_size, data_offset, aspect_ratio, use_feather),
      get_default_hash(srgb_to_linear, frame, motion_blur_samples, motion_blur_shutter));
}

/* --------------------------------------------------------------------
 * Cached Mask.
 */

static Vector<MaskRasterHandle *> get_mask_raster_handles(Mask *mask,
                                                          int2 size,
                                                          int frame,
                                                          bool frame_is_current,
                                                          bool use_feather,
                                                          int motion_blur_samples,
                                                          float motion_blur_shutter)
{
  Vector<MaskRasterHandle *> handles;

  if (!mask) {
    return handles;
  }

  /* If motion blur samples are 1 (no motion blur) and frame is current, we can just use currently
   * evaluated mask. */
  if (motion_blur_samples == 1 && frame_is_current) {
    MaskRasterHandle *handle = BKE_maskrasterize_handle_new();
    BKE_maskrasterize_handle_init(handle, mask, size.x, size.y, true, true, use_feather);
    handles.append(handle);
    return handles;
  }

  /* Otherwise, we have a number of motion blur samples or non-current frame, so make a copy of the
   * Mask ID and evaluate it at the needed frames to get the needed raster handles. */
  Mask *evaluation_mask = reinterpret_cast<Mask *>(
      BKE_id_copy_ex(nullptr, &mask->id, nullptr, LIB_ID_COPY_LOCALIZE | LIB_ID_COPY_NO_ANIMDATA));

  /* We evaluate at the frames in the range [current_frame - shutter, current_frame + shutter]. */
  const float start_frame = frame - motion_blur_shutter;
  const float frame_step = (motion_blur_shutter * 2.0f) / motion_blur_samples;
  for (int i = 0; i < motion_blur_samples; i++) {
    MaskRasterHandle *handle = BKE_maskrasterize_handle_new();
    BKE_mask_evaluate(evaluation_mask, start_frame + frame_step * i, true);
    BKE_maskrasterize_handle_init(
        handle, evaluation_mask, size.x, size.y, true, true, use_feather);
    handles.append(handle);
  }

  BKE_id_free(nullptr, &evaluation_mask->id);

  return handles;
}

CachedMask::CachedMask(Context &context,
                       Mask *mask,
                       const Domain &domain,
                       int frame,
                       float aspect_ratio,
                       bool use_feather,
                       int motion_blur_samples,
                       float motion_blur_shutter,
                       bool srgb_to_linear)
    : result(context.create_result(ResultType::Float))
{
  const bool frame_is_current = context.get_frame_number() == frame;
  Vector<MaskRasterHandle *> handles = get_mask_raster_handles(mask,
                                                               domain.display_size,
                                                               frame,
                                                               frame_is_current,
                                                               use_feather,
                                                               motion_blur_samples,
                                                               motion_blur_shutter);

  Result result_cpu = context.create_result(ResultType::Float);

  result_cpu.allocate_texture(domain, false, ResultStorageType::CPU);
  parallel_for(domain.data_size, [&](const int2 texel) {
    /* Compute the coordinates in the [0, 1] range and add 0.5 to evaluate the mask at the
     * center of pixels. */
    float2 coordinates = (float2(texel + domain.data_offset) + 0.5f) / float2(domain.display_size);
    /* Do aspect ratio correction around the center 0.5 point. */
    coordinates = (coordinates - float2(0.5)) * float2(1.0, aspect_ratio) + float2(0.5);

    float mask_value = 0.0f;
    for (MaskRasterHandle *handle : handles) {
      mask_value += BKE_maskrasterize_handle_sample(handle, coordinates);
    }
    mask_value /= handles.size();
    if (srgb_to_linear) {
      mask_value = srgb_to_linearrgb(mask_value);
    }
    result_cpu.store_pixel(texel, mask_value);
  });

  for (MaskRasterHandle *handle : handles) {
    BKE_maskrasterize_handle_free(handle);
  }

  if (context.use_gpu()) {
    Result result_gpu = result_cpu.upload_to_gpu(false);
    this->result.share_data(result_gpu);
    result_gpu.release();
  }
  else {
    this->result.share_data(result_cpu);
  }

  result_cpu.release();
}

CachedMask::~CachedMask()
{
  this->result.release();
}

/* --------------------------------------------------------------------
 * Cached Mask Container.
 */

void CachedMaskContainer::reset()
{
  /* First, delete all cached masks that are no longer needed. */
  for (auto &cached_masks_for_id : map_.values()) {
    cached_masks_for_id.remove_if([](auto item) { return !item.value->needed; });
  }
  map_.remove_if([](auto item) { return item.value.is_empty(); });
  update_counts_.remove_if([&](auto item) { return !map_.contains(item.key); });

  /* Second, reset the needed status of the remaining cached masks to false to ready them to track
   * their needed status for the next evaluation. */
  for (auto &cached_masks_for_id : map_.values()) {
    for (auto &value : cached_masks_for_id.values()) {
      value->needed = false;
    }
  }
}

Result &CachedMaskContainer::get(Context &context,
                                 Mask *mask,
                                 const Domain &domain,
                                 float aspect_ratio,
                                 bool use_feather,
                                 int frame,
                                 int motion_blur_samples,
                                 float motion_blur_shutter,
                                 bool srgb_to_linear)
{
  const CachedMaskKey key(domain,
                          aspect_ratio,
                          use_feather,
                          srgb_to_linear,
                          frame,
                          motion_blur_samples,
                          motion_blur_shutter);

  const std::string library_key = mask->id.lib ? mask->id.lib->id.name : "";
  const std::string id_key = std::string(mask->id.name) + library_key;
  auto &cached_masks_for_id = map_.lookup_or_add_default(id_key);

  /* Invalidate the cache for that mask if it was changed since it was cached. */
  if (!cached_masks_for_id.is_empty() &&
      mask->runtime.last_update != update_counts_.lookup(id_key))
  {
    cached_masks_for_id.clear();
  }

  auto &cached_mask = *cached_masks_for_id.lookup_or_add_cb(key, [&]() {
    return std::make_unique<CachedMask>(context,
                                        mask,
                                        domain,
                                        frame,
                                        aspect_ratio,
                                        use_feather,
                                        motion_blur_samples,
                                        motion_blur_shutter,
                                        srgb_to_linear);
  });

  /* Store the current update count to later compare to and check if the mask changed. */
  update_counts_.add_overwrite(id_key, mask->runtime.last_update);

  cached_mask.needed = true;
  return cached_mask.result;
}

}  // namespace blender::compositor
