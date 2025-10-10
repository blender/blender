# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  ../../makesrna
)

set(INC_SYS
  ${PYTHON_INCLUDE_DIRS}
)

set(SRC
  bl_math_py_api.cc
  blf_py_api.cc
  bpy_threads.cc
  idprop_py_api.cc
  idprop_py_ui_api.cc
  imbuf_py_api.cc
  py_capi_rna.cc
  py_capi_utils.cc
  python_compat.cc

  bl_math_py_api.hh
  blf_py_api.hh
  idprop_py_api.hh
  idprop_py_ui_api.hh
  imbuf_py_api.hh
  py_capi_rna.hh
  py_capi_utils.hh
  python_compat.hh

  # header-only
  python_utildefines.hh
)

set(LIB
  ${PYTHON_LINKFLAGS}
  ${PYTHON_LIBRARIES}
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::dna
  PRIVATE bf::gpu
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
