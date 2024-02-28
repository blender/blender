/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_DenoiseOperation.h"
#include "BLI_system.h"
#ifdef WITH_OPENIMAGEDENOISE
#  include "BLI_threads.h"
#  include <OpenImageDenoise/oidn.hpp>
static pthread_mutex_t oidn_lock = BLI_MUTEX_INITIALIZER;
#endif

namespace blender::compositor {

bool COM_is_denoise_supported()
{
#ifdef WITH_OPENIMAGEDENOISE
  /* Always supported through Accelerate framework BNNS on macOS. */
#  ifdef __APPLE__
  return true;
#  else
  return BLI_cpu_support_sse42();
#  endif

#else
  return false;
#endif
}

#ifdef WITH_OPENIMAGEDENOISE
static bool oidn_progress_monitor_function(void *user_ptr, double /*n*/)
{
  const NodeOperation *operation = static_cast<const NodeOperation *>(user_ptr);
  return !operation->is_braked();
}
#endif

class DenoiseFilter {
 private:
#ifdef WITH_OPENIMAGEDENOISE
  oidn::DeviceRef device_;
  oidn::FilterRef filter_;
  bool initialized_ = false;
#endif

 public:
#ifdef WITH_OPENIMAGEDENOISE
  ~DenoiseFilter()
  {
    BLI_assert(!initialized_);
  }

  void init_and_lock_denoiser(NodeOperation *operation, MemoryBuffer *output)
  {
    /* Since it's memory intensive, it's better to run only one instance of OIDN at a time.
     * OpenImageDenoise is multithreaded internally and should use all available cores
     * nonetheless. */
    BLI_mutex_lock(&oidn_lock);

    device_ = oidn::newDevice(oidn::DeviceType::CPU);
    device_.set("setAffinity", false);
    device_.commit();
    filter_ = device_.newFilter("RT");
    filter_.setProgressMonitorFunction(oidn_progress_monitor_function, operation);
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
    filter_.setImage(name.data(),
                     buffer->get_buffer(),
                     oidn::Format::Float3,
                     buffer->get_width(),
                     buffer->get_height(),
                     0,
                     buffer->get_elem_bytes_len());
  }

  template<typename T> void set(const StringRef option_name, T value)
  {
    BLI_assert(initialized_);
    filter_.set(option_name.data(), value);
  }

  void execute()
  {
    BLI_assert(initialized_);
    filter_.commit();
    filter_.execute();
  }

#else
  void init_and_lock_denoiser(NodeOperation * /*operation*/, MemoryBuffer * /*output*/) {}

  void deinit_and_unlock_denoiser() {}

  void set_image(const StringRef /*name*/, MemoryBuffer * /*buffer*/) {}

  template<typename T> void set(const StringRef /*option_name*/, T /*value*/) {}

  void execute() {}
#endif
};

DenoiseBaseOperation::DenoiseBaseOperation()
{
  flags_.can_be_constant = true;
  output_rendered_ = false;
}

void DenoiseBaseOperation::get_area_of_interest(const int /*input_idx*/,
                                                const rcti & /*output_area*/,
                                                rcti &r_input_area)
{
  r_input_area = this->get_canvas();
}

DenoiseOperation::DenoiseOperation()
{
  this->add_input_socket(DataType::Color);
  this->add_input_socket(DataType::Vector);
  this->add_input_socket(DataType::Color);
  this->add_output_socket(DataType::Color);
  settings_ = nullptr;
}

static bool are_guiding_passes_noise_free(const NodeDenoise *settings)
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
  if (settings_) {
    hash_params(int(settings_->hdr), are_guiding_passes_noise_free(settings_));
  }
}

void DenoiseOperation::generate_denoise(MemoryBuffer *output,
                                        MemoryBuffer *input_color,
                                        MemoryBuffer *input_normal,
                                        MemoryBuffer *input_albedo,
                                        const NodeDenoise *settings)
{
  if (input_color->is_a_single_elem()) {
    output->fill(output->get_rect(), input_color->get_elem(0, 0));
    return;
  }

  BLI_assert(COM_is_denoise_supported());

  DenoiseFilter filter;
  filter.init_and_lock_denoiser(this, output);

  filter.set_image("color", input_color);
  if (!input_albedo->is_a_single_elem()) {
    filter.set_image("albedo", input_albedo);
    if (!input_normal->is_a_single_elem()) {
      filter.set_image("normal", input_normal);
    }
  }

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
}

void DenoiseOperation::update_memory_buffer(MemoryBuffer *output,
                                            const rcti & /*area*/,
                                            Span<MemoryBuffer *> inputs)
{
  if (!output_rendered_) {
    this->generate_denoise(output, inputs[0], inputs[1], inputs[2], settings_);
    output_rendered_ = true;
  }
}

DenoisePrefilterOperation::DenoisePrefilterOperation(DataType data_type)
{
  this->add_input_socket(data_type);
  this->add_output_socket(data_type);
  image_name_ = "";
}

void DenoisePrefilterOperation::hash_output_params()
{
  hash_param(image_name_);
}

void DenoisePrefilterOperation::generate_denoise(MemoryBuffer *output, MemoryBuffer *input)
{
  if (input->is_a_single_elem()) {
    copy_v4_v4(output->get_elem(0, 0), input->get_elem(0, 0));
    return;
  }

  BLI_assert(COM_is_denoise_supported());

  DenoiseFilter filter;
  filter.init_and_lock_denoiser(this, output);
  filter.set_image(image_name_, input);
  filter.execute();
  filter.deinit_and_unlock_denoiser();
}

void DenoisePrefilterOperation::update_memory_buffer(MemoryBuffer *output,
                                                     const rcti & /*area*/,
                                                     Span<MemoryBuffer *> inputs)
{
  if (!output_rendered_) {
    this->generate_denoise(output, inputs[0]);
    output_rendered_ = true;
  }
}

}  // namespace blender::compositor
