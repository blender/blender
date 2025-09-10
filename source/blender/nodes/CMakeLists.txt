# SPDX-FileCopyrightText: 2006 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(DISCOVER_NODES_PATH ${CMAKE_CURRENT_SOURCE_DIR}/intern/discover_nodes.py)

# Creates a library with the given name that exposes one function which calls
# the register functions of all nodes in the provided source files.
function(add_node_discovery
  lib_name
  sources
  output_file
  output_function
  )

  set(extra_args "")

  # See `add_definitions(-DUSE_MAKEFILE_WORKAROUND)` comment for `makesrna`.
  if(CMAKE_GENERATOR MATCHES ".*Unix Makefiles")
    set(extra_args "--use-makefile-workaround")
  endif()

  add_custom_command(
    OUTPUT
      ${output_file}
    COMMAND
      ${PYTHON_EXECUTABLE}
      ${DISCOVER_NODES_PATH}
      ${CMAKE_CURRENT_SOURCE_DIR}
      ${output_file}
      ${output_function}
      ${sources}
      ${extra_args}
    DEPENDS
      ${SRC}
      ${DISCOVER_NODES_PATH}
  )
  set_source_files_properties(${output_file} PROPERTIES GENERATED TRUE)
  add_library(${lib_name} ${output_file})
endfunction()

add_subdirectory(composite)
add_subdirectory(function)
add_subdirectory(geometry)
add_subdirectory(shader)
add_subdirectory(texture)

set(INC
  PUBLIC .
  composite
  function
  geometry
  intern
  shader
  texture
  geometry/include
  ../animrig
  ../editors/include
  ../depsgraph
  ../makesrna
  ../modifiers

  # RNA_prototypes.hh
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS
)

set(SRC
  intern/derived_node_tree.cc
  intern/geometry_nodes_bundle.cc
  intern/geometry_nodes_caller_ui.cc
  intern/geometry_nodes_closure.cc
  intern/geometry_nodes_closure_zone.cc
  intern/geometry_nodes_dependencies.cc
  intern/geometry_nodes_execute.cc
  intern/geometry_nodes_foreach_geometry_element_zone.cc
  intern/geometry_nodes_gizmos.cc
  intern/geometry_nodes_lazy_function.cc
  intern/geometry_nodes_list.cc
  intern/geometry_nodes_log.cc
  intern/geometry_nodes_repeat_zone.cc
  intern/geometry_nodes_warning.cc
  intern/inverse_eval.cc
  intern/list_function_eval.cc
  intern/math_functions.cc
  intern/node_common.cc
  intern/node_declaration.cc
  intern/node_exec.cc
  intern/node_geometry_exec.cc
  intern/node_multi_function.cc
  intern/node_register.cc
  intern/node_rna_define.cc
  intern/node_socket.cc
  intern/node_socket_declarations.cc
  intern/node_util.cc
  intern/partial_eval.cc
  intern/shader_nodes_inline.cc
  intern/socket_search_link.cc
  intern/socket_usage_inference.cc
  intern/socket_value_inference.cc
  intern/sync_sockets.cc
  intern/trace_values.cc
  intern/value_elem.cc
  intern/volume_grid_function_eval.cc

  NOD_common.hh
  NOD_composite.hh
  NOD_derived_node_tree.hh
  NOD_geometry.hh
  NOD_geometry_exec.hh
  NOD_geometry_nodes_bundle.hh
  NOD_geometry_nodes_bundle_fwd.hh
  NOD_geometry_nodes_bundle_signature.hh
  NOD_geometry_nodes_caller_ui.hh
  NOD_geometry_nodes_closure.hh
  NOD_geometry_nodes_closure_eval.hh
  NOD_geometry_nodes_closure_fwd.hh
  NOD_geometry_nodes_closure_location.hh
  NOD_geometry_nodes_closure_signature.hh
  NOD_geometry_nodes_dependencies.hh
  NOD_geometry_nodes_execute.hh
  NOD_geometry_nodes_gizmos.hh
  NOD_geometry_nodes_lazy_function.hh
  NOD_geometry_nodes_list.hh
  NOD_geometry_nodes_list_fwd.hh
  NOD_geometry_nodes_log.hh
  NOD_geometry_nodes_values.hh
  NOD_geometry_nodes_warning.hh
  NOD_inverse_eval_params.hh
  NOD_inverse_eval_path.hh
  NOD_inverse_eval_run.hh
  NOD_math_functions.hh
  NOD_menu_value.hh
  NOD_multi_function.hh
  NOD_nested_node_id.hh
  NOD_node_declaration.hh
  NOD_node_extra_info.hh
  NOD_node_in_compute_context.hh
  NOD_partial_eval.hh
  NOD_register.hh
  NOD_rna_define.hh
  NOD_shader.h
  NOD_shader_nodes_inline.hh
  NOD_socket.hh
  NOD_socket_declarations.hh
  NOD_socket_declarations_geometry.hh
  NOD_socket_items.hh
  NOD_socket_items_blend.hh
  NOD_socket_items_ops.hh
  NOD_socket_items_ui.hh
  NOD_socket_search_link.hh
  NOD_socket_usage_inference.hh
  NOD_socket_usage_inference_fwd.hh
  NOD_socket_value_inference.hh
  NOD_sync_sockets.hh
  NOD_texture.h
  NOD_trace_values.hh
  NOD_value_elem.hh
  NOD_value_elem_eval.hh
  intern/list_function_eval.hh
  intern/node_common.h
  intern/node_exec.hh
  intern/node_util.hh
  intern/volume_grid_function_eval.hh
)

