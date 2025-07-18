/* SPDX-FileCopyrightText: 2021-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

enum {
  Kernel_DummyConstant,
#define KERNEL_STRUCT_MEMBER(parent, type, name) KernelData_##parent##_##name,
#include "kernel/data_template.h"

  KernelData_kernel_features
};

#ifdef __KERNEL_METAL__
#  define KERNEL_STRUCT_MEMBER(parent, type, name) \
    constant type kernel_data_##parent##_##name \
        [[function_constant(KernelData_##parent##_##name)]];
#  include "kernel/data_template.h"

constant int kernel_data_kernel_features [[function_constant(KernelData_kernel_features)]];
#endif
