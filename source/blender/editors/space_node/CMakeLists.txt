# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../../asset_system
  ../../blenfont
  ../../blenkernel
  ../../blenlib
  ../../blenloader
  ../../blentranslation
  ../../compositor
  ../../depsgraph
  ../../draw
  ../../functions
  ../../geometry
  ../../gpu
  ../../imbuf
  ../../makesrna
  ../../nodes
  ../../render
  ../../windowmanager
  ../../../../intern/guardedalloc
  ../../../../extern/fmtlib/include

  # RNA_prototypes.h
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)


set(SRC
  add_menu_assets.cc
  add_node_search.cc
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
  node_templates.cc
  node_view.cc
  space_node.cc

  node_intern.hh
)

set(LIB
  bf_blenkernel
  bf_blenlib
  PRIVATE bf::dna
  bf_editor_screen
  extern_fmtlib
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
