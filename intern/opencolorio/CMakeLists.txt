# SPDX-FileCopyrightText: 2012 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  ../../source/blender/gpu
  ../../source/blender/gpu/intern
)

set(INC_SYS

)

set(SRC
  ocio_capi.cc
  fallback_impl.cc

  ocio_capi.h
  ocio_impl.h
  ocio_shader_shared.hh
)

set(LIB
  PRIVATE bf::blenlib
  PRIVATE bf::dna
  PRIVATE bf::intern::guardedalloc
)

if(WITH_OPENCOLORIO)
  add_definitions(
    -DWITH_OCIO
  )

  add_definitions(${OPENCOLORIO_DEFINITIONS})

  list(APPEND INC_SYS
    ${OPENCOLORIO_INCLUDE_DIRS}
    ${Epoxy_INCLUDE_DIRS}
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

  set(GLSL_SRC
    gpu_shader_display_transform_vert.glsl
    gpu_shader_display_transform_frag.glsl

    ocio_shader_shared.hh
  )

  set(GLSL_C)
  foreach(GLSL_FILE ${GLSL_SRC})
    data_to_c_simple(${GLSL_FILE} GLSL_C)
  endforeach()

  blender_add_lib(bf_ocio_shaders "${GLSL_C}" "" "" "")

  list(APPEND LIB
    bf_ocio_shaders
  )

  set(GLSL_SOURCE_CONTENT "")
  foreach(GLSL_FILE ${GLSL_SRC})
    get_filename_component(GLSL_FILE_NAME ${GLSL_FILE} NAME)
    string(REPLACE "." "_" GLSL_FILE_NAME_UNDERSCORES ${GLSL_FILE_NAME})
    string(APPEND GLSL_SOURCE_CONTENT "SHADER_SOURCE\(datatoc_${GLSL_FILE_NAME_UNDERSCORES}, \"${GLSL_FILE_NAME}\", \"${GLSL_FILE}\"\)\n")
  endforeach()

  set(glsl_source_list_file "${CMAKE_CURRENT_BINARY_DIR}/glsl_ocio_source_list.h")
  file(GENERATE OUTPUT ${glsl_source_list_file} CONTENT "${GLSL_SOURCE_CONTENT}")
  list(APPEND SRC ${glsl_source_list_file})
  list(APPEND INC ${CMAKE_CURRENT_BINARY_DIR})

  target_include_directories(bf_ocio_shaders PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

endif()


blender_add_lib(bf_intern_opencolorio "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
