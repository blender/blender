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

# This is odd but leave it for now...
# Why have win32 check here? - this is only used for windows.
# ... because one day we might want to use it on other platforms.
if(WIN32)
  list(APPEND SRC
    utf_winfunc.cc
    utf_winfunc.hh
  )
endif()

blender_add_lib(bf_intern_utfconv "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
add_library(bf::intern::utfconv ALIAS bf_intern_utfconv)
