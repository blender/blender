# SPDX-FileCopyrightText: 2012-2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  PUBLIC .
)

set(INC_SYS

)

set(SRC
  utfconv.cc

  utfconv.hh
)

set(LIB
)

# This library is currently windows only, for the other platforms an empty target is created this
# way we don't have to clutter if(WIN32) ... endif() all over the place to use this library.
if(WIN32)
  list(APPEND SRC
    utf_winfunc.cc
    utf_winfunc.hh
  )
  blender_add_lib(bf_intern_utfconv "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
else()
  add_library(bf_intern_utfconv INTERFACE)
endif()

add_library(bf::intern::utfconv ALIAS bf_intern_utfconv)
