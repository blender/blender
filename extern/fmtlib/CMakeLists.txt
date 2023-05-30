# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  include
)

set(INC_SYS
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
