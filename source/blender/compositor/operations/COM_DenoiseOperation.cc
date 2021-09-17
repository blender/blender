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
 * Copyright 2019, Blender Foundation.
 */

#include "COM_DenoiseOperation.h"
#include "BLI_math.h"
#include "BLI_system.h"
#ifdef WITH_OPENIMAGEDENOISE
#  include "BLI_threads.h"
#  include <OpenImageDenoise/oidn.hpp>
static pthread_mutex_t oidn_lock = BLI_MUTEX_INITIALIZER;
#endif
#include <iostream>

namespace blender::compositor {

DenoiseOperation::DenoiseOperation()
{
  this->addInputSocket(DataType::Color);
  this->addInputSocket(DataType::Vector);
  this->addInputSocket(DataType::Color);
  this->addOutputSocket(DataType::Color);
  this->m_settings = nullptr;
  flags.is_fullframe_operation = true;
  output_rendered_ = false;
}
void DenoiseOperation::initExecution()
{
  SingleThreadedOperation::initExecution();
  this->m_inputProgramColor = getInputSocketReader(0);
  this->m_inputProgramNormal = getInputSocketReader(1);
  this->m_inputProgramAlbedo = getInputSocketReader(2);
}

void DenoiseOperation::deinitExecution()
{
  this->m_inputProgramColor = nullptr;
  this->m_inputProgramNormal = nullptr;
  this->m_inputProgramAlbedo = nullptr;
  SingleThreadedOperation::deinitExecution();
}

MemoryBuffer *DenoiseOperation::createMemoryBuffer(rcti *rect2)
{
  MemoryBuffer *tileColor = (MemoryBuffer *)this->m_inputProgramColor->initializeTileData(rect2);
  MemoryBuffer *tileNormal = (MemoryBuffer *)this->m_inputProgramNormal->initializeTileData(rect2);
  MemoryBuffer *tileAlbedo = (MemoryBuffer *)this->m_inputProgramAlbedo->initializeTileData(rect2);
  rcti rect;
  rect.xmin = 0;
  rect.ymin = 0;
  rect.xmax = getWidth();
  rect.ymax = getHeight();
  MemoryBuffer *result = new MemoryBuffer(DataType::Color, rect);
  this->generateDenoise(result, tileColor, tileNormal, tileAlbedo, this->m_settings);
  return result;
}

bool DenoiseOperation::determineDependingAreaOfInterest(rcti * /*input*/,
                                                        ReadBufferOperation *readOperation,
                                                        rcti *output)
{
  if (isCached()) {
    return false;
  }

  rcti newInput;
  newInput.xmax = this->getWidth();
  newInput.xmin = 0;
  newInput.ymax = this->getHeight();
  newInput.ymin = 0;
  return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
}

void DenoiseOperation::generateDenoise(MemoryBuffer *output,
                                       MemoryBuffer *input_color,
                                       MemoryBuffer *input_normal,
                                       MemoryBuffer *input_albedo,
                                       NodeDenoise *settings)
{
  BLI_assert(input_color->getBuffer());
  if (!input_color->getBuffer()) {
    return;
  }

#ifdef WITH_OPENIMAGEDENOISE
  /* Always supported through Accelerate framework BNNS on macOS. */
#  ifndef __APPLE__
  if (BLI_cpu_support_sse41())
#  endif
  {
    /* OpenImageDenoise needs full buffers. */
    MemoryBuffer *buf_color = input_color->is_a_single_elem() ? input_color->inflate() :
                                                                input_color;
    MemoryBuffer *buf_normal = input_normal && input_normal->is_a_single_elem() ?
                                   input_normal->inflate() :
                                   input_normal;
    MemoryBuffer *buf_albedo = input_albedo && input_albedo->is_a_single_elem() ?
                                   input_albedo->inflate() :
                                   input_albedo;

    /* Since it's memory intensive, it's better to run only one instance of OIDN at a time.
     * OpenImageDenoise is multithreaded internally and should use all available cores nonetheless.
     */
    BLI_mutex_lock(&oidn_lock);

    oidn::DeviceRef device = oidn::newDevice();
    device.commit();

    oidn::FilterRef filter = device.newFilter("RT");
    filter.setImage("color",
                    buf_color->getBuffer(),
                    oidn::Format::Float3,
                    buf_color->getWidth(),
                    buf_color->getHeight(),
                    0,
                    sizeof(float[4]));
    if (buf_normal && buf_normal->getBuffer()) {
      filter.setImage("normal",
                      buf_normal->getBuffer(),
                      oidn::Format::Float3,
                      buf_normal->getWidth(),
                      buf_normal->getHeight(),
                      0,
                      sizeof(float[3]));
    }
    if (buf_albedo && buf_albedo->getBuffer()) {
      filter.setImage("albedo",
                      buf_albedo->getBuffer(),
                      oidn::Format::Float3,
                      buf_albedo->getWidth(),
                      buf_albedo->getHeight(),
                      0,
                      sizeof(float[4]));
    }
    filter.setImage("output",
                    output->getBuffer(),
                    oidn::Format::Float3,
                    buf_color->getWidth(),
                    buf_color->getHeight(),
                    0,
                    sizeof(float[4]));

    BLI_assert(settings);
    if (settings) {
      filter.set("hdr", settings->hdr);
      filter.set("srgb", false);
    }

    filter.commit();
    filter.execute();
    BLI_mutex_unlock(&oidn_lock);

    /* Copy the alpha channel, OpenImageDenoise currently only supports RGB. */
    output->copy_from(input_color, input_color->get_rect(), 3, COM_DATA_TYPE_VALUE_CHANNELS, 3);

    /* Delete inflated buffers. */
    if (input_color->is_a_single_elem()) {
      delete buf_color;
    }
    if (input_normal && input_normal->is_a_single_elem()) {
      delete buf_normal;
    }
    if (input_albedo && input_albedo->is_a_single_elem()) {
      delete buf_albedo;
    }

    return;
  }
#endif
  /* If built without OIDN or running on an unsupported CPU, just pass through. */
  UNUSED_VARS(input_albedo, input_normal, settings);
  output->copy_from(input_color, input_color->get_rect());
}

void DenoiseOperation::get_area_of_interest(const int UNUSED(input_idx),
                                            const rcti &UNUSED(output_area),
                                            rcti &r_input_area)
{
  r_input_area.xmin = 0;
  r_input_area.xmax = this->getWidth();
  r_input_area.ymin = 0;
  r_input_area.ymax = this->getHeight();
}

void DenoiseOperation::update_memory_buffer(MemoryBuffer *output,
                                            const rcti &UNUSED(area),
                                            Span<MemoryBuffer *> inputs)
{
  if (!output_rendered_) {
    this->generateDenoise(output, inputs[0], inputs[1], inputs[2], m_settings);
    output_rendered_ = true;
  }
}

}  // namespace blender::compositor
