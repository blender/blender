# SPDX-FileCopyrightText: 2026 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  PUBLIC .
)

set(INC_SYS
)

# volk.c is not a translation unit, it is included by volk.h but we 
# still want it to show up in the various IDE projects, so mark the file
# as header only (even though it has a .c extension) to prevent the 
# build system from trying to build it.
set(VOLK_HEADER_UNITS
  volk.c
)
set_source_files_properties(${VOLK_HEADER_UNITS} PROPERTIES HEADER_FILE_ONLY TRUE)

set(SRC
  volk_impl.cc
  volk.h
  ${VOLK_HEADER_UNITS}
)

set(LIB
  PUBLIC bf::dependencies::optional::vulkan
)

blender_add_lib(extern_volk "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
add_library(bf::extern::volk ALIAS extern_volk)
