# SPDX-FileCopyrightText: 2014 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

add_definitions(${GFLAGS_DEFINES})
add_definitions(${GLOG_DEFINES})
add_definitions(-DBLENDER_GFLAGS_NAMESPACE=${GFLAGS_NAMESPACE})

set(INC
  .
  ..
  ${GLOG_INCLUDE_DIRS}
  ${GFLAGS_INCLUDE_DIRS}
  ../../../extern/gtest/include
)

set(INC_SYS
)

set(SRC
  testing_main.cc

  testing.h
)

set(LIB
  PRIVATE bf::intern::guardedalloc
)

blender_add_lib(bf_testing_main "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
