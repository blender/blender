# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../io
  ../../asset_system
  ../../blenkernel
  ../../blenloader
  ../../blentranslation
  ../../compositor
  ../../compositor/realtime_compositor
  ../../draw
  ../../functions
  ../../geometry
  ../../gpu
  ../../imbuf
  ../../makesrna
  ../../nodes
  ../../render
  ../../windowmanager

  # RNA_prototypes.h
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS
)

set(SRC
  add_menu_assets.cc
  clipboard.cc
  drawnode.cc
  link_drag_search.cc
  node_add.cc
  node_context_path.cc
  node_draw.cc
  node_edit.cc
  node_geometry_attribute_search.cc
  node_gizmo.cc
  node_group.cc
  node_ops.cc
  node_relationships.cc
  node_select.cc
  node_shader_preview.cc
  node_templates.cc
  node_view.cc
  space_node.cc

  node_intern.hh
)

set(LIB
  PRIVATE bf::blenfont
  bf_blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::depsgraph
  PRIVATE bf::dna
  bf_editor_screen
  PRIVATE bf::intern::guardedalloc
  bf_realtime_compositor
  PRIVATE bf::extern::fmtlib
)

if(WITH_COMPOSITOR_CPU)
  add_definitions(-DWITH_COMPOSITOR_CPU)
endif()

if(WITH_OPENIMAGEDENOISE)
  add_definitions(-DWITH_OPENIMAGEDENOISE)
endif()

if(WITH_OPENSUBDIV)
  add_definitions(-DWITH_OPENSUBDIV)
endif()

if(WITH_OPENVDB)
  list(APPEND INC
    ../../../../intern/openvdb
  )
  list(APPEND INC_SYS
    ${OPENVDB_INCLUDE_DIRS}
  )
  list(APPEND LIB
    bf_intern_openvdb
    ${OPENVDB_LIBRARIES}
  )
  add_definitions(-DWITH_OPENVDB ${OPENVDB_DEFINITIONS})
endif()

if(WITH_TBB)
  add_definitions(-DWITH_TBB)
  if(WIN32)
    # TBB includes Windows.h which will define min/max macros
    # that will collide with the stl versions.
    add_definitions(-DNOMINMAX)
  endif()
  list(APPEND INC_SYS
    ${TBB_INCLUDE_DIRS}
  )

  list(APPEND LIB
    ${TBB_LIBRARIES}
  )
endif()

blender_add_lib(bf_editor_space_node "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

# RNA_prototypes.h dna_type_offsets.h
add_dependencies(bf_editor_space_node bf_rna)
