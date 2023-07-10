# SPDX-FileCopyrightText: 2006 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

remove_extra_strict_flags()

set(INC
  .
  ../blenkernel
)

set(INC_SYS

)

set(SRC
  intern/ikplugin_api.c

  BIK_api.h
  intern/ikplugin_api.h
)

set(LIB
  PRIVATE bf::blenlib
  PRIVATE bf::dna
  PRIVATE bf::intern::guardedalloc
)

if(WITH_IK_SOLVER)
  list(APPEND LIB
    bf_intern_iksolver
  )
  list(APPEND INC
    ../../../intern/iksolver/extern
  )
  list(APPEND SRC
    intern/iksolver_plugin.c
    intern/iksolver_plugin.h
  )
  add_definitions(-DWITH_IK_SOLVER)
endif()

if(WITH_IK_ITASC)
  list(APPEND LIB
    bf_intern_itasc
  )
  list(APPEND INC
    ../../../intern/itasc
  )
  list(APPEND INC_SYS
    ${EIGEN3_INCLUDE_DIRS}
  )
  list(APPEND SRC
    intern/itasc_plugin.cpp
    intern/itasc_plugin.h
  )
  add_definitions(-DWITH_IK_ITASC)
endif()

blender_add_lib(bf_ikplugin "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
