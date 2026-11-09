/* SPDX-FileCopyrightText: 2023 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#if defined(WITH_EMBREE_GPU)
#  undef ccl_gpu_kernel_signature
#  define ccl_gpu_kernel_signature(name, ...) \
    void oneapi_kernel_##name(KernelParamsOneAPI *ccl_restrict kernel_params, \
                              size_t kernel_global_size, \
                              size_t kernel_local_size, \
                              sycl::handler &cgh, \
                              __VA_ARGS__) \
    { \
      (void)(kernel_params); \
      cgh.parallel_for( \
          sycl::nd_range<1>(kernel_global_size, kernel_local_size), \
          [=](sycl::nd_item<1> item, sycl::kernel_handler oneapi_kernel_handler) { \
            KernelGlobalsGPU oneapi_kg{kernel_params, oneapi_kernel_handler};

/* The functions read the specialization constants through the kernel handler of their kg, the
 * Embree callbacks the kernel parameters. */
#  undef ccl_gpu_kernel_kg
#  define ccl_gpu_kernel_kg (&oneapi_kg)
#endif
