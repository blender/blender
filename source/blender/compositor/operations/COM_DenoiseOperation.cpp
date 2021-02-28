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

DenoiseOperation::DenoiseOperation()
{
  this->addInputSocket(COM_DT_COLOR);
  this->addInputSocket(COM_DT_VECTOR);
  this->addInputSocket(COM_DT_COLOR);
  this->addOutputSocket(COM_DT_COLOR);
  this->m_settings = nullptr;
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
  MemoryBuffer *result = new MemoryBuffer(COM_DT_COLOR, &rect);
  float *data = result->getBuffer();
  this->generateDenoise(data, tileColor, tileNormal, tileAlbedo, this->m_settings);
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

void DenoiseOperation::generateDenoise(float *data,
                                       MemoryBuffer *inputTileColor,
                                       MemoryBuffer *inputTileNormal,
                                       MemoryBuffer *inputTileAlbedo,
                                       NodeDenoise *settings)
{
  float *inputBufferColor = inputTileColor->getBuffer();
  BLI_assert(inputBufferColor);
  if (!inputBufferColor) {
    return;
  }
#ifdef WITH_OPENIMAGEDENOISE
  /* Always supported through Accelerate framework BNNS on macOS. */
#  ifndef __APPLE__
  if (BLI_cpu_support_sse41())
#  endif
  {
    oidn::DeviceRef device = oidn::newDevice();
    device.commit();

    oidn::FilterRef filter = device.newFilter("RT");
    filter.setImage("color",
                    inputBufferColor,
                    oidn::Format::Float3,
                    inputTileColor->getWidth(),
                    inputTileColor->getHeight(),
                    0,
                    sizeof(float[4]));
    if (inputTileNormal && inputTileNormal->getBuffer()) {
      filter.setImage("normal",
                      inputTileNormal->getBuffer(),
                      oidn::Format::Float3,
                      inputTileNormal->getWidth(),
                      inputTileNormal->getHeight(),
                      0,
                      sizeof(float[3]));
    }
    if (inputTileAlbedo && inputTileAlbedo->getBuffer()) {
      filter.setImage("albedo",
                      inputTileAlbedo->getBuffer(),
                      oidn::Format::Float3,
                      inputTileAlbedo->getWidth(),
                      inputTileAlbedo->getHeight(),
                      0,
                      sizeof(float[4]));
    }
    filter.setImage("output",
                    data,
                    oidn::Format::Float3,
                    inputTileColor->getWidth(),
                    inputTileColor->getHeight(),
                    0,
                    sizeof(float[4]));

    BLI_assert(settings);
    if (settings) {
      filter.set("hdr", settings->hdr);
      filter.set("srgb", false);
    }

    filter.commit();
    /* Since it's memory intensive, it's better to run only one instance of OIDN at a time.
     * OpenImageDenoise is multithreaded internally and should use all available cores nonetheless.
     */
    BLI_mutex_lock(&oidn_lock);
    filter.execute();
    BLI_mutex_unlock(&oidn_lock);

    /* copy the alpha channel, OpenImageDenoise currently only supports RGB */
    size_t numPixels = inputTileColor->getWidth() * inputTileColor->getHeight();
    for (size_t i = 0; i < numPixels; i++) {
      data[i * 4 + 3] = inputBufferColor[i * 4 + 3];
    }
    return;
  }
#endif
  /* If built without OIDN or running on an unsupported CPU, just pass through. */
  UNUSED_VARS(inputTileAlbedo, inputTileNormal, settings);
  ::memcpy(data,
           inputBufferColor,
           sizeof(float[4]) * inputTileColor->getWidth() * inputTileColor->getHeight());
}
