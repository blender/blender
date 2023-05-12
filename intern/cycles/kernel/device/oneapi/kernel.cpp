/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2021-2022 Intel Corporation */

#ifdef WITH_ONEAPI

#  include "kernel.h"
#  include <iostream>
#  include <map>
#  include <set>

#  include <sycl/sycl.hpp>

#  include "kernel/device/oneapi/compat.h"
#  include "kernel/device/oneapi/globals.h"
#  include "kernel/device/oneapi/kernel_templates.h"

#  include "kernel/device/gpu/kernel.h"

#  include "device/kernel.cpp"

static OneAPIErrorCallback s_error_cb = nullptr;
static void *s_error_user_ptr = nullptr;

#  ifdef WITH_EMBREE_GPU
static const RTCFeatureFlags CYCLES_ONEAPI_EMBREE_BASIC_FEATURES =
    (const RTCFeatureFlags)(RTC_FEATURE_FLAG_TRIANGLE | RTC_FEATURE_FLAG_INSTANCE |
                            RTC_FEATURE_FLAG_FILTER_FUNCTION_IN_ARGUMENTS |
                            RTC_FEATURE_FLAG_POINT | RTC_FEATURE_FLAG_MOTION_BLUR);
static const RTCFeatureFlags CYCLES_ONEAPI_EMBREE_ALL_FEATURES =
    (const RTCFeatureFlags)(CYCLES_ONEAPI_EMBREE_BASIC_FEATURES |
                            RTC_FEATURE_FLAG_ROUND_CATMULL_ROM_CURVE |
                            RTC_FEATURE_FLAG_FLAT_CATMULL_ROM_CURVE);
#  endif

void oneapi_set_error_cb(OneAPIErrorCallback cb, void *user_ptr)
{
  s_error_cb = cb;
  s_error_user_ptr = user_ptr;
}

/* NOTE(@nsirgien): Execution of this simple kernel will check basic functionality like
 * memory allocations, memory transfers and execution of kernel with USM memory. */
bool oneapi_run_test_kernel(SyclQueue *queue_)
{
  assert(queue_);
  sycl::queue *queue = reinterpret_cast<sycl::queue *>(queue_);
  const size_t N = 8;
  const size_t memory_byte_size = sizeof(int) * N;

  bool is_computation_correct = true;
  try {
    int *A_host = (int *)sycl::aligned_alloc_host(16, memory_byte_size, *queue);

    for (size_t i = (size_t)0; i < N; i++) {
      A_host[i] = rand() % 32;
    }

    int *A_device = (int *)sycl::malloc_device(memory_byte_size, *queue);
    int *B_device = (int *)sycl::malloc_device(memory_byte_size, *queue);

    queue->memcpy(A_device, A_host, memory_byte_size);
    queue->wait_and_throw();

    queue->submit([&](sycl::handler &cgh) {
      cgh.parallel_for(N, [=](sycl::id<1> idx) { B_device[idx] = A_device[idx] + idx.get(0); });
    });
    queue->wait_and_throw();

    int *B_host = (int *)sycl::aligned_alloc_host(16, memory_byte_size, *queue);

    queue->memcpy(B_host, B_device, memory_byte_size);
    queue->wait_and_throw();

    for (size_t i = (size_t)0; i < N; i++) {
      const int expected_result = i + A_host[i];
      if (B_host[i] != expected_result) {
        is_computation_correct = false;
        if (s_error_cb) {
          s_error_cb(("Incorrect result in test kernel execution -  expected " +
                      std::to_string(expected_result) + ", got " + std::to_string(B_host[i]))
                         .c_str(),
                     s_error_user_ptr);
        }
      }
    }

    sycl::free(A_host, *queue);
    sycl::free(B_host, *queue);
    sycl::free(A_device, *queue);
    sycl::free(B_device, *queue);
    queue->wait_and_throw();
  }
  catch (sycl::exception const &e) {
    if (s_error_cb) {
      s_error_cb(e.what(), s_error_user_ptr);
    }
    return false;
  }

  return is_computation_correct;
}

