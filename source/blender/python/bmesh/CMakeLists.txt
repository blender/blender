# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  ../../blenkernel
  ../../blenlib
  ../../bmesh
  ../../depsgraph
  ../../../../intern/guardedalloc
)

set(INC_SYS
  ${PYTHON_INCLUDE_DIRS}
)

set(SRC
  bmesh_py_api.c
  bmesh_py_geometry.c
  bmesh_py_ops.c
  bmesh_py_ops_call.c
  bmesh_py_types.c
  bmesh_py_types_customdata.c
  bmesh_py_types_meshdata.c
  bmesh_py_types_select.c
  bmesh_py_utils.c

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
  bf_blenkernel
  bf_blenlib
  PRIVATE bf::dna
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
