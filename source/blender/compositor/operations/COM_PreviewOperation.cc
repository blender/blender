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
 * Copyright 2011, Blender Foundation.
 */

#include "COM_PreviewOperation.h"

#include "BKE_node.h"
#include "IMB_colormanagement.h"

namespace blender::compositor {

PreviewOperation::PreviewOperation(const ColorManagedViewSettings *viewSettings,
                                   const ColorManagedDisplaySettings *displaySettings,
                                   const unsigned int defaultWidth,
                                   const unsigned int defaultHeight)

{
  this->addInputSocket(DataType::Color, ResizeMode::Align);
  m_preview = nullptr;
  m_outputBuffer = nullptr;
  m_input = nullptr;
  m_divider = 1.0f;
  m_viewSettings = viewSettings;
  m_displaySettings = displaySettings;
  m_defaultWidth = defaultWidth;
  m_defaultHeight = defaultHeight;
  flags.use_viewer_border = true;
  flags.is_preview_operation = true;
}

void PreviewOperation::verifyPreview(bNodeInstanceHash *previews, bNodeInstanceKey key)
{
  /* Size (0, 0) ensures the preview rect is not allocated in advance,
   * this is set later in initExecution once the resolution is determined.
   */
  m_preview = BKE_node_preview_verify(previews, key, 0, 0, true);
}

void PreviewOperation::initExecution()
{
  m_input = getInputSocketReader(0);

  if (this->getWidth() == (unsigned int)m_preview->xsize &&
      this->getHeight() == (unsigned int)m_preview->ysize) {
    m_outputBuffer = m_preview->rect;
  }

  if (m_outputBuffer == nullptr) {
    m_outputBuffer = (unsigned char *)MEM_callocN(
        sizeof(unsigned char) * 4 * getWidth() * getHeight(), "PreviewOperation");
    if (m_preview->rect) {
      MEM_freeN(m_preview->rect);
    }
    m_preview->xsize = getWidth();
    m_preview->ysize = getHeight();
    m_preview->rect = m_outputBuffer;
  }
}

void PreviewOperation::deinitExecution()
{
  m_outputBuffer = nullptr;
  m_input = nullptr;
}

void PreviewOperation::executeRegion(rcti *rect, unsigned int /*tileNumber*/)
{
  int offset;
  float color[4];
  struct ColormanageProcessor *cm_processor;

  cm_processor = IMB_colormanagement_display_processor_new(m_viewSettings, m_displaySettings);

  for (int y = rect->ymin; y < rect->ymax; y++) {
    offset = (y * getWidth() + rect->xmin) * 4;
    for (int x = rect->xmin; x < rect->xmax; x++) {
      float rx = floor(x / m_divider);
      float ry = floor(y / m_divider);

      color[0] = 0.0f;
      color[1] = 0.0f;
      color[2] = 0.0f;
      color[3] = 1.0f;
      m_input->readSampled(color, rx, ry, PixelSampler::Nearest);
      IMB_colormanagement_processor_apply_v4(cm_processor, color);
      rgba_float_to_uchar(m_outputBuffer + offset, color);
      offset += 4;
    }
  }

  IMB_colormanagement_processor_free(cm_processor);
}
bool PreviewOperation::determineDependingAreaOfInterest(rcti *input,
                                                        ReadBufferOperation *readOperation,
                                                        rcti *output)
{
  rcti newInput;

  newInput.xmin = input->xmin / m_divider;
  newInput.xmax = input->xmax / m_divider;
  newInput.ymin = input->ymin / m_divider;
  newInput.ymax = input->ymax / m_divider;

  return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
}
void PreviewOperation::determine_canvas(const rcti &UNUSED(preferred_area), rcti &r_area)
{
  /* Use default preview resolution as preferred ensuring it has size so that
   * generated inputs (which don't have resolution on their own) are displayed */
  BLI_assert(m_defaultWidth > 0 && m_defaultHeight > 0);
  rcti local_preferred;
  BLI_rcti_init(&local_preferred, 0, m_defaultWidth, 0, m_defaultHeight);
  NodeOperation::determine_canvas(local_preferred, r_area);

  /* If resolution is 0 there are two possible scenarios:
   * - Either node is not connected at all
   * - Or it is connected to an input which has no resolution.
   *
   * In the former case we rely on the execution system to not evaluate this node.
   *
   * The latter case would only happen if an input doesn't set any resolution ignoring output
   * preferred resolution. In such case preview size will be 0 too.
   */
  int width = BLI_rcti_size_x(&r_area);
  int height = BLI_rcti_size_y(&r_area);
  m_divider = 0.0f;
  if (width > 0 && height > 0) {
    if (width > height) {
      m_divider = (float)COM_PREVIEW_SIZE / (width);
    }
    else {
      m_divider = (float)COM_PREVIEW_SIZE / (height);
    }
  }
  width = width * m_divider;
  height = height * m_divider;

  BLI_rcti_init(&r_area, r_area.xmin, r_area.xmin + width, r_area.ymin, r_area.ymin + height);
}

eCompositorPriority PreviewOperation::getRenderPriority() const
{
  return eCompositorPriority::Low;
}

void PreviewOperation::get_area_of_interest(const int input_idx,
                                            const rcti &output_area,
                                            rcti &r_input_area)
{
  BLI_assert(input_idx == 0);
  UNUSED_VARS_NDEBUG(input_idx);

  r_input_area.xmin = output_area.xmin / m_divider;
  r_input_area.xmax = output_area.xmax / m_divider;
  r_input_area.ymin = output_area.ymin / m_divider;
  r_input_area.ymax = output_area.ymax / m_divider;
}

void PreviewOperation::update_memory_buffer_partial(MemoryBuffer *UNUSED(output),
                                                    const rcti &area,
                                                    Span<MemoryBuffer *> inputs)
{
  MemoryBuffer *input = inputs[0];
  struct ColormanageProcessor *cm_processor = IMB_colormanagement_display_processor_new(
      m_viewSettings, m_displaySettings);

  rcti buffer_area;
  BLI_rcti_init(&buffer_area, 0, this->getWidth(), 0, this->getHeight());
  BuffersIteratorBuilder<uchar> it_builder(
      m_outputBuffer, buffer_area, area, COM_data_type_num_channels(DataType::Color));

  for (BuffersIterator<uchar> it = it_builder.build(); !it.is_end(); ++it) {
    const float rx = it.x / m_divider;
    const float ry = it.y / m_divider;

    float color[4];
    input->read_elem_checked(rx, ry, color);
    IMB_colormanagement_processor_apply_v4(cm_processor, color);
    rgba_float_to_uchar(it.out, color);
  }

  IMB_colormanagement_processor_free(cm_processor);
}

}  // namespace blender::compositor
