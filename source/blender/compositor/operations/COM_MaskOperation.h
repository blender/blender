/* SPDX-FileCopyrightText: 2012 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_listbase.h"
#include "COM_MultiThreadedOperation.h"
#include "DNA_mask_types.h"
#include "IMB_imbuf_types.hh"

/* Forward declarations. */
struct MaskRasterHandle;

namespace blender::compositor {

/**
 * Class with implementation of mask rasterization
 */
class MaskOperation : public MultiThreadedOperation {
 protected:
  Mask *mask_;

  /* NOTE: these are used more like aspect,
   * but they _do_ impact on mask detail */
  int mask_width_;
  int mask_height_;
  float mask_width_inv_;  /* `1 / mask_width_` */
  float mask_height_inv_; /* `1 / mask_height_` */
  float mask_px_ofs_[2];

  float frame_shutter_;
  int frame_number_;

  bool do_feather_;

  struct MaskRasterHandle *raster_mask_handles_[CMP_NODE_MASK_MBLUR_SAMPLES_MAX];
  unsigned int raster_mask_handle_tot_;

  /**
   * Determine the output resolution. The resolution is retrieved from the Renderer
   */
  void determine_canvas(const rcti &preferred_area, rcti &r_area) override;

 public:
  MaskOperation();

  void init_execution() override;
  void deinit_execution() override;

  void set_mask(Mask *mask)
  {
    mask_ = mask;
  }
  void set_mask_width(int width)
  {
    mask_width_ = width;
    mask_width_inv_ = 1.0f / (float)width;
    mask_px_ofs_[0] = mask_width_inv_ * 0.5f;
  }
  void set_mask_height(int height)
  {
    mask_height_ = height;
    mask_height_inv_ = 1.0f / (float)height;
    mask_px_ofs_[1] = mask_height_inv_ * 0.5f;
  }
  int get_mask_width()
  {
    return mask_width_;
  }
  int get_mask_height()
  {
    return mask_height_;
  }
  void set_framenumber(int frame_number)
  {
    frame_number_ = frame_number;
  }
  void set_feather(bool feather)
  {
    do_feather_ = feather;
  }

  void set_motion_blur_samples(int samples)
  {
    raster_mask_handle_tot_ = std::min(std::max(1, samples), CMP_NODE_MASK_MBLUR_SAMPLES_MAX);
  }
  void set_motion_blur_shutter(float shutter)
  {
    frame_shutter_ = shutter;
  }

  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;

 private:
  Vector<MaskRasterHandle *> get_non_null_handles() const;
};

}  // namespace blender::compositor
