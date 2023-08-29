# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  ../../blenkernel
  ../../gpu
  ../../makesrna
  ../../../../intern/clog
)

set(INC_SYS
  ${Epoxy_INCLUDE_DIRS}
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
  python_utildefines.h
)

set(LIB
  ${Epoxy_LIBRARIES}
  ${PYTHON_LINKFLAGS}
  ${PYTHON_LIBRARIES}
  PRIVATE bf::blenlib
  PRIVATE bf::dna
  PRIVATE bf::intern::guardedalloc
)

if(WITH_PYTHON_MODULE)
  add_definitions(-DWITH_PYTHON_MODULE)
endif()

blender_add_lib(bf_python_ext "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
