/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <cstdint>
#include <memory>

#include "BLI_array.hh"
#include "BLI_hash.hh"
#include "BLI_index_range.hh"
#include "BLI_listbase.h"
#include "BLI_math_vector_types.hh"
#include "BLI_task.hh"

#include "GPU_texture.h"

#include "BKE_lib_id.h"
#include "BKE_mask.h"

#include "DNA_ID.h"
#include "DNA_mask_types.h"

#include "COM_cached_mask.hh"
#include "COM_context.hh"

namespace blender::realtime_compositor {

/* --------------------------------------------------------------------
 * Cached Mask Key.
 */

CachedMaskKey::CachedMaskKey(int2 size,
                             bool use_feather,
                             int motion_blur_samples,
                             float motion_blur_shutter)
    : size(size),
      use_feather(use_feather),
      motion_blur_samples(motion_blur_samples),
      motion_blur_shutter(motion_blur_shutter)
{
}

uint64_t CachedMaskKey::hash() const
{
  return get_default_hash_4(size, use_feather, motion_blur_samples, motion_blur_shutter);
}

bool operator==(const CachedMaskKey &a, const CachedMaskKey &b)
{
  return a.size == b.size && a.use_feather == b.use_feather &&
         a.motion_blur_samples == b.motion_blur_samples &&
         a.motion_blur_shutter == b.motion_blur_shutter;
}

/* --------------------------------------------------------------------
 * Cached Mask.
 */

static Vector<MaskRasterHandle *> get_mask_raster_handles(Mask *mask,
                                                          int2 size,
                                                          int current_frame,
                                                          bool use_feather,
                                                          int motion_blur_samples,
                                                          float motion_blur_shutter)
{
  Vector<MaskRasterHandle *> handles;

  if (!mask) {
    return handles;
  }

  /* If motion blur samples are 1, that means motion blur is disabled, in that case, just return
   * the currently evaluated raster handle. */
  if (motion_blur_samples == 1) {
    MaskRasterHandle *handle = BKE_maskrasterize_handle_new();
    BKE_maskrasterize_handle_init(handle, mask, size.x, size.y, true, true, use_feather);
    handles.append(handle);
    return handles;
  }

  /* Otherwise, we have a number of motion blur samples, so make a copy of the Mask ID and evaluate
   * it at the different motion blur frames to get the needed raster handles. */
  Mask *evaluation_mask = reinterpret_cast<Mask *>(
      BKE_id_copy_ex(nullptr, &mask->id, nullptr, LIB_ID_COPY_LOCALIZE | LIB_ID_COPY_NO_ANIMDATA));

  /* We evaluate at the frames in the range [current_frame - shutter, current_frame + shutter]. */
  const float start_frame = current_frame - motion_blur_shutter;
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

CachedMask::CachedMask(Mask *mask,
                       int2 size,
                       int frame,
                       bool use_feather,
                       int motion_blur_samples,
                       float motion_blur_shutter)
{
  Vector<MaskRasterHandle *> handles = get_mask_raster_handles(
      mask, size, frame, use_feather, motion_blur_samples, motion_blur_shutter);

  Array<float> evaluated_mask(size.x * size.y);
  threading::parallel_for(IndexRange(size.y), 1, [&](const IndexRange sub_y_range) {
    for (const int64_t y : sub_y_range) {
      for (const int64_t x : IndexRange(size.x)) {
        /* Compute the coordinates in the [0, 1] range and add 0.5 to evaluate the mask at the
         * center of pixels. */
        const float2 coordinates = (float2(x, y) + 0.5f) / float2(size);
        float mask_value = 0.0f;
        for (MaskRasterHandle *handle : handles) {
          mask_value += BKE_maskrasterize_handle_sample(handle, coordinates);
        }
        evaluated_mask[y * size.x + x] = mask_value / handles.size();
      }
    }
  });

  for (MaskRasterHandle *handle : handles) {
    BKE_maskrasterize_handle_free(handle);
  }

  texture_ = GPU_texture_create_2d("Cached Mask",
                                   size.x,
                                   size.y,
                                   1,
                                   GPU_R16F,
                                   GPU_TEXTURE_USAGE_SHADER_READ,
                                   evaluated_mask.data());
}

CachedMask::~CachedMask()
{
  GPU_texture_free(texture_);
}

GPUTexture *CachedMask::texture()
{
  return texture_;
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

  /* Second, reset the needed status of the remaining cached masks to false to ready them to track
   * their needed status for the next evaluation. */
  for (auto &cached_masks_for_id : map_.values()) {
    for (auto &value : cached_masks_for_id.values()) {
      value->needed = false;
    }
  }
}

CachedMask &CachedMaskContainer::get(Context &context,
                                     Mask *mask,
                                     int2 size,
                                     bool use_feather,
                                     int motion_blur_samples,
                                     float motion_blur_shutter)
{
  const CachedMaskKey key(size, use_feather, motion_blur_samples, motion_blur_shutter);

  auto &cached_masks_for_id = map_.lookup_or_add_default(mask->id.name);

  /* Invalidate the cache for that mask ID if it was changed and reset the recalculate flag. */
  if (context.query_id_recalc_flag(reinterpret_cast<ID *>(mask)) & ID_RECALC_ALL) {
    cached_masks_for_id.clear();
  }

  auto &cached_mask = *cached_masks_for_id.lookup_or_add_cb(key, [&]() {
    return std::make_unique<CachedMask>(mask,
                                        size,
                                        context.get_frame_number(),
                                        use_feather,
                                        motion_blur_samples,
                                        motion_blur_shutter);
  });

  cached_mask.needed = true;
  return cached_mask;
}

}  // namespace blender::realtime_compositor