/* TODO: Move device information to OneapiDevice initialized on creation and use it. */
/* TODO: Move below function to oneapi/queue.cpp. */
size_t oneapi_kernel_preferred_local_size(SyclQueue *queue,
                                          const DeviceKernel kernel,
                                          const size_t kernel_global_size)
{
  assert(queue);
  (void)kernel_global_size;
  const static size_t preferred_work_group_size_intersect_shading = 32;
  const static size_t preferred_work_group_size_technical = 1024;

  size_t preferred_work_group_size = 0;
  switch (kernel) {
    case DEVICE_KERNEL_INTEGRATOR_INIT_FROM_CAMERA:
    case DEVICE_KERNEL_INTEGRATOR_INIT_FROM_BAKE:
    case DEVICE_KERNEL_INTEGRATOR_INTERSECT_CLOSEST:
    case DEVICE_KERNEL_INTEGRATOR_INTERSECT_SHADOW:
    case DEVICE_KERNEL_INTEGRATOR_INTERSECT_SUBSURFACE:
    case DEVICE_KERNEL_INTEGRATOR_INTERSECT_VOLUME_STACK:
    case DEVICE_KERNEL_INTEGRATOR_SHADE_BACKGROUND:
    case DEVICE_KERNEL_INTEGRATOR_SHADE_LIGHT:
    case DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE:
    case DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE_RAYTRACE:
    case DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE_MNEE:
    case DEVICE_KERNEL_INTEGRATOR_SHADE_VOLUME:
    case DEVICE_KERNEL_INTEGRATOR_SHADE_SHADOW:
      preferred_work_group_size = preferred_work_group_size_intersect_shading;
      break;

    case DEVICE_KERNEL_INTEGRATOR_QUEUED_PATHS_ARRAY:
    case DEVICE_KERNEL_INTEGRATOR_QUEUED_SHADOW_PATHS_ARRAY:
    case DEVICE_KERNEL_INTEGRATOR_ACTIVE_PATHS_ARRAY:
    case DEVICE_KERNEL_INTEGRATOR_TERMINATED_PATHS_ARRAY:
    case DEVICE_KERNEL_INTEGRATOR_SORTED_PATHS_ARRAY:
    case DEVICE_KERNEL_INTEGRATOR_COMPACT_PATHS_ARRAY:
    case DEVICE_KERNEL_INTEGRATOR_COMPACT_STATES:
    case DEVICE_KERNEL_INTEGRATOR_TERMINATED_SHADOW_PATHS_ARRAY:
    case DEVICE_KERNEL_INTEGRATOR_COMPACT_SHADOW_PATHS_ARRAY:
    case DEVICE_KERNEL_INTEGRATOR_COMPACT_SHADOW_STATES:
    case DEVICE_KERNEL_INTEGRATOR_RESET:
    case DEVICE_KERNEL_INTEGRATOR_SHADOW_CATCHER_COUNT_POSSIBLE_SPLITS:
      preferred_work_group_size = preferred_work_group_size_technical;
      break;

    default:
      preferred_work_group_size = 512;
  }

  const size_t limit_work_group_size = reinterpret_cast<sycl::queue *>(queue)
                                           ->get_device()
                                           .get_info<sycl::info::device::max_work_group_size>();

  return std::min(limit_work_group_size, preferred_work_group_size);
}

bool oneapi_kernel_is_required_for_features(const std::string &kernel_name,
                                            const uint kernel_features)
{
  if ((kernel_features & KERNEL_FEATURE_NODE_RAYTRACE) == 0 &&
      kernel_name.find(device_kernel_as_string(DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE_RAYTRACE)) !=
          std::string::npos)
    return false;
  if ((kernel_features & KERNEL_FEATURE_MNEE) == 0 &&
      kernel_name.find(device_kernel_as_string(DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE_MNEE)) !=
          std::string::npos)
    return false;
  if ((kernel_features & KERNEL_FEATURE_VOLUME) == 0 &&
      kernel_name.find(device_kernel_as_string(DEVICE_KERNEL_INTEGRATOR_INTERSECT_VOLUME_STACK)) !=
          std::string::npos)
    return false;

  return true;
}

