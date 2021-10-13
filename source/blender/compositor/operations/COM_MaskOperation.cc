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
  this->addOutputSocket(DataType::Value);
  m_mask = nullptr;
  m_maskWidth = 0;
  m_maskHeight = 0;
  m_maskWidthInv = 0.0f;
  m_maskHeightInv = 0.0f;
  m_frame_shutter = 0.0f;
  m_frame_number = 0;
  m_rasterMaskHandleTot = 1;
  memset(m_rasterMaskHandles, 0, sizeof(m_rasterMaskHandles));
}

void MaskOperation::initExecution()
{
  if (m_mask && m_rasterMaskHandles[0] == nullptr) {
    if (m_rasterMaskHandleTot == 1) {
      m_rasterMaskHandles[0] = BKE_maskrasterize_handle_new();

      BKE_maskrasterize_handle_init(
          m_rasterMaskHandles[0], m_mask, m_maskWidth, m_maskHeight, true, true, m_do_feather);
    }
    else {
      /* make a throw away copy of the mask */
      const float frame = (float)m_frame_number - m_frame_shutter;
      const float frame_step = (m_frame_shutter * 2.0f) / m_rasterMaskHandleTot;
      float frame_iter = frame;

      Mask *mask_temp = (Mask *)BKE_id_copy_ex(
          nullptr, &m_mask->id, nullptr, LIB_ID_COPY_LOCALIZE | LIB_ID_COPY_NO_ANIMDATA);

      /* trick so we can get unkeyed edits to display */
      {
        MaskLayer *masklay;
        MaskLayerShape *masklay_shape;

        for (masklay = (MaskLayer *)mask_temp->masklayers.first; masklay;
             masklay = masklay->next) {
          masklay_shape = BKE_mask_layer_shape_verify_frame(masklay, m_frame_number);
          BKE_mask_layer_shape_from_mask(masklay, masklay_shape);
        }
      }

      for (unsigned int i = 0; i < m_rasterMaskHandleTot; i++) {
        m_rasterMaskHandles[i] = BKE_maskrasterize_handle_new();

        /* re-eval frame info */
        BKE_mask_evaluate(mask_temp, frame_iter, true);

        BKE_maskrasterize_handle_init(m_rasterMaskHandles[i],
                                      mask_temp,
                                      m_maskWidth,
                                      m_maskHeight,
                                      true,
                                      true,
                                      m_do_feather);

        frame_iter += frame_step;
      }

      BKE_id_free(nullptr, &mask_temp->id);
    }
  }
}

void MaskOperation::deinitExecution()
{
  for (unsigned int i = 0; i < m_rasterMaskHandleTot; i++) {
    if (m_rasterMaskHandles[i]) {
      BKE_maskrasterize_handle_free(m_rasterMaskHandles[i]);
      m_rasterMaskHandles[i] = nullptr;
    }
  }
}

void MaskOperation::determine_canvas(const rcti &preferred_area, rcti &r_area)
{
  if (m_maskWidth == 0 || m_maskHeight == 0) {
    r_area = COM_AREA_NONE;
  }
  else {
    r_area = preferred_area;
    r_area.xmax = r_area.xmin + m_maskWidth;
    r_area.ymax = r_area.ymin + m_maskHeight;
  }
}

void MaskOperation::executePixelSampled(float output[4],
                                        float x,
                                        float y,
                                        PixelSampler /*sampler*/)
{
  const float xy[2] = {
      (x * m_maskWidthInv) + m_mask_px_ofs[0],
      (y * m_maskHeightInv) + m_mask_px_ofs[1],
  };

  if (m_rasterMaskHandleTot == 1) {
    if (m_rasterMaskHandles[0]) {
      output[0] = BKE_maskrasterize_handle_sample(m_rasterMaskHandles[0], xy);
    }
    else {
      output[0] = 0.0f;
    }
  }
  else {
    /* In case loop below fails. */
    output[0] = 0.0f;

    for (unsigned int i = 0; i < m_rasterMaskHandleTot; i++) {
      if (m_rasterMaskHandles[i]) {
        output[0] += BKE_maskrasterize_handle_sample(m_rasterMaskHandles[i], xy);
      }
    }

    /* until we get better falloff */
    output[0] /= m_rasterMaskHandleTot;
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
    xy[0] = it.x * m_maskWidthInv + m_mask_px_ofs[0];
    xy[1] = it.y * m_maskHeightInv + m_mask_px_ofs[1];
    *it.out = 0.0f;
    for (MaskRasterHandle *handle : handles) {
      *it.out += BKE_maskrasterize_handle_sample(handle, xy);
    }

    /* Until we get better falloff. */
    *it.out /= m_rasterMaskHandleTot;
  }
}

Vector<MaskRasterHandle *> MaskOperation::get_non_null_handles() const
{
  Vector<MaskRasterHandle *> handles;
  for (int i = 0; i < m_rasterMaskHandleTot; i++) {
    MaskRasterHandle *handle = m_rasterMaskHandles[i];
    if (handle == nullptr) {
      continue;
    }
    handles.append(handle);
  }
  return handles;
}

}  // namespace blender::compositor
