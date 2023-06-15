/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

/* TODO(sergey): The integrator folder might not be the best. Is easy to move files around if the
 * better place is figured out. */

#include "device/denoise.h"
#include "device/device.h"
#include "util/function.h"
#include "util/unique_ptr.h"

CCL_NAMESPACE_BEGIN

class BufferParams;
class Device;
class RenderBuffers;
class Progress;

/* Implementation of a specific denoising algorithm.
 *
 * This class takes care of breaking down denoising algorithm into a series of device calls or to
 * calls of an external API to denoise given input.
 *
 * TODO(sergey): Are we better with device or a queue here? */
class Denoiser {
 public:
  /* Create denoiser for the given path trace device.
   *
   * Notes:
   * - The denoiser must be configured. This means that `params.use` must be true.
   *   This is checked in debug builds.
   * - The device might be MultiDevice. */
  static unique_ptr<Denoiser> create(Device *path_trace_device, const DenoiseParams &params);

  virtual ~Denoiser() = default;

  void set_params(const DenoiseParams &params);
  const DenoiseParams &get_params() const;

  /* Create devices and load kernels needed for denoising.
   * The progress is used to communicate state when kernels actually needs to be loaded.
   *
   * NOTE: The `progress` is an optional argument, can be nullptr. */
  virtual bool load_kernels(Progress *progress);

  /* Denoise the entire buffer.
   *
   * Buffer parameters denotes an effective parameters used during rendering. It could be
   * a lower resolution render into a bigger allocated buffer, which is used in viewport during
   * navigation and non-unit pixel size. Use that instead of render_buffers->params.
   *
   * The buffer might be coming from a "foreign" device from what this denoise is created for.
   * This means that in general case the denoiser will make sure the input data is available on
   * the denoiser device, perform denoising, and put data back to the device where the buffer
   * came from.
   *
   * The `num_samples` corresponds to the number of samples in the render buffers. It is used
   * to scale buffers down to the "final" value in algorithms which don't do automatic exposure,
   * or which needs "final" value for data passes.
   *
   * The `allow_inplace_modification` means that the denoiser is allowed to do in-place
   * modification of the input passes (scaling them down i.e.). This will lower the memory
   * footprint of the denoiser but will make input passes "invalid" (from path tracer) point of
   * view.
   *
   * Returns true when all passes are denoised. Will return false if there is a denoiser error (for
   * example, caused by misconfigured denoiser) or when user requested to cancel rendering. */
  virtual bool denoise_buffer(const BufferParams &buffer_params,
                              RenderBuffers *render_buffers,
                              const int num_samples,
                              bool allow_inplace_modification) = 0;

  /* Get a device which is used to perform actual denoising.
   *
   * Notes:
   *
   * - The device is lazily initialized via `load_kernels()`, so it will be nullptr until then,
   *
   * - The device can be different from the path tracing device. This happens, for example, when
   *   using OptiX denoiser and rendering on CPU.
   *
   * - No threading safety is ensured in this call. This means, that it is up to caller to ensure
   *   that there is no threading-conflict between denoising task lazily initializing the device
   *   and access to this device happen. */
  Device *get_denoiser_device() const;

  function<bool(void)> is_cancelled_cb;

  bool is_cancelled() const
  {
    if (!is_cancelled_cb) {
      return false;
    }
    return is_cancelled_cb();
  }

 protected:
  Denoiser(Device *path_trace_device, const DenoiseParams &params);

  /* Make sure denoising device is initialized. */
  virtual Device *ensure_denoiser_device(Progress *progress);

  /* Get device type mask which is used to filter available devices when new device needs to be
   * created. */
  virtual uint get_device_type_mask() const = 0;

  Device *path_trace_device_;
  DenoiseParams params_;

  /* Cached pointer to the device on which denoising will happen.
   * Used to avoid lookup of a device for every denoising request. */
  Device *denoiser_device_ = nullptr;

  /* Denoiser device which was created to perform denoising in the case the none of the rendering
   * devices are capable of denoising. */
  unique_ptr<Device> local_denoiser_device_;
  bool device_creation_attempted_ = false;
};

CCL_NAMESPACE_END