bool oneapi_kernel_is_compatible_with_hardware_raytracing(const std::string &kernel_name)
{
  /* MNEE and Raytrace kernels work correctly with Hardware Raytracing starting with Embree 4.1.
   */
#  if defined(RTC_VERSION) && RTC_VERSION < 40100
  return (kernel_name.find(device_kernel_as_string(DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE_MNEE)) ==
          std::string::npos) &&
         (kernel_name.find(device_kernel_as_string(
              DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE_RAYTRACE)) == std::string::npos);
#  else
  return true;
#  endif
}

bool oneapi_kernel_has_intersections(const std::string &kernel_name)
{
  for (int i = 0; i < (int)DEVICE_KERNEL_NUM; i++) {
    DeviceKernel kernel = (DeviceKernel)i;
    if (device_kernel_has_intersection(kernel)) {
      if (kernel_name.find(device_kernel_as_string(kernel)) != std::string::npos) {
        return true;
      }
    }
  }
  return false;
}

bool oneapi_load_kernels(SyclQueue *queue_,
                         const uint kernel_features,
                         bool use_hardware_raytracing)
{
  assert(queue_);
  sycl::queue *queue = reinterpret_cast<sycl::queue *>(queue_);

#  ifdef WITH_EMBREE_GPU
  /* For best performance, we always JIT compile the kernels that are using Embree. */
  if (use_hardware_raytracing) {
    try {
      sycl::kernel_bundle<sycl::bundle_state::input> all_kernels_bundle =
          sycl::get_kernel_bundle<sycl::bundle_state::input>(queue->get_context(),
                                                             {queue->get_device()});

      for (const sycl::kernel_id &kernel_id : all_kernels_bundle.get_kernel_ids()) {
        const std::string &kernel_name = kernel_id.get_name();

        if (!oneapi_kernel_is_required_for_features(kernel_name, kernel_features) ||
            !(oneapi_kernel_has_intersections(kernel_name) &&
              oneapi_kernel_is_compatible_with_hardware_raytracing(kernel_name)))
        {
          continue;
        }

        sycl::kernel_bundle<sycl::bundle_state::input> one_kernel_bundle_input =
            sycl::get_kernel_bundle<sycl::bundle_state::input>(queue->get_context(), {kernel_id});

        /* Hair requires embree curves support. */
        if (kernel_features & KERNEL_FEATURE_HAIR) {
          one_kernel_bundle_input
              .set_specialization_constant<ONEAPIKernelContext::oneapi_embree_features>(
                  CYCLES_ONEAPI_EMBREE_ALL_FEATURES);
          sycl::build(one_kernel_bundle_input);
        }
        else {
          one_kernel_bundle_input
              .set_specialization_constant<ONEAPIKernelContext::oneapi_embree_features>(
                  CYCLES_ONEAPI_EMBREE_BASIC_FEATURES);
          sycl::build(one_kernel_bundle_input);
        }
      }
    }
    catch (sycl::exception const &e) {
      if (s_error_cb) {
        s_error_cb(e.what(), s_error_user_ptr);
      }
      return false;
    }
  }
#  endif

  try {
    sycl::kernel_bundle<sycl::bundle_state::input> all_kernels_bundle =
        sycl::get_kernel_bundle<sycl::bundle_state::input>(queue->get_context(),
                                                           {queue->get_device()});

    for (const sycl::kernel_id &kernel_id : all_kernels_bundle.get_kernel_ids()) {
      const std::string &kernel_name = kernel_id.get_name();

      /* In case HWRT is on, compilation of kernels using Embree is already handled in previous
       * block. */
      if (!oneapi_kernel_is_required_for_features(kernel_name, kernel_features) ||
          (use_hardware_raytracing && oneapi_kernel_has_intersections(kernel_name) &&
           oneapi_kernel_is_compatible_with_hardware_raytracing(kernel_name)))
      {
        continue;
      }

#  ifdef WITH_EMBREE_GPU
      if (oneapi_kernel_has_intersections(kernel_name)) {
        sycl::kernel_bundle<sycl::bundle_state::input> one_kernel_bundle_input =
            sycl::get_kernel_bundle<sycl::bundle_state::input>(queue->get_context(), {kernel_id});
        one_kernel_bundle_input
            .set_specialization_constant<ONEAPIKernelContext::oneapi_embree_features>(
                RTC_FEATURE_FLAG_NONE);
        sycl::build(one_kernel_bundle_input);
        continue;
      }
#  endif
      /* This call will ensure that AoT or cached JIT binaries are available
       * for execution. It will trigger compilation if it is not already the case. */
      (void)sycl::get_kernel_bundle<sycl::bundle_state::executable>(queue->get_context(),
                                                                    {kernel_id});
    }
  }
  catch (sycl::exception const &e) {
    if (s_error_cb) {
      s_error_cb(e.what(), s_error_user_ptr);
    }
    return false;
  }
  return true;
}

