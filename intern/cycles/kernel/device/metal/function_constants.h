/* SPDX-FileCopyrightText: 2021-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

enum {
  Kernel_DummyConstant,
#define KERNEL_STRUCT_MEMBER(parent, type, name) KernelData_##parent##_##name,
#include "kernel/data_template.h"

  /* On macOS 12+ KernelData_kernel_features are stored in a single 64bit integer.
   * On older macOS versions the value is split into two 32bit values as 64bit values are not
   * available. The version check is done at runtime. */
  KernelData_kernel_features_64bit,
  KernelData_kernel_features_hi,
  KernelData_kernel_features_lo,
};

#ifdef __KERNEL_METAL__
#  define KERNEL_STRUCT_MEMBER(parent, type, name) \
    constant type kernel_data_##parent##_##name \
        [[function_constant(KernelData_##parent##_##name)]];
#  include "kernel/data_template.h"

#  if defined(__METAL_FUNCTION_CONSTANTS_64BIT__)
constant ulong kernel_data_kernel_features [[function_constant(KernelData_kernel_features_64bit)]];
#  else
constant uint kernel_data_kernel_features_hi [[function_constant(KernelData_kernel_features_hi)]];
constant uint kernel_data_kernel_features_lo [[function_constant(KernelData_kernel_features_lo)]];
/* TODO(sergey): There might be a better way to combine lo and hi words. */
#    define kernel_data_kernel_features \
      ((uint64_t(kernel_data_kernel_features_hi) << 32) | uint64_t(kernel_data_kernel_features_lo))
#  endif

#endif
