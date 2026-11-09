/* SPDX-FileCopyrightText: 2023 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#if defined(WITH_EMBREE_GPU)
#  undef ccl_gpu_kernel_signature
#  define ccl_gpu_kernel_signature __ccl_gpu_kernel_signature
#  undef ccl_gpu_kernel_kg
#  define ccl_gpu_kernel_kg nullptr
#endif
