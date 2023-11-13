# SPDX-FileCopyrightText: 2002-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  PUBLIC .
  ../../source/blender/blenlib
)

set(SRC
  nanosvg.h
  nanosvgrast.h
  blender_nanosvg.c
  blender_raster.c
)

set(LIB
)

blender_add_lib(extern_nanosvg "${SRC}" "${INC}" "" "${LIB}")
add_library(bf::extern::nanosvg ALIAS extern_nanosvg)
