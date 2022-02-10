# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright 2012 Blender Foundation. All rights reserved.

set(INC
  .
  ../glew-mx
  ../guardedalloc
  ../../source/blender/blenlib
  ../../source/blender/gpu
  ../../source/blender/makesdna
)

set(INC_SYS

)

set(SRC
  ocio_capi.cc
  fallback_impl.cc

  ocio_capi.h
  ocio_impl.h
)

set(LIB
)

if(WITH_OPENCOLORIO)
  add_definitions(
    -DWITH_OCIO
  )

  add_definitions(${GL_DEFINITIONS})
  add_definitions(${OPENCOLORIO_DEFINITIONS})

  list(APPEND INC_SYS
    ${OPENCOLORIO_INCLUDE_DIRS}
    ${GLEW_INCLUDE_PATH}
  )

  list(APPEND SRC
    ocio_impl.cc
    ocio_impl_glsl.cc
  )

  list(APPEND LIB
    ${OPENCOLORIO_LIBRARIES}
  )

  if(WIN32)
    list(APPEND INC_SYS
      ${BOOST_INCLUDE_DIR}
    )
    list(APPEND LIB
      ${BOOST_LIBRARIES}
    )
  endif()

  data_to_c_simple(gpu_shader_display_transform.glsl SRC)
  data_to_c_simple(gpu_shader_display_transform_vertex.glsl SRC)
endif()


blender_add_lib(bf_intern_opencolorio "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