bool oneapi_enqueue_kernel(KernelContext *kernel_context,
                           int kernel,
                           size_t global_size,
                           const uint kernel_features,
                           bool use_hardware_raytracing,
                           void **args)
{
  bool success = true;
  ::DeviceKernel device_kernel = (::DeviceKernel)kernel;
  KernelGlobalsGPU *kg = (KernelGlobalsGPU *)kernel_context->kernel_globals;
  sycl::queue *queue = reinterpret_cast<sycl::queue *>(kernel_context->queue);
  assert(queue);
  if (!queue) {
    return false;
  }

  size_t local_size = oneapi_kernel_preferred_local_size(
      kernel_context->queue, device_kernel, global_size);
  assert(global_size % local_size == 0);

  /* Local size for DEVICE_KERNEL_INTEGRATOR_ACTIVE_PATHS_ARRAY needs to be enforced so we
   * overwrite it outside of oneapi_kernel_preferred_local_size. */
  if (device_kernel == DEVICE_KERNEL_INTEGRATOR_ACTIVE_PATHS_ARRAY) {
    local_size = GPU_PARALLEL_ACTIVE_INDEX_DEFAULT_BLOCK_SIZE;
  }

  /* Kernels listed below need a specific number of work groups. */
  if (device_kernel == DEVICE_KERNEL_INTEGRATOR_ACTIVE_PATHS_ARRAY ||
      device_kernel == DEVICE_KERNEL_INTEGRATOR_QUEUED_PATHS_ARRAY ||
      device_kernel == DEVICE_KERNEL_INTEGRATOR_QUEUED_SHADOW_PATHS_ARRAY ||
      device_kernel == DEVICE_KERNEL_INTEGRATOR_TERMINATED_PATHS_ARRAY ||
      device_kernel == DEVICE_KERNEL_INTEGRATOR_TERMINATED_SHADOW_PATHS_ARRAY ||
      device_kernel == DEVICE_KERNEL_INTEGRATOR_COMPACT_PATHS_ARRAY ||
      device_kernel == DEVICE_KERNEL_INTEGRATOR_COMPACT_SHADOW_PATHS_ARRAY)
  {
    int num_states = *((int *)(args[0]));
    /* Round up to the next work-group. */
    size_t groups_count = (num_states + local_size - 1) / local_size;
    /* NOTE(@nsirgien): As for now non-uniform work-groups don't work on most oneAPI devices,
     * we extend work size to fit uniformity requirements. */
    global_size = groups_count * local_size;

#  ifdef WITH_ONEAPI_SYCL_HOST_TASK
    /* Path array implementation is serial in case of SYCL Host Task execution. */
    global_size = 1;
    local_size = 1;
#  endif
  }

  /* Let the compiler throw an error if there are any kernels missing in this implementation. */
#  if defined(_WIN32)
#    pragma warning(error : 4062)
#  elif defined(__GNUC__)
#    pragma GCC diagnostic push
#    pragma GCC diagnostic error "-Wswitch"
#  endif

  try {
    queue->submit([&](sycl::handler &cgh) {
#  ifdef WITH_EMBREE_GPU
      /* Spec says it has no effect if the called kernel doesn't support the below specialization
       * constant but it can still trigger a recompilation, so we set it only if needed. */
      if (device_kernel_has_intersection(device_kernel)) {
        const RTCFeatureFlags used_embree_features = !use_hardware_raytracing ?
                                                         RTC_FEATURE_FLAG_NONE :
                                                     !(kernel_features & KERNEL_FEATURE_HAIR) ?
                                                         CYCLES_ONEAPI_EMBREE_BASIC_FEATURES :
                                                         CYCLES_ONEAPI_EMBREE_ALL_FEATURES;
        cgh.set_specialization_constant<ONEAPIKernelContext::oneapi_embree_features>(
            used_embree_features);
      }
#  else
      (void)kernel_features;
#  endif
      switch (device_kernel) {
        case DEVICE_KERNEL_INTEGRATOR_RESET: {
          oneapi_call(kg, cgh, global_size, local_size, args, oneapi_kernel_integrator_reset);
          break;
        }
        case DEVICE_KERNEL_INTEGRATOR_INIT_FROM_CAMERA: {
          oneapi_call(
              kg, cgh, global_size, local_size, args, oneapi_kernel_integrator_init_from_camera);
          break;
        }
        case DEVICE_KERNEL_INTEGRATOR_INIT_FROM_BAKE: {
          oneapi_call(
              kg, cgh, global_size, local_size, args, oneapi_kernel_integrator_init_from_bake);
          break;
        }
        case DEVICE_KERNEL_INTEGRATOR_INTERSECT_CLOSEST: {
          oneapi_call(
              kg, cgh, global_size, local_size, args, oneapi_kernel_integrator_intersect_closest);
          break;
        }
        case DEVICE_KERNEL_INTEGRATOR_INTERSECT_SHADOW: {
          oneapi_call(
              kg, cgh, global_size, local_size, args, oneapi_kernel_integrator_intersect_shadow);
          break;
        }
        case DEVICE_KERNEL_INTEGRATOR_INTERSECT_SUBSURFACE: {
          oneapi_call(kg,
                      cgh,
                      global_size,
                      local_size,
                      args,
                      oneapi_kernel_integrator_intersect_subsurface);
          break;
        }
        case DEVICE_KERNEL_INTEGRATOR_INTERSECT_VOLUME_STACK: {
          oneapi_call(kg,
                      cgh,
                      global_size,
                      local_size,
                      args,
                      oneapi_kernel_integrator_intersect_volume_stack);
          break;
        }
        case DEVICE_KERNEL_INTEGRATOR_SHADE_BACKGROUND: {
          oneapi_call(
              kg, cgh, global_size, local_size, args, oneapi_kernel_integrator_shade_background);
          break;
        }
        case DEVICE_KERNEL_INTEGRATOR_SHADE_LIGHT: {
          oneapi_call(
              kg, cgh, global_size, local_size, args, oneapi_kernel_integrator_shade_light);
          break;
        }
        case DEVICE_KERNEL_INTEGRATOR_SHADE_SHADOW: {
          oneapi_call(
              kg, cgh, global_size, local_size, args, oneapi_kernel_integrator_shade_shadow);
          break;
        }
        case DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE: {
          oneapi_call(
              kg, cgh, global_size, local_size, args, oneapi_kernel_integrator_shade_surface);
          break;
        }
        case DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE_RAYTRACE: {
          oneapi_call(kg,
                      cgh,
                      global_size,
                      local_size,
                      args,
                      oneapi_kernel_integrator_shade_surface_raytrace);
          break;
        }
        case DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE_MNEE: {
          oneapi_call(
              kg, cgh, global_size, local_size, args, oneapi_kernel_integrator_shade_surface_mnee);
          break;
        }
        case DEVICE_KERNEL_INTEGRATOR_SHADE_VOLUME: {
          oneapi_call(
              kg, cgh, global_size, local_size, args, oneapi_kernel_integrator_shade_volume);
          break;
        }
        case DEVICE_KERNEL_INTEGRATOR_QUEUED_PATHS_ARRAY: {
          oneapi_call(
              kg, cgh, global_size, local_size, args, oneapi_kernel_integrator_queued_paths_array);
          break;
        }
        case DEVICE_KERNEL_INTEGRATOR_QUEUED_SHADOW_PATHS_ARRAY: {
          oneapi_call(kg,
                      cgh,
                      global_size,
                      local_size,
                      args,
                      oneapi_kernel_integrator_queued_shadow_paths_array);
          break;
        }
        case DEVICE_KERNEL_INTEGRATOR_ACTIVE_PATHS_ARRAY: {
          oneapi_call(
              kg, cgh, global_size, local_size, args, oneapi_kernel_integrator_active_paths_array);
          break;
        }
        case DEVICE_KERNEL_INTEGRATOR_TERMINATED_PATHS_ARRAY: {
          oneapi_call(kg,
                      cgh,
                      global_size,
                      local_size,
                      args,
                      oneapi_kernel_integrator_terminated_paths_array);
          break;
        }
        case DEVICE_KERNEL_INTEGRATOR_TERMINATED_SHADOW_PATHS_ARRAY: {
          oneapi_call(kg,
                      cgh,
                      global_size,
                      local_size,
                      args,
                      oneapi_kernel_integrator_terminated_shadow_paths_array);
          break;
        }
        case DEVICE_KERNEL_INTEGRATOR_SORTED_PATHS_ARRAY: {
          oneapi_call(
              kg, cgh, global_size, local_size, args, oneapi_kernel_integrator_sorted_paths_array);
          break;
        }
        case DEVICE_KERNEL_INTEGRATOR_SORT_BUCKET_PASS: {
          oneapi_call(
              kg, cgh, global_size, local_size, args, oneapi_kernel_integrator_sort_bucket_pass);
          break;
        }
        case DEVICE_KERNEL_INTEGRATOR_SORT_WRITE_PASS: {
          oneapi_call(
              kg, cgh, global_size, local_size, args, oneapi_kernel_integrator_sort_write_pass);
          break;
        }
        case DEVICE_KERNEL_INTEGRATOR_COMPACT_PATHS_ARRAY: {
          oneapi_call(kg,
                      cgh,
                      global_size,
                      local_size,
                      args,
                      oneapi_kernel_integrator_compact_paths_array);
          break;
        }
        case DEVICE_KERNEL_INTEGRATOR_COMPACT_SHADOW_PATHS_ARRAY: {
          oneapi_call(kg,
                      cgh,
                      global_size,
                      local_size,
                      args,
                      oneapi_kernel_integrator_compact_shadow_paths_array);
          break;
        }
        case DEVICE_KERNEL_ADAPTIVE_SAMPLING_CONVERGENCE_CHECK: {
          oneapi_call(kg,
                      cgh,
                      global_size,
                      local_size,
                      args,
                      oneapi_kernel_adaptive_sampling_convergence_check);
          break;
        }
        case DEVICE_KERNEL_ADAPTIVE_SAMPLING_CONVERGENCE_FILTER_X: {
          oneapi_call(
              kg, cgh, global_size, local_size, args, oneapi_kernel_adaptive_sampling_filter_x);
          break;
        }
        case DEVICE_KERNEL_ADAPTIVE_SAMPLING_CONVERGENCE_FILTER_Y: {
          oneapi_call(
              kg, cgh, global_size, local_size, args, oneapi_kernel_adaptive_sampling_filter_y);
          break;
        }
        case DEVICE_KERNEL_SHADER_EVAL_DISPLACE: {
          oneapi_call(kg, cgh, global_size, local_size, args, oneapi_kernel_shader_eval_displace);
          break;
        }
        case DEVICE_KERNEL_SHADER_EVAL_BACKGROUND: {
          oneapi_call(
              kg, cgh, global_size, local_size, args, oneapi_kernel_shader_eval_background);
          break;
        }
        case DEVICE_KERNEL_SHADER_EVAL_CURVE_SHADOW_TRANSPARENCY: {
          oneapi_call(kg,
                      cgh,
                      global_size,
                      local_size,
                      args,
                      oneapi_kernel_shader_eval_curve_shadow_transparency);
          break;
        }
        case DEVICE_KERNEL_PREFIX_SUM: {
          oneapi_call(kg, cgh, global_size, local_size, args, oneapi_kernel_prefix_sum);
          break;
        }

        /* clang-format off */
    #  define DEVICE_KERNEL_FILM_CONVERT_PARTIAL(VARIANT, variant) \
    case DEVICE_KERNEL_FILM_CONVERT_##VARIANT: { \
      oneapi_call(kg, cgh, \
                            global_size, \
                            local_size, \
                            args, \
                            oneapi_kernel_film_convert_##variant); \
      break; \
     }

#  define DEVICE_KERNEL_FILM_CONVERT(variant, VARIANT) \
      DEVICE_KERNEL_FILM_CONVERT_PARTIAL(VARIANT, variant) \
      DEVICE_KERNEL_FILM_CONVERT_PARTIAL(VARIANT##_HALF_RGBA, variant##_half_rgba)

      DEVICE_KERNEL_FILM_CONVERT(depth, DEPTH);
      DEVICE_KERNEL_FILM_CONVERT(mist, MIST);
      DEVICE_KERNEL_FILM_CONVERT(sample_count, SAMPLE_COUNT);
      DEVICE_KERNEL_FILM_CONVERT(float, FLOAT);
      DEVICE_KERNEL_FILM_CONVERT(light_path, LIGHT_PATH);
      DEVICE_KERNEL_FILM_CONVERT(float3, FLOAT3);
      DEVICE_KERNEL_FILM_CONVERT(motion, MOTION);
      DEVICE_KERNEL_FILM_CONVERT(cryptomatte, CRYPTOMATTE);
      DEVICE_KERNEL_FILM_CONVERT(shadow_catcher, SHADOW_CATCHER);
      DEVICE_KERNEL_FILM_CONVERT(shadow_catcher_matte_with_shadow,
                                 SHADOW_CATCHER_MATTE_WITH_SHADOW);
      DEVICE_KERNEL_FILM_CONVERT(combined, COMBINED);
      DEVICE_KERNEL_FILM_CONVERT(float4, FLOAT4);

#  undef DEVICE_KERNEL_FILM_CONVERT
#  undef DEVICE_KERNEL_FILM_CONVERT_PARTIAL
          /* clang-format on */

        case DEVICE_KERNEL_FILTER_GUIDING_PREPROCESS: {
          oneapi_call(
              kg, cgh, global_size, local_size, args, oneapi_kernel_filter_guiding_preprocess);
          break;
        }
        case DEVICE_KERNEL_FILTER_GUIDING_SET_FAKE_ALBEDO: {
          oneapi_call(kg,
                      cgh,
                      global_size,
                      local_size,
                      args,
                      oneapi_kernel_filter_guiding_set_fake_albedo);
          break;
        }
        case DEVICE_KERNEL_FILTER_COLOR_PREPROCESS: {
          oneapi_call(
              kg, cgh, global_size, local_size, args, oneapi_kernel_filter_color_preprocess);
          break;
        }
        case DEVICE_KERNEL_FILTER_COLOR_POSTPROCESS: {
          oneapi_call(
              kg, cgh, global_size, local_size, args, oneapi_kernel_filter_color_postprocess);
          break;
        }
        case DEVICE_KERNEL_CRYPTOMATTE_POSTPROCESS: {
          oneapi_call(
              kg, cgh, global_size, local_size, args, oneapi_kernel_cryptomatte_postprocess);
          break;
        }
        case DEVICE_KERNEL_INTEGRATOR_COMPACT_STATES: {
          oneapi_call(
              kg, cgh, global_size, local_size, args, oneapi_kernel_integrator_compact_states);
          break;
        }
        case DEVICE_KERNEL_INTEGRATOR_COMPACT_SHADOW_STATES: {
          oneapi_call(kg,
                      cgh,
                      global_size,
                      local_size,
                      args,
                      oneapi_kernel_integrator_compact_shadow_states);
          break;
        }
        case DEVICE_KERNEL_INTEGRATOR_SHADOW_CATCHER_COUNT_POSSIBLE_SPLITS: {
          oneapi_call(kg,
                      cgh,
                      global_size,
                      local_size,
                      args,
                      oneapi_kernel_integrator_shadow_catcher_count_possible_splits);
          break;
        }
        /* Unsupported kernels */
        case DEVICE_KERNEL_NUM:
        case DEVICE_KERNEL_INTEGRATOR_MEGAKERNEL:
          kernel_assert(0);
          break;
      }
    });
  }
  catch (sycl::exception const &e) {
    if (s_error_cb) {
      s_error_cb(e.what(), s_error_user_ptr);
      success = false;
    }
  }

#  if defined(_WIN32)
#    pragma warning(default : 4062)
#  elif defined(__GNUC__)
#    pragma GCC diagnostic pop
#  endif
  return success;
}

#endif /* WITH_ONEAPI */