set(LIB
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::blenloader
  PRIVATE bf::blentranslation
  PRIVATE bf::bmesh
  PRIVATE bf::depsgraph
  PRIVATE bf::dna
  PRIVATE bf::functions
  PRIVATE bf::geometry
  PRIVATE bf::gpu
  PRIVATE bf::imbuf
  PRIVATE bf::intern::guardedalloc
  bf_nodes_composite
  bf_nodes_function
  bf_nodes_geometry
  bf_nodes_shader
  bf_nodes_texture
  PRIVATE bf::extern::fmtlib
  PRIVATE bf::extern::xxhash
  PRIVATE bf::render
  PRIVATE bf::windowmanager
  PRIVATE bf::intern::clog
)

if(WITH_BULLET)
  list(APPEND INC_SYS
    ${BULLET_INCLUDE_DIRS}
    ../../../intern/rigidbody
  )
  list(APPEND LIB
    ${BULLET_LIBRARIES}
  )
  add_definitions(-DWITH_BULLET)
endif()

if(WITH_TBB)
  if(WIN32)
    # TBB includes Windows.h which will define min/max macros
    # that will collide with the STL versions.
    add_definitions(-DNOMINMAX)
  endif()
endif()

if(WITH_IMAGE_OPENEXR)
  add_definitions(-DWITH_IMAGE_OPENEXR)
endif()

if(WITH_FREESTYLE)
  add_definitions(-DWITH_FREESTYLE)
endif()

if(WITH_OPENSUBDIV)
  add_definitions(-DWITH_OPENSUBDIV)
endif()

if(WITH_GMP)
  add_definitions(-DWITH_GMP)

  list(APPEND INC_SYS
    ${GMP_INCLUDE_DIRS}
  )

  list(APPEND LIB
    ${GMP_LIBRARIES}
  )
endif()

blender_add_lib(bf_nodes "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
add_library(bf::nodes ALIAS bf_nodes)

# RNA_prototypes.hh
add_dependencies(bf_nodes bf_rna)

if(WITH_GTESTS)
  set(TEST_INC
  )
  set(TEST_SRC
    intern/geometry_nodes_bundle_tests.cc
    intern/node_iterator_tests.cc
  )
  set(TEST_LIB
    bf_nodes
  )
  blender_add_test_suite_lib(nodes "${TEST_SRC}" "${INC};${TEST_INC}" "${INC_SYS}" "${LIB}")
endif()
