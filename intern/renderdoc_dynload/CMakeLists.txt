# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  include
  ../../extern/renderdoc/include
)

set(INC_SYS
)

set(SRC
  intern/renderdoc_api.cc

  include/renderdoc_api.hh
)

blender_add_lib(bf_intern_renderdoc_dynload "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
