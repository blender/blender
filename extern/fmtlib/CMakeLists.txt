# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
)

set(INC_SYS
  PUBLIC include
)

set(SRC
  include/fmt/core.h
  include/fmt/format-inl.h
  include/fmt/format.h
  src/format.cc
)

set(LIB
)

blender_add_lib(extern_fmtlib "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
add_library(bf::extern::fmtlib ALIAS extern_fmtlib)
