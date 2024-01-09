# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  ../../blenkernel
  ../../gpu
  ../../makesrna
)

set(INC_SYS
  ${PYTHON_INCLUDE_DIRS}
)

set(SRC
  bgl.cc
  bl_math_py_api.cc
  blf_py_api.cc
  bpy_threads.cc
  idprop_py_api.cc
  idprop_py_ui_api.cc
  imbuf_py_api.cc
  py_capi_rna.cc
  py_capi_utils.cc

  bgl.h
  bl_math_py_api.h
  blf_py_api.h
  idprop_py_api.h
  idprop_py_ui_api.h
  imbuf_py_api.h
  py_capi_rna.h
  py_capi_utils.h

  # header-only
  python_compat.h
  python_utildefines.h
)

set(LIB
  ${PYTHON_LINKFLAGS}
  ${PYTHON_LIBRARIES}
  PRIVATE bf::blenlib
  PRIVATE bf::dna
  PRIVATE bf::intern::clog
  PRIVATE bf::intern::guardedalloc
)

# NOTE: use irrespective of `WITH_OPENGL_BACKEND` as the `bgl` module uses this for constants.
list(APPEND INC_SYS
  ${Epoxy_INCLUDE_DIRS}
)

if(WITH_OPENGL_BACKEND)
  list(APPEND LIB
    ${Epoxy_LIBRARIES}
  )
endif()

if(WITH_PYTHON_MODULE)
  add_definitions(-DWITH_PYTHON_MODULE)
endif()

blender_add_lib(bf_python_ext "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
