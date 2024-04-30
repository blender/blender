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
  .
  composite
  function
  geometry
  intern
  shader
  texture
  ../editors/include
  ../blenkernel
  ../blenloader
  ../blentranslation
  ../bmesh
  ../functions
  ../geometry
  ../gpu
  ../imbuf
  ../makesrna
  ../modifiers
  ../render
  ../windowmanager

  # RNA_prototypes.h
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS
)

set(SRC
  intern/derived_node_tree.cc
  intern/geometry_nodes_execute.cc
  intern/geometry_nodes_lazy_function.cc
  intern/geometry_nodes_log.cc
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
  intern/socket_search_link.cc

  NOD_common.h
  NOD_composite.hh
  NOD_derived_node_tree.hh
  NOD_geometry.hh
  NOD_geometry_exec.hh
  NOD_geometry_nodes_execute.hh
  NOD_geometry_nodes_lazy_function.hh
  NOD_geometry_nodes_log.hh
  NOD_math_functions.hh
  NOD_multi_function.hh
  NOD_node_declaration.hh
  NOD_node_extra_info.hh
  NOD_register.hh
  NOD_rna_define.hh
  NOD_shader.h
  NOD_socket.hh
  NOD_socket_declarations.hh
  NOD_socket_declarations_geometry.hh
  NOD_socket_items.hh
  NOD_socket_items_ops.hh
  NOD_socket_search_link.hh
  NOD_static_types.h
  NOD_texture.h
  intern/node_common.h
  intern/node_exec.hh
  intern/node_util.hh
)

set(LIB
  PRIVATE bf::blenlib
  bf_bmesh
  PRIVATE bf::depsgraph
  PRIVATE bf::dna
  bf_functions
  PRIVATE bf::intern::guardedalloc
  bf_nodes_composite
  bf_nodes_function
  bf_nodes_geometry
  bf_nodes_shader
  bf_nodes_texture
  PRIVATE bf::extern::fmtlib
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
  list(APPEND INC_SYS
    ${TBB_INCLUDE_DIRS}
  )
  add_definitions(-DWITH_TBB)
  if(WIN32)
    # TBB includes Windows.h which will define min/max macros
    # that will collide with the stl versions.
    add_definitions(-DNOMINMAX)
  endif()
endif()

if(WITH_IMAGE_OPENEXR)
  add_definitions(-DWITH_OPENEXR)
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

if(WITH_OPENVDB)
  list(APPEND INC
    ../../../intern/openvdb
  )
  list(APPEND INC_SYS
    ${OPENVDB_INCLUDE_DIRS}
  )
  add_definitions(-DWITH_OPENVDB ${OPENVDB_DEFINITIONS})
endif()

blender_add_lib(bf_nodes "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

# RNA_prototypes.h
add_dependencies(bf_nodes bf_rna)
