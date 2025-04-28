# SPDX-FileCopyrightText: 2024 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
)

set(INC_SYS

)

set(SRC
  uri_convert.cc

  uri_convert.hh
)

set(LIB
)

blender_add_lib(bf_intern_uriconvert "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
