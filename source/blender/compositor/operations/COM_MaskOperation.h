/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Copyright 2012, Blender Foundation.
 */

#pragma once

#include "BLI_listbase.h"
#include "COM_MultiThreadedOperation.h"
#include "DNA_mask_types.h"
#include "IMB_imbuf_types.h"

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
  int maskWidth_;
  int maskHeight_;
  float maskWidthInv_;  /* `1 / maskWidth_` */
  float maskHeightInv_; /* `1 / maskHeight_` */
  float mask_px_ofs_[2];

  float frame_shutter_;
  int frame_number_;

  bool do_feather_;

  struct MaskRasterHandle *rasterMaskHandles_[CMP_NODE_MASK_MBLUR_SAMPLES_MAX];
  unsigned int rasterMaskHandleTot_;

  /**
   * Determine the output resolution. The resolution is retrieved from the Renderer
   */
  void determine_canvas(const rcti &preferred_area, rcti &r_area) override;

 public:
  MaskOperation();

  void initExecution() override;
  void deinitExecution() override;

  void setMask(Mask *mask)
  {
    mask_ = mask;
  }
  void setMaskWidth(int width)
  {
    maskWidth_ = width;
    maskWidthInv_ = 1.0f / (float)width;
    mask_px_ofs_[0] = maskWidthInv_ * 0.5f;
  }
  void setMaskHeight(int height)
  {
    maskHeight_ = height;
    maskHeightInv_ = 1.0f / (float)height;
    mask_px_ofs_[1] = maskHeightInv_ * 0.5f;
  }
  void setFramenumber(int frame_number)
  {
    frame_number_ = frame_number;
  }
  void setFeather(bool feather)
  {
    do_feather_ = feather;
  }

  void setMotionBlurSamples(int samples)
  {
    rasterMaskHandleTot_ = MIN2(MAX2(1, samples), CMP_NODE_MASK_MBLUR_SAMPLES_MAX);
  }
  void setMotionBlurShutter(float shutter)
  {
    frame_shutter_ = shutter;
  }

  void executePixelSampled(float output[4], float x, float y, PixelSampler sampler) override;

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;

 private:
  Vector<MaskRasterHandle *> get_non_null_handles() const;
};

}  // namespace blender::compositor
