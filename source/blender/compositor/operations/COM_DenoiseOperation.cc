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

bool COM_is_denoise_supported()
{
#ifdef WITH_OPENIMAGEDENOISE
  /* Always supported through Accelerate framework BNNS on macOS. */
#  ifdef __APPLE__
  return true;
#  else
  return BLI_cpu_support_sse41();
#  endif

#else
  return false;
#endif
}

class DenoiseFilter {
 private:
#ifdef WITH_OPENIMAGEDENOISE
  oidn::DeviceRef device;
  oidn::FilterRef filter;
#endif
  bool initialized_ = false;

 public:
  ~DenoiseFilter()
  {
    BLI_assert(!initialized_);
  }

#ifdef WITH_OPENIMAGEDENOISE
  void init_and_lock_denoiser(MemoryBuffer *output)
  {
    /* Since it's memory intensive, it's better to run only one instance of OIDN at a time.
     * OpenImageDenoise is multithreaded internally and should use all available cores
     * nonetheless. */
    BLI_mutex_lock(&oidn_lock);

    device = oidn::newDevice();
    device.commit();
    filter = device.newFilter("RT");
    initialized_ = true;
    set_image("output", output);
  }

  void deinit_and_unlock_denoiser()
  {
    BLI_mutex_unlock(&oidn_lock);
    initialized_ = false;
  }

  void set_image(const StringRef name, MemoryBuffer *buffer)
  {
    BLI_assert(initialized_);
    BLI_assert(!buffer->is_a_single_elem());
    filter.setImage(name.data(),
                    buffer->getBuffer(),
                    oidn::Format::Float3,
                    buffer->getWidth(),
                    buffer->getHeight(),
                    0,
                    buffer->get_elem_bytes_len());
  }

  template<typename T> void set(const StringRef option_name, T value)
  {
    BLI_assert(initialized_);
    filter.set(option_name.data(), value);
  }

  void execute()
  {
    BLI_assert(initialized_);
    filter.commit();
    filter.execute();
  }

#else
  void init_and_lock_denoiser(MemoryBuffer *UNUSED(output))
  {
  }

  void deinit_and_unlock_denoiser()
  {
  }

  void set_image(const StringRef UNUSED(name), MemoryBuffer *UNUSED(buffer))
  {
  }

  template<typename T> void set(const StringRef UNUSED(option_name), T UNUSED(value))
  {
  }

  void execute()
  {
  }
#endif
};

DenoiseBaseOperation::DenoiseBaseOperation()
{
  flags.is_fullframe_operation = true;
  output_rendered_ = false;
}

bool DenoiseBaseOperation::determineDependingAreaOfInterest(rcti * /*input*/,
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

void DenoiseBaseOperation::get_area_of_interest(const int UNUSED(input_idx),
                                                const rcti &UNUSED(output_area),
                                                rcti &r_input_area)
{
  r_input_area.xmin = 0;
  r_input_area.xmax = this->getWidth();
  r_input_area.ymin = 0;
  r_input_area.ymax = this->getHeight();
}

DenoiseOperation::DenoiseOperation()
{
  this->addInputSocket(DataType::Color);
  this->addInputSocket(DataType::Vector);
  this->addInputSocket(DataType::Color);
  this->addOutputSocket(DataType::Color);
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

static bool are_guiding_passes_noise_free(NodeDenoise *settings)
{
  switch (settings->prefilter) {
    case CMP_NODE_DENOISE_PREFILTER_NONE:
    case CMP_NODE_DENOISE_PREFILTER_ACCURATE: /* Prefiltered with #DenoisePrefilterOperation. */
      return true;
    case CMP_NODE_DENOISE_PREFILTER_FAST:
    default:
      return false;
  }
}

void DenoiseOperation::hash_output_params()
{
  if (m_settings) {
    hash_params((int)m_settings->hdr, are_guiding_passes_noise_free(m_settings));
  }
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

  BLI_assert(COM_is_denoise_supported());
  /* OpenImageDenoise needs full buffers. */
  MemoryBuffer *buf_color = input_color->is_a_single_elem() ? input_color->inflate() : input_color;
  MemoryBuffer *buf_normal = input_normal && input_normal->is_a_single_elem() ?
                                 input_normal->inflate() :
                                 input_normal;
  MemoryBuffer *buf_albedo = input_albedo && input_albedo->is_a_single_elem() ?
                                 input_albedo->inflate() :
                                 input_albedo;

  DenoiseFilter filter;
  filter.init_and_lock_denoiser(output);

  filter.set_image("color", buf_color);
  filter.set_image("normal", buf_normal);
  filter.set_image("albedo", buf_albedo);

  BLI_assert(settings);
  if (settings) {
    filter.set("hdr", settings->hdr);
    filter.set("srgb", false);
    filter.set("cleanAux", are_guiding_passes_noise_free(settings));
  }

  filter.execute();
  filter.deinit_and_unlock_denoiser();

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

DenoisePrefilterOperation::DenoisePrefilterOperation(DataType data_type)
{
  this->addInputSocket(data_type);
  this->addOutputSocket(data_type);
  image_name_ = "";
}

void DenoisePrefilterOperation::hash_output_params()
{
  hash_param(image_name_);
}

MemoryBuffer *DenoisePrefilterOperation::createMemoryBuffer(rcti *rect2)
{
  MemoryBuffer *input = (MemoryBuffer *)this->get_input_operation(0)->initializeTileData(rect2);
  rcti rect;
  BLI_rcti_init(&rect, 0, getWidth(), 0, getHeight());

  MemoryBuffer *result = new MemoryBuffer(getOutputSocket()->getDataType(), rect);
  generate_denoise(result, input);

  return result;
}

void DenoisePrefilterOperation::generate_denoise(MemoryBuffer *output, MemoryBuffer *input)
{
  BLI_assert(COM_is_denoise_supported());

  /* Denoising needs full buffers. */
  MemoryBuffer *input_buf = input->is_a_single_elem() ? input->inflate() : input;

  DenoiseFilter filter;
  filter.init_and_lock_denoiser(output);
  filter.set_image(image_name_, input_buf);
  filter.execute();
  filter.deinit_and_unlock_denoiser();

  /* Delete inflated buffers. */
  if (input->is_a_single_elem()) {
    delete input_buf;
  }
}

void DenoisePrefilterOperation::update_memory_buffer(MemoryBuffer *output,
                                                     const rcti &UNUSED(area),
                                                     Span<MemoryBuffer *> inputs)
{
  if (!output_rendered_) {
    this->generate_denoise(output, inputs[0]);
    output_rendered_ = true;
  }
}

}  // namespace blender::compositor
