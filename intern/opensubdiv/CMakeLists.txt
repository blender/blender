# SPDX-FileCopyrightText: 2013 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  ../../source/blender/gpu/intern
)

set(INC_SYS
)

set(SRC
  opensubdiv_capi.hh
  opensubdiv_capi_type.hh
  opensubdiv_converter_capi.hh
  opensubdiv_evaluator_capi.hh
  opensubdiv_evaluator.hh
  opensubdiv_topology_refiner.hh
)

set(LIB
  PRIVATE bf::blenlib
  PRIVATE bf::intern::guardedalloc
  PRIVATE bf::gpu
)

if(WITH_OPENSUBDIV)
  macro(OPENSUBDIV_DEFINE_COMPONENT component)
    if(${${component}})
      add_definitions(-D${component})
    endif()
  endmacro()

  list(APPEND INC_SYS
    ${OPENSUBDIV_INCLUDE_DIRS}
    ${Epoxy_INCLUDE_DIRS}
  )

  list(APPEND SRC
    # Base.
    internal/base/memory.h
    internal/base/opensubdiv_capi.cc
    internal/base/type_convert.cc
    internal/base/type_convert.h
    internal/base/util.cc
    internal/base/util.h

    # Evaluator.
    internal/evaluator/eval_output.cc
    internal/evaluator/eval_output.h
    internal/evaluator/eval_output_cpu.cc
    internal/evaluator/eval_output_cpu.h
    internal/evaluator/eval_output_gpu.cc
    internal/evaluator/eval_output_gpu.h
    internal/evaluator/evaluator_cache_impl.cc
    internal/evaluator/evaluator_cache_impl.h
    internal/evaluator/evaluator_capi.cc
    internal/evaluator/evaluator_impl.cc
    internal/evaluator/gpu_compute_evaluator.cc
    internal/evaluator/gpu_compute_evaluator.h
    internal/evaluator/gpu_patch_table.cc
    internal/evaluator/gpu_patch_table.hh
    internal/evaluator/gpu_vertex_buffer_wrapper.hh
    internal/evaluator/patch_map.cc
    internal/evaluator/patch_map.h


    # Topology.
    internal/topology/mesh_topology.cc
    internal/topology/mesh_topology_compare.cc
    internal/topology/mesh_topology.h
    internal/topology/topology_refiner_factory.cc
    internal/topology/topology_refiner_impl.cc
    internal/topology/topology_refiner_impl_compare.cc
  )

  list(APPEND LIB
    ${OPENSUBDIV_LIBRARIES}
    ${Epoxy_LIBRARIES}
  )

  if(WIN32)
    add_definitions(-DNOMINMAX)
    add_definitions(-D_USE_MATH_DEFINES)
  endif()

  set(GLSL_SRC
    internal/evaluator/shaders/osd_eval_stencils_comp.glsl
    internal/evaluator/shaders/osd_eval_patches_comp.glsl
  )

  set(GLSL_C)
  foreach(GLSL_FILE ${GLSL_SRC})
    glsl_to_c(${GLSL_FILE} GLSL_C)
  endforeach()

  blender_add_lib(bf_osd_shaders "${GLSL_C}" "" "" "")

  list(APPEND LIB
    bf_osd_shaders
  )

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

  set(glsl_source_list_file "${CMAKE_CURRENT_BINARY_DIR}/glsl_osd_source_list.h")
  file(GENERATE OUTPUT ${glsl_source_list_file} CONTENT "${GLSL_SOURCE_CONTENT}")
  list(APPEND SRC ${glsl_source_list_file})
  set(glsl_metadata_list_file "${CMAKE_CURRENT_BINARY_DIR}/glsl_osd_metadata_list.hh")
  file(GENERATE OUTPUT ${glsl_metadata_list_file} CONTENT "${GLSL_METADATA_CONTENT}")
  list(APPEND SRC ${glsl_metadata_list_file})
  set(glsl_infos_list_file "${CMAKE_CURRENT_BINARY_DIR}/glsl_osd_infos_list.hh")
  file(GENERATE OUTPUT ${glsl_infos_list_file} CONTENT "${GLSL_INFOS_CONTENT}")
  list(APPEND SRC ${glsl_infos_list_file})
  list(APPEND INC ${CMAKE_CURRENT_BINARY_DIR})

  target_include_directories(bf_osd_shaders PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

else()
  list(APPEND SRC
    stub/opensubdiv_stub.cc
    stub/opensubdiv_evaluator_stub.cc
  )
endif()

blender_add_lib(bf_intern_opensubdiv "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

# Tests.

if(WITH_GTESTS AND WITH_OPENSUBDIV)
  add_definitions(${GFLAGS_DEFINES})
  add_definitions(${GLOG_DEFINES})

  set(TEST_SRC
    internal/topology/mesh_topology_test.cc
  )

  blender_add_test_suite_lib(intern_opensubdiv "${TEST_SRC}" "${INC}" "${INC_SYS}" "${LIB};bf_intern_opensubdiv")
endif()
