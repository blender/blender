# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  ../../gpu/intern
)

set(INC_SYS
)

set(SRC
  intern/config.cc
  intern/cpu_processor_cache.hh
  intern/description.cc
  intern/description.hh
  intern/gpu_shader_binder.cc
  intern/gpu_shader_binder_internal.hh
  intern/ocio_shader_shared.hh
  intern/opencolorio.hh
  intern/source_processor.cc
  intern/source_processor.hh
  intern/version.cc
  intern/view_specific_look.cc
  intern/view_specific_look.hh
  intern/white_point.cc
  intern/white_point.hh

  intern/fallback/fallback_colorspace.hh
  intern/fallback/fallback_config.cc
  intern/fallback/fallback_config.hh
  intern/fallback/fallback_cpu_processor.hh
  intern/fallback/fallback_default_display.hh
  intern/fallback/fallback_default_look.hh
  intern/fallback/fallback_default_view.hh
  intern/fallback/fallback_display_cpu_processor.cc
  intern/fallback/fallback_display_cpu_processor.hh
  intern/fallback/fallback_gpu_shader_binder.cc
  intern/fallback/fallback_gpu_shader_binder.hh
  intern/fallback/fallback_processor_cache.cc
  intern/fallback/fallback_processor_cache.hh

  intern/libocio/error_handling.cc
  intern/libocio/error_handling.hh
  intern/libocio/libocio_colorspace.cc
  intern/libocio/libocio_colorspace.hh
  intern/libocio/libocio_config.cc
  intern/libocio/libocio_config.hh
  intern/libocio/libocio_cpu_processor.cc
  intern/libocio/libocio_cpu_processor.hh
  intern/libocio/libocio_display.cc
  intern/libocio/libocio_display.hh
  intern/libocio/libocio_display_processor.cc
  intern/libocio/libocio_display_processor.hh
  intern/libocio/libocio_gpu_shader_binder.cc
  intern/libocio/libocio_gpu_shader_binder.hh
  intern/libocio/libocio_look.cc
  intern/libocio/libocio_look.hh
  intern/libocio/libocio_processor.cc
  intern/libocio/libocio_processor.hh
  intern/libocio/libocio_view.hh

  OCIO_api.hh
  OCIO_colorspace.hh
  OCIO_config.hh
  OCIO_cpu_processor.hh
  OCIO_display.hh
  OCIO_gpu_shader_binder.hh
  OCIO_look.hh
  OCIO_matrix.hh
  OCIO_packed_image.hh
  OCIO_role_names.hh
  OCIO_version.hh
  OCIO_view.hh
)

set(LIB
  PRIVATE bf::blenlib
  PRIVATE bf::blenkernel
  PRIVATE bf::dna
  PRIVATE bf::gpu
  PRIVATE bf::intern::guardedalloc
  PRIVATE bf::intern::clog
  PUBLIC bf::dependencies::optional::opencolorio
  PRIVATE bf::extern::fmtlib
)

blender_add_lib(bf_imbuf_opencolorio "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
add_library(bf::imbuf::opencolorio ALIAS bf_imbuf_opencolorio)

target_include_directories(bf_imbuf_opencolorio PUBLIC .)

set(GLSL_SRC
  shaders/gpu_shader_display_transform_vert.glsl
  shaders/gpu_shader_display_transform_frag.glsl

  intern/ocio_shader_shared.hh
)

set(GLSL_C)
foreach(GLSL_FILE ${GLSL_SRC})
  glsl_to_c(${GLSL_FILE} GLSL_C)
endforeach()

blender_add_lib(bf_imbuf_opencolorio_shaders "${GLSL_C}" "" "" "")

target_link_libraries(bf_imbuf_opencolorio PRIVATE bf_imbuf_opencolorio_shaders)

set(GLSL_SOURCE_CONTENT "")
set(GLSL_METADATA_CONTENT "")
set(GLSL_INFOS_CONTENT "")
foreach(GLSL_FILE ${GLSL_SRC})
  get_filename_component(GLSL_FILE_NAME ${GLSL_FILE} NAME)
  string(REPLACE "." "_" GLSL_FILE_NAME_UNDERSCORES ${GLSL_FILE_NAME})
  string(APPEND GLSL_SOURCE_CONTENT "SHADER_SOURCE\(${GLSL_FILE_NAME_UNDERSCORES}, \"${GLSL_FILE_NAME}\", \"${GLSL_FILE}\"\)\n")
  string(APPEND GLSL_METADATA_CONTENT "#include \"${GLSL_FILE}.hh\"\n")
  string(APPEND GLSL_INFOS_CONTENT "#include \"${GLSL_FILE}.info\"\n")
endforeach()

set(glsl_source_list_file "${CMAKE_CURRENT_BINARY_DIR}/glsl_ocio_source_list.h")
file(GENERATE OUTPUT ${glsl_source_list_file} CONTENT "${GLSL_SOURCE_CONTENT}")
list(APPEND SRC ${glsl_source_list_file})
set(glsl_metadata_list_file "${CMAKE_CURRENT_BINARY_DIR}/glsl_ocio_metadata_list.hh")
file(GENERATE OUTPUT ${glsl_metadata_list_file} CONTENT "${GLSL_METADATA_CONTENT}")
list(APPEND SRC ${glsl_metadata_list_file})
set(glsl_infos_list_file "${CMAKE_CURRENT_BINARY_DIR}/glsl_ocio_infos_list.hh")
file(GENERATE OUTPUT ${glsl_infos_list_file} CONTENT "${GLSL_INFOS_CONTENT}")
list(APPEND SRC ${glsl_infos_list_file})
list(APPEND INC ${CMAKE_CURRENT_BINARY_DIR})

target_include_directories(bf_imbuf_opencolorio_shaders PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

if(WITH_GTESTS)
  set(TEST_SRC
    intern/description_test.cc
    intern/source_processor_test.cc
    intern/view_specific_look_test.cc
  )
  blender_add_test_suite_lib(imbuf_opencolorio "${TEST_SRC}" "${INC}" "${INC_SYS}" "${LIB};bf::imbuf::opencolorio")
endif()
