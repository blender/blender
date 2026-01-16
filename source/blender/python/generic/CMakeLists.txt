# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  ../../makesrna
)

set(INC_SYS
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
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::dna
  PRIVATE bf::gpu
  PRIVATE bf::intern::clog
  PRIVATE bf::intern::guardedalloc
  PRIVATE bf::dependencies::optional::python
  PRIVATE bf::dependencies::epoxy
)

if(WITH_PYTHON_MODULE)
  add_definitions(-DWITH_PYTHON_MODULE)
endif()

blender_add_lib(bf_python_ext "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
