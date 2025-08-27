# SPDX-FileCopyrightText: 2015 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

if(WITH_OPENVDB)
  set(INC_SYS
  )

  set(INC
    PUBLIC .
  )

  set(SRC
    openvdb_capi.h
    openvdb_fwd.hh
    openvdb_capi.cc
  )

  set(LIB
    PUBLIC bf::dependencies::optional::openvdb
  )

  blender_add_lib(bf_intern_openvdb "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
else()
  # Empty target
  add_library(bf_intern_openvdb INTERFACE)
endif()

add_library(bf::intern::optional::openvdb ALIAS bf_intern_openvdb)
