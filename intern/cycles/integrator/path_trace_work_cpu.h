/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/device/cpu/globals.h"
#include "kernel/integrator/state.h"

#include "device/queue.h"

#include "integrator/path_trace_work.h"

#include "util/vector.h"

CCL_NAMESPACE_BEGIN

struct KernelWorkTile;
struct ThreadKernelGlobalsCPU;
struct IntegratorStateCPU;

class CPUKernels;

/* Implementation of PathTraceWork which schedules work on to queues pixel-by-pixel,
 * for CPU devices.
 *
 * NOTE: For the CPU rendering there are assumptions about TBB arena size and number of concurrent
 * queues on the render device which makes this work be only usable on CPU. */
class PathTraceWorkCPU : public PathTraceWork {
 public:
  PathTraceWorkCPU(Device *device,
                   Film *film,
                   DeviceScene *device_scene,
                   const bool *cancel_requested_flag);

  void init_execution() override;

  void render_samples(RenderStatistics &statistics,
                      const int start_sample,
                      const int samples_num,
                      const int sample_offset) override;

  void copy_to_display(PathTraceDisplay *display,
                       PassMode pass_mode,
                       const int num_samples) override;
  void destroy_gpu_resources(PathTraceDisplay *display) override;

  bool copy_render_buffers_from_device() override;
  bool copy_render_buffers_to_device() override;
  bool zero_render_buffers() override;

  int adaptive_sampling_converge_filter_count_active(const float threshold, bool reset) override;
  void cryptomatte_postproces() override;

#if defined(WITH_PATH_GUIDING)
  /* Initializes the per-thread guiding kernel data. The function sets the pointers to the
   * global guiding field and the sample data storage as well es initializes the per-thread
   * guided sampling distributions (e.g., SurfaceSamplingDistribution and
   * VolumeSamplingDistribution). */
  void guiding_init_kernel_globals(void *guiding_field,
                                   void *sample_data_storage,
                                   const bool train) override;

  /* Pushes the collected training data/samples of a path to the global sample storage.
   * This function is called at the end of a random walk/path generation. */
  void guiding_push_sample_data_to_global_storage(ThreadKernelGlobalsCPU *kg,
                                                  IntegratorStateCPU *state,
                                                  ccl_global float *ccl_restrict render_buffer);
#endif

 protected:
  /* Core path tracing routine. Renders given work time on the given queue. */
  void render_samples_full_pipeline(ThreadKernelGlobalsCPU *kernel_globals,
                                    const KernelWorkTile &work_tile,
                                    const int samples_num);

  /* CPU kernels. */
  const CPUKernels &kernels_;

  /* Copy of kernel globals which is suitable for concurrent access from multiple threads.
   *
   * More specifically, the `kernel_globals_` is local to each threads and nobody else is
   * accessing it, but some "localization" is required to decouple from kernel globals stored
   * on the device level. */
  vector<ThreadKernelGlobalsCPU> kernel_thread_globals_;
};

CCL_NAMESPACE_END
