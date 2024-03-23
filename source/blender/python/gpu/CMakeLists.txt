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
  ${PYTHON_INCLUDE_DIRS}
)

set(SRC
  gpu_py.cc
  gpu_py_api.cc
  gpu_py_batch.cc
  gpu_py_buffer.cc
  gpu_py_capabilities.cc
  gpu_py_compute.cc
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

  gpu_py.hh
  gpu_py_api.hh
  gpu_py_batch.hh
  gpu_py_buffer.hh
  gpu_py_capabilities.hh
  gpu_py_compute.hh
  gpu_py_element.hh
  gpu_py_framebuffer.hh
  gpu_py_matrix.hh
  gpu_py_offscreen.hh
  gpu_py_platform.hh
  gpu_py_select.hh
  gpu_py_shader.hh
  gpu_py_state.hh
  gpu_py_texture.hh
  gpu_py_types.hh
  gpu_py_uniformbuffer.hh
  gpu_py_vertex_buffer.hh
  gpu_py_vertex_format.hh
)

set(LIB
  ${PYTHON_LINKFLAGS}
  ${PYTHON_LIBRARIES}
  PRIVATE bf::blenlib
  PRIVATE bf::dna
  PRIVATE bf::intern::guardedalloc
)

if(WITH_OPENGL_BACKEND)
  list(APPEND INC_SYS
    ${Epoxy_INCLUDE_DIRS}
  )
  list(APPEND LIB
    ${Epoxy_LIBRARIES}
  )
endif()

blender_add_lib(bf_python_gpu "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
