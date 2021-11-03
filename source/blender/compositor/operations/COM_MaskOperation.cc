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

#include "COM_MaskOperation.h"

#include "BKE_lib_id.h"
#include "BKE_mask.h"

namespace blender::compositor {

MaskOperation::MaskOperation()
{
  this->add_output_socket(DataType::Value);
  mask_ = nullptr;
  mask_width_ = 0;
  mask_height_ = 0;
  mask_width_inv_ = 0.0f;
  mask_height_inv_ = 0.0f;
  frame_shutter_ = 0.0f;
  frame_number_ = 0;
  raster_mask_handle_tot_ = 1;
  memset(raster_mask_handles_, 0, sizeof(raster_mask_handles_));
}

void MaskOperation::init_execution()
{
  if (mask_ && raster_mask_handles_[0] == nullptr) {
    if (raster_mask_handle_tot_ == 1) {
      raster_mask_handles_[0] = BKE_maskrasterize_handle_new();

      BKE_maskrasterize_handle_init(
          raster_mask_handles_[0], mask_, mask_width_, mask_height_, true, true, do_feather_);
    }
    else {
      /* make a throw away copy of the mask */
      const float frame = (float)frame_number_ - frame_shutter_;
      const float frame_step = (frame_shutter_ * 2.0f) / raster_mask_handle_tot_;
      float frame_iter = frame;

      Mask *mask_temp = (Mask *)BKE_id_copy_ex(
          nullptr, &mask_->id, nullptr, LIB_ID_COPY_LOCALIZE | LIB_ID_COPY_NO_ANIMDATA);

      /* trick so we can get unkeyed edits to display */
      {
        MaskLayer *masklay;
        MaskLayerShape *masklay_shape;

        for (masklay = (MaskLayer *)mask_temp->masklayers.first; masklay;
             masklay = masklay->next) {
          masklay_shape = BKE_mask_layer_shape_verify_frame(masklay, frame_number_);
          BKE_mask_layer_shape_from_mask(masklay, masklay_shape);
        }
      }

      for (unsigned int i = 0; i < raster_mask_handle_tot_; i++) {
        raster_mask_handles_[i] = BKE_maskrasterize_handle_new();

        /* re-eval frame info */
        BKE_mask_evaluate(mask_temp, frame_iter, true);

        BKE_maskrasterize_handle_init(raster_mask_handles_[i],
                                      mask_temp,
                                      mask_width_,
                                      mask_height_,
                                      true,
                                      true,
                                      do_feather_);

        frame_iter += frame_step;
      }

      BKE_id_free(nullptr, &mask_temp->id);
    }
  }
}

void MaskOperation::deinit_execution()
{
  for (unsigned int i = 0; i < raster_mask_handle_tot_; i++) {
    if (raster_mask_handles_[i]) {
      BKE_maskrasterize_handle_free(raster_mask_handles_[i]);
      raster_mask_handles_[i] = nullptr;
    }
  }
}

void MaskOperation::determine_canvas(const rcti &preferred_area, rcti &r_area)
{
  if (mask_width_ == 0 || mask_height_ == 0) {
    r_area = COM_AREA_NONE;
  }
  else {
    r_area = preferred_area;
    r_area.xmax = r_area.xmin + mask_width_;
    r_area.ymax = r_area.ymin + mask_height_;
  }
}

void MaskOperation::execute_pixel_sampled(float output[4],
                                          float x,
                                          float y,
                                          PixelSampler /*sampler*/)
{
  const float xy[2] = {
      (x * mask_width_inv_) + mask_px_ofs_[0],
      (y * mask_height_inv_) + mask_px_ofs_[1],
  };

  if (raster_mask_handle_tot_ == 1) {
    if (raster_mask_handles_[0]) {
      output[0] = BKE_maskrasterize_handle_sample(raster_mask_handles_[0], xy);
    }
    else {
      output[0] = 0.0f;
    }
  }
  else {
    /* In case loop below fails. */
    output[0] = 0.0f;

    for (unsigned int i = 0; i < raster_mask_handle_tot_; i++) {
      if (raster_mask_handles_[i]) {
        output[0] += BKE_maskrasterize_handle_sample(raster_mask_handles_[i], xy);
      }
    }

    /* until we get better falloff */
    output[0] /= raster_mask_handle_tot_;
  }
}

void MaskOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                 const rcti &area,
                                                 Span<MemoryBuffer *> UNUSED(inputs))
{
  Vector<MaskRasterHandle *> handles = get_non_null_handles();
  if (handles.size() == 0) {
    output->fill(area, COM_VALUE_ZERO);
    return;
  }

  float xy[2];
  for (BuffersIterator<float> it = output->iterate_with({}, area); !it.is_end(); ++it) {
    xy[0] = it.x * mask_width_inv_ + mask_px_ofs_[0];
    xy[1] = it.y * mask_height_inv_ + mask_px_ofs_[1];
    *it.out = 0.0f;
    for (MaskRasterHandle *handle : handles) {
      *it.out += BKE_maskrasterize_handle_sample(handle, xy);
    }

    /* Until we get better falloff. */
    *it.out /= raster_mask_handle_tot_;
  }
}

Vector<MaskRasterHandle *> MaskOperation::get_non_null_handles() const
{
  Vector<MaskRasterHandle *> handles;
  for (int i = 0; i < raster_mask_handle_tot_; i++) {
    MaskRasterHandle *handle = raster_mask_handles_[i];
    if (handle == nullptr) {
      continue;
    }
    handles.append(handle);
  }
  return handles;
}

}  // namespace blender::compositor
