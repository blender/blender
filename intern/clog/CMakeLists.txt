# SPDX-FileCopyrightText: 2002-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  PUBLIC .
)

set(INC_SYS

)

set(SRC
  clog.c

  CLG_log.h
)

set(LIB
  PRIVATE bf::intern::guardedalloc
  PRIVATE bf::intern::atomic
)

blender_add_lib(bf_intern_clog "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

# Disabled for makesdna/makesrna.
target_compile_definitions(bf_intern_clog PRIVATE WITH_CLOG_PTHREADS)

add_library(bf::intern::clog ALIAS bf_intern_clog)
