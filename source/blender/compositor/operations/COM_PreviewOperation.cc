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
#include "BKE_image.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_math_color.h"
#include "BLI_utildefines.h"
#include "COM_defines.h"
#include "MEM_guardedalloc.h"
#include "PIL_time.h"
#include "WM_api.h"
#include "WM_types.h"

#include "BKE_node.h"
#include "IMB_colormanagement.h"
#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

PreviewOperation::PreviewOperation(const ColorManagedViewSettings *viewSettings,
                                   const ColorManagedDisplaySettings *displaySettings)

{
  this->addInputSocket(COM_DT_COLOR, COM_SC_NO_RESIZE);
  this->m_preview = nullptr;
  this->m_outputBuffer = nullptr;
  this->m_input = nullptr;
  this->m_divider = 1.0f;
  this->m_viewSettings = viewSettings;
  this->m_displaySettings = displaySettings;
}

void PreviewOperation::verifyPreview(bNodeInstanceHash *previews, bNodeInstanceKey key)
{
  /* Size (0, 0) ensures the preview rect is not allocated in advance,
   * this is set later in initExecution once the resolution is determined.
   */
  this->m_preview = BKE_node_preview_verify(previews, key, 0, 0, true);
}

void PreviewOperation::initExecution()
{
  this->m_input = getInputSocketReader(0);

  if (this->getWidth() == (unsigned int)this->m_preview->xsize &&
      this->getHeight() == (unsigned int)this->m_preview->ysize) {
    this->m_outputBuffer = this->m_preview->rect;
  }

  if (this->m_outputBuffer == nullptr) {
    this->m_outputBuffer = (unsigned char *)MEM_callocN(
        sizeof(unsigned char) * 4 * getWidth() * getHeight(), "PreviewOperation");
    if (this->m_preview->rect) {
      MEM_freeN(this->m_preview->rect);
    }
    this->m_preview->xsize = getWidth();
    this->m_preview->ysize = getHeight();
    this->m_preview->rect = this->m_outputBuffer;
  }
}

void PreviewOperation::deinitExecution()
{
  this->m_outputBuffer = nullptr;
  this->m_input = nullptr;
}

void PreviewOperation::executeRegion(rcti *rect, unsigned int /*tileNumber*/)
{
  int offset;
  float color[4];
  struct ColormanageProcessor *cm_processor;

  cm_processor = IMB_colormanagement_display_processor_new(this->m_viewSettings,
                                                           this->m_displaySettings);

  for (int y = rect->ymin; y < rect->ymax; y++) {
    offset = (y * getWidth() + rect->xmin) * 4;
    for (int x = rect->xmin; x < rect->xmax; x++) {
      float rx = floor(x / this->m_divider);
      float ry = floor(y / this->m_divider);

      color[0] = 0.0f;
      color[1] = 0.0f;
      color[2] = 0.0f;
      color[3] = 1.0f;
      this->m_input->readSampled(color, rx, ry, COM_PS_NEAREST);
      IMB_colormanagement_processor_apply_v4(cm_processor, color);
      rgba_float_to_uchar(this->m_outputBuffer + offset, color);
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

  newInput.xmin = input->xmin / this->m_divider;
  newInput.xmax = input->xmax / this->m_divider;
  newInput.ymin = input->ymin / this->m_divider;
  newInput.ymax = input->ymax / this->m_divider;

  return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
}
void PreviewOperation::determineResolution(unsigned int resolution[2],
                                           unsigned int preferredResolution[2])
{
  NodeOperation::determineResolution(resolution, preferredResolution);

  /* If resolution is 0 there are two possible scenarios:
   * - Either node is not connected at all
   * - It is connected to input which doesn't have own resolution (i.e. color input).
   *
   * In the former case we rely on the execution system to not evaluate this node.
   *
   * For the latter case we use 1 pixel preview, so that it's possible to see preview color in the
   * preview. This is how final F12 render will behave (flood-fill final frame with the color).
   *
   * Having things consistent in terms that node preview is scaled down F12 render is a very
   * natural thing to do. */
  int width = max_ii(1, resolution[0]);
  int height = max_ii(1, resolution[1]);

  this->m_divider = 0.0f;
  if (width > height) {
    this->m_divider = (float)COM_PREVIEW_SIZE / (width);
  }
  else {
    this->m_divider = (float)COM_PREVIEW_SIZE / (height);
  }
  width = width * this->m_divider;
  height = height * this->m_divider;

  resolution[0] = width;
  resolution[1] = height;
}

CompositorPriority PreviewOperation::getRenderPriority() const
{
  return COM_PRIORITY_LOW;
}
