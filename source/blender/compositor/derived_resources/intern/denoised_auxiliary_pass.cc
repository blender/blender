/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef WITH_OPENIMAGEDENOISE

#  include <cstdint>
#  include <memory>

#  include "BLI_assert.h"
#  include "BLI_hash.hh"

#  include "COM_context.hh"
#  include "COM_denoised_auxiliary_pass.hh"
#  include "COM_result.hh"
#  include "COM_utilities.hh"
#  include "COM_utilities_oidn.hh"

#  include <OpenImageDenoise/oidn.hpp>

namespace blender::compositor {

/* ------------------------------------------------------------------------------------------------
 * Denoised Auxiliary Pass Key.
 */

DenoisedAuxiliaryPassKey::DenoisedAuxiliaryPassKey(const DenoisedAuxiliaryPassType type,
                                                   const oidn::Quality quality)
    : type(type), quality(quality)
{
}

uint64_t DenoisedAuxiliaryPassKey::hash() const
{
  return get_default_hash(this->type, this->quality);
}

bool operator==(const DenoisedAuxiliaryPassKey &a, const DenoisedAuxiliaryPassKey &b)
{
  return a.type == b.type && a.quality == b.quality;
}

/* --------------------------------------------------------------------
 * Denoised Auxiliary Pass.
 */

/* A callback to cancel the filter operations by evaluating the context's is_canceled method. The
 * API specifies that true indicates the filter should continue, while false indicates it should
 * stop, so invert the condition. This callback can also be used to track progress using the given
 * n argument, but we currently don't make use of it. See OIDNProgressMonitorFunction in the API
 * for more information. */
static bool oidn_progress_monitor_function(void *user_ptr, double /*n*/)
{
  const Context *context = static_cast<const Context *>(user_ptr);
  return !context->is_canceled();
}

static const char *get_pass_name(const DenoisedAuxiliaryPassType type)
{
  switch (type) {
    case DenoisedAuxiliaryPassType::Albedo:
      return "albedo";
    case DenoisedAuxiliaryPassType::Normal:
      return "normal";
  }

  BLI_assert_unreachable();
  return "";
}

DenoisedAuxiliaryPass::DenoisedAuxiliaryPass(Context &context,
                                             const Result &pass,
                                             const DenoisedAuxiliaryPassType type,
                                             const oidn::Quality quality)
    : result(context.create_result(pass.type()))
{
  Result denoise_input = context.create_result(pass.type());
  Result *denoise_output = nullptr;
  bool is_denoising_in_place = false;
  if (context.use_gpu()) {
    Result input_pass_cpu = pass.download_to_cpu();
    denoise_input.share_data(input_pass_cpu);
    input_pass_cpu.release();

    denoise_output = &denoise_input;
    is_denoising_in_place = true;
  }
  else {
    denoise_input.share_data(pass);

    this->result.allocate_texture(pass.domain());
    denoise_output = &this->result;
  }

  oidn::DeviceRef device = create_oidn_device(context);
  device.commit();

  oidn::BufferRef input_buffer = create_oidn_buffer(device, denoise_input);
  oidn::BufferRef output_buffer = create_oidn_buffer(device, *denoise_output);

  oidn::FilterRef filter = device.newFilter("RT");
  const char *pass_name = get_pass_name(type);
  const int2 size = pass.domain().data_size;
  const int pixel_stride = sizeof(float) * denoise_input.channels_count();
  filter.setImage(pass_name, input_buffer, oidn::Format::Float3, size.x, size.y, 0, pixel_stride);
  filter.setImage("output", output_buffer, oidn::Format::Float3, size.x, size.y, 0, pixel_stride);
  filter.set("quality", quality);
  filter.setProgressMonitorFunction(oidn_progress_monitor_function, &context);
  filter.commit();
  filter.execute();

  if (output_buffer.getStorage() != oidn::Storage::Host) {
    output_buffer.read(
        0, denoise_output->size_in_bytes(), denoise_output->cpu_data_for_write().data());
  }

  if (is_denoising_in_place) {
    this->result.share_data(denoise_input);
  }
  denoise_input.release();
}

DenoisedAuxiliaryPass::~DenoisedAuxiliaryPass()
{
  this->result.release();
}

/* --------------------------------------------------------------------
 * Denoised Auxiliary Pass Container.
 */

DenoisedAuxiliaryPass &DenoisedAuxiliaryPassContainer::get(Context &context,
                                                           const Result &pass,
                                                           const DenoisedAuxiliaryPassType type,
                                                           const oidn::Quality quality)
{
  const DenoisedAuxiliaryPassKey key(type, quality);

  return *map_.lookup_or_add_cb(key, [&]() {
    return std::make_unique<DenoisedAuxiliaryPass>(context, pass, type, quality);
  });
}

}  // namespace blender::compositor

#endif
