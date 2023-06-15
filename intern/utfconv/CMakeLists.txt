# SPDX-FileCopyrightText: 2012-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
)

set(INC_SYS

)

set(SRC
  utfconv.c

  utfconv.h
)

set(LIB
)

# This is odd but leave it for now...
# Why have win32 check here? - this is only used for windows.
# ... because one day we might want to use it on other platforms.
if(WIN32)
  list(APPEND SRC
    utf_winfunc.c
    utf_winfunc.h
  )
endif()

blender_add_lib(bf_intern_utfconv "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
