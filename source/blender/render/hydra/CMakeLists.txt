# SPDX-FileCopyrightText: 2011-2022 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0

# This suppresses the warning "This file includes at least one deprecated or antiquated
# header which may be removed without further notice at a future date", which is caused
# by the USD library including <ext/hash_set> on Linux. This has been reported at:
# https://github.com/PixarAnimationStudios/USD/issues/1057.
if(UNIX AND NOT APPLE)
  add_definitions(-D_GLIBCXX_PERMIT_BACKWARD_HASH)
endif()
if(WIN32)
  add_definitions(-DNOMINMAX -DWIN32_LEAN_AND_MEAN -DBOOST_DEBUG_PYTHON)
endif()
add_definitions(-DBOOST_ALL_NO_LIB)

# Precompiled Linux libs are made with GCC, and USD uses some extensions
# which lead to an incompatible ABI for Clang. Using those extensions with
# Clang as well works around the issue.
if(UNIX AND NOT APPLE)
  if(CMAKE_C_COMPILER_ID MATCHES "Clang")
    if(EXISTS ${LIBDIR})
      add_definitions(-DARCH_HAS_GNU_STL_EXTENSIONS)
    endif()
  endif()
endif()

# USD headers use deprecated TBB headers, silence warning.
add_definitions(-DTBB_SUPPRESS_DEPRECATED_MESSAGES=1)

if(WIN32)
  # Some USD library headers trigger the "unreferenced formal parameter"
  # warning alert.
  # Silence them by restore warn C4100 back to w4
  remove_cc_flag("/w34100")
endif()

set(INC
  ../../../../intern/guardedalloc
  ../../blenkernel
  ../../blenlib
  ../../imbuf
  ../../makesdna
  ../../makesrna
  ../../nodes
  ../../io/usd
  ../../gpu
  ../../gpu/intern
  ../../python/intern
  # RNA_prototypes.h
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
  ..
)

set(INC_SYS
  ${PYTHON_INCLUDE_DIRS}
  ${Epoxy_INCLUDE_DIRS}
  ${USD_INCLUDE_DIRS}
  ${BOOST_INCLUDE_DIR}
  ${TBB_INCLUDE_DIR}
  ${GFLAGS_INCLUDE_DIRS}
  ${EIGEN3_INCLUDE_DIRS}
)

set(LIB
  ${Epoxy_LIBRARIES}
  ${PYTHON_LIBRARIES}
  ${BOOST_LIBRARIES}
  ${USD_LIBRARIES}
  ${TBB_LIBRARIES}
  PRIVATE bf::depsgraph
  PRIVATE bf::intern::clog
  bf_io_usd
)

set(SRC
  camera.cc
  engine.cc
  final_engine.cc
  light_tasks_delegate.cc
  preview_engine.cc
  python.cc
  render_task_delegate.cc
  viewport_engine.cc

  camera.hh
  engine.hh
  final_engine.hh
  light_tasks_delegate.hh
  preview_engine.hh
  render_task_delegate.hh
  settings.hh
  viewport_engine.hh
)

blender_add_lib(bf_render_hydra "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

# RNA_prototypes.h
add_dependencies(bf_render_hydra bf_rna)
