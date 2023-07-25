# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
)

set(INC_SYS
  ${PYTHON_INCLUDE_DIRS}
)

set(SRC
  bmesh_py_api.cc
  bmesh_py_geometry.cc
  bmesh_py_ops.cc
  bmesh_py_ops_call.cc
  bmesh_py_types.cc
  bmesh_py_types_customdata.cc
  bmesh_py_types_meshdata.cc
  bmesh_py_types_select.cc
  bmesh_py_utils.cc

  bmesh_py_api.h
  bmesh_py_geometry.h
  bmesh_py_ops.h
  bmesh_py_ops_call.h
  bmesh_py_types.h
  bmesh_py_types_customdata.h
  bmesh_py_types_meshdata.h
  bmesh_py_types_select.h
  bmesh_py_utils.h
)

set(LIB
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::bmesh
  PRIVATE bf::depsgraph
  PRIVATE bf::dna
  PRIVATE bf::intern::guardedalloc
  bf_python_mathutils

  ${PYTHON_LINKFLAGS}
  ${PYTHON_LIBRARIES}
)

if(WITH_FREESTYLE)
  add_definitions(-DWITH_FREESTYLE)
endif()

if(WITH_GMP)
  add_definitions(-DWITH_GMP)
endif()

blender_add_lib(bf_python_bmesh "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
