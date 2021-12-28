# ***** BEGIN GPL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# The Original Code is Copyright (C) 2006, Blender Foundation
# All rights reserved.
# ***** END GPL LICENSE BLOCK *****

add_subdirectory(composite)
add_subdirectory(function)
add_subdirectory(geometry)
add_subdirectory(shader)

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
  ../blenlib
  ../blentranslation
  ../bmesh
  ../depsgraph
  ../functions
  ../geometry
  ../gpu
  ../imbuf
  ../makesdna
  ../makesrna
  ../render
  ../windowmanager
  ../../../intern/glew-mx
  ../../../intern/guardedalloc
)


set(SRC
  texture/nodes/node_texture_at.c
  texture/nodes/node_texture_bricks.c
  texture/nodes/node_texture_checker.c
  texture/nodes/node_texture_common.c
  texture/nodes/node_texture_compose.c
  texture/nodes/node_texture_coord.c
  texture/nodes/node_texture_curves.c
  texture/nodes/node_texture_decompose.c
  texture/nodes/node_texture_distance.c
  texture/nodes/node_texture_hueSatVal.c
  texture/nodes/node_texture_image.c
  texture/nodes/node_texture_invert.c
  texture/nodes/node_texture_math.c
  texture/nodes/node_texture_mixRgb.c
  texture/nodes/node_texture_output.c
  texture/nodes/node_texture_proc.c
  texture/nodes/node_texture_rotate.c
  texture/nodes/node_texture_scale.c
  texture/nodes/node_texture_texture.c
  texture/nodes/node_texture_translate.c
  texture/nodes/node_texture_valToNor.c
  texture/nodes/node_texture_valToRgb.c
  texture/nodes/node_texture_viewer.c
  texture/node_texture_tree.c
  texture/node_texture_util.c

  intern/derived_node_tree.cc
  intern/geometry_nodes_eval_log.cc
  intern/math_functions.cc
  intern/node_common.cc
  intern/node_declaration.cc
  intern/node_exec.cc
  intern/node_geometry_exec.cc
  intern/node_multi_function.cc
  intern/node_socket.cc
  intern/node_socket_declarations.cc
  intern/socket_search_link.cc
  intern/node_tree_ref.cc
  intern/node_util.c

  texture/node_texture_util.h

  NOD_common.h
  NOD_composite.h
  NOD_derived_node_tree.hh
  NOD_function.h
  NOD_geometry.h
  NOD_geometry_exec.hh
  NOD_geometry_nodes_eval_log.hh
  NOD_math_functions.hh
  NOD_multi_function.hh
  NOD_node_declaration.hh
  NOD_node_tree_ref.hh
  NOD_shader.h
  NOD_socket.h
  NOD_socket_declarations.hh
  NOD_socket_search_link.hh
  NOD_socket_declarations_geometry.hh
  NOD_static_types.h
  NOD_texture.h
  intern/node_common.h
  intern/node_exec.h
  intern/node_util.h
)

set(LIB
  bf_bmesh
  bf_functions
  bf_nodes_composite
  bf_nodes_function
  bf_nodes_geometry
  bf_nodes_shader
)

if(WITH_BULLET)
  list(APPEND INC_SYS
    ${BULLET_INCLUDE_DIRS}
    "../../../intern/rigidbody/"
  )
  if(NOT WITH_SYSTEM_BULLET)
  list(APPEND LIB
    extern_bullet
    )
  endif()

  list(APPEND LIB
    ${BULLET_LIBRARIES}
  )
  add_definitions(-DWITH_BULLET)
endif()

if(WITH_PYTHON)
  list(APPEND INC
    ../python
  )
  list(APPEND INC_SYS
    ${PYTHON_INCLUDE_DIRS}
  )
  list(APPEND LIB
    ${PYTHON_LINKFLAGS}
    ${PYTHON_LIBRARIES}
  )
  add_definitions(-DWITH_PYTHON)
endif()

if(WITH_INTERNATIONAL)
  add_definitions(-DWITH_INTERNATIONAL)
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
  list(APPEND INC_SYS
    ${OPENVDB_INCLUDE_DIRS}
  )
  add_definitions(-DWITH_OPENVDB ${OPENVDB_DEFINITIONS})
endif()

blender_add_lib(bf_nodes "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
