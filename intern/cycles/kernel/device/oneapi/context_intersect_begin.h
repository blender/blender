/* SPDX-FileCopyrightText: 2023 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#if !defined(WITH_ONEAPI_SYCL_HOST_TASK) && defined(WITH_EMBREE_GPU)
#  undef ccl_gpu_kernel_signature
#  define ccl_gpu_kernel_signature(name, ...) \
    void oneapi_kernel_##name(KernelGlobalsGPU *ccl_restrict kg, \
                              size_t kernel_global_size, \
                              size_t kernel_local_size, \
                              sycl::handler &cgh, \
                              __VA_ARGS__) \
    { \
      (kg); \
      cgh.parallel_for( \
          sycl::nd_range<1>(kernel_global_size, kernel_local_size), \
          [=](sycl::nd_item<1> item, sycl::kernel_handler oneapi_kernel_handler) { \
            ((ONEAPIKernelContext*)kg)->kernel_handler = oneapi_kernel_handler;
#endif
