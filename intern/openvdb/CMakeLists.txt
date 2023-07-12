# SPDX-FileCopyrightText: 2015 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
)

set(INC_SYS
)

set(SRC
  openvdb_capi.h
)

set(LIB
  PRIVATE bf::intern::guardedalloc
)

if(WITH_OPENVDB)
  add_definitions(-DWITH_OPENVDB ${OPENVDB_DEFINITIONS})

  list(APPEND INC_SYS
    ${OPENVDB_INCLUDE_DIRS}
  )

  list(APPEND SRC
    openvdb_capi.cc
  )

  list(APPEND LIB
    ${OPENVDB_LIBRARIES}
  )
endif()

blender_add_lib(bf_intern_openvdb "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
