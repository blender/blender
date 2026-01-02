# SPDX-FileCopyrightText: 2011-2026 Blender Foundation
#
# SPDX-License-Identifier: Apache-2.0

set(INC
  ../../..
)

set(INC_SYS

)

set(SRC_KERNEL_DEVICE_CPU
  globals.cpp
  kernel.cpp
  kernel_avx2.cpp
)

set(SRC_KERNEL_DEVICE_CPU_HEADERS
  bvh.h
  compat.h
  image.h
  globals.h
  kernel.h
  kernel_arch.h
  kernel_arch_impl.h
)

set(LIB

)

# -----------------------------------------------------------------------------
# CPU module.

include_directories(${INC})
include_directories(SYSTEM ${INC_SYS})

if(DEFINED CYCLES_KERNEL_FLAGS)
  set_source_files_properties(kernel.cpp PROPERTIES COMPILE_FLAGS "${CYCLES_KERNEL_FLAGS}")
endif()

if(CXX_HAS_AVX2)
  set_source_files_properties(kernel_avx2.cpp PROPERTIES COMPILE_FLAGS "${CYCLES_AVX2_FLAGS}")
endif()

# Warnings to avoid using doubles in the kernel.
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_C_COMPILER_ID MATCHES "Clang")
  add_check_cxx_compiler_flags(
    CMAKE_CXX_FLAGS
    _has_cxxflag_float_conversion "-Werror=float-conversion"
    _has_cxxflag_double_promotion "-Werror=double-promotion"
  )
  unset(_has_cxxflag_float_conversion)
  unset(_has_cxxflag_double_promotion)
endif()

if(WITH_CYCLES_OSL)
  list(APPEND LIB
    cycles_kernel_osl
  )
endif()

cycles_add_library(cycles_kernel_cpu "${LIB}"
  ${SRC_KERNEL_DEVICE_CPU}
  ${SRC_KERNEL_DEVICE_CPU_HEADERS}
)
cycles_set_solution_folder(cycles_kernel_cpu)

source_group("device\\cpu" FILES ${SRC_KERNEL_DEVICE_CPU} ${SRC_KERNEL_DEVICE_CPU_HEADERS})
