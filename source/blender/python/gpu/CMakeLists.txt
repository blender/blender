# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  ../../blenkernel
  ../../editors/include
  ../../gpu
  ../../imbuf
)

set(INC_SYS
  ${Epoxy_INCLUDE_DIRS}
  ${PYTHON_INCLUDE_DIRS}
)

set(SRC
  gpu_py.cc
  gpu_py_api.cc
  gpu_py_batch.cc
  gpu_py_buffer.cc
  gpu_py_capabilities.cc
  gpu_py_element.cc
  gpu_py_framebuffer.cc
  gpu_py_matrix.cc
  gpu_py_offscreen.cc
  gpu_py_platform.cc
  gpu_py_select.cc
  gpu_py_shader.cc
  gpu_py_shader_create_info.cc
  gpu_py_state.cc
  gpu_py_texture.cc
  gpu_py_types.cc
  gpu_py_uniformbuffer.cc
  gpu_py_vertex_buffer.cc
  gpu_py_vertex_format.cc

  gpu_py.h
  gpu_py_api.h
  gpu_py_batch.h
  gpu_py_buffer.h
  gpu_py_capabilities.h
  gpu_py_element.h
  gpu_py_framebuffer.h
  gpu_py_matrix.h
  gpu_py_offscreen.h
  gpu_py_platform.h
  gpu_py_select.h
  gpu_py_shader.h
  gpu_py_state.h
  gpu_py_texture.h
  gpu_py_types.h
  gpu_py_uniformbuffer.h
  gpu_py_vertex_buffer.h
  gpu_py_vertex_format.h
)

set(LIB
  ${Epoxy_LIBRARIES}
  ${PYTHON_LINKFLAGS}
  ${PYTHON_LIBRARIES}
  PRIVATE bf::blenlib
  PRIVATE bf::dna
  PRIVATE bf::intern::guardedalloc
)

blender_add_lib(bf_python_gpu "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
