# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../io
  ../../compositor
  ../../compositor/algorithms
  ../../compositor/cached_resources
  ../../compositor/derived_resources
  ../../compositor/utilities
  ../../makesrna
  ../../nodes/geometry/include
  ../../sequencer
  ../asset

  # RNA_prototypes.hh
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
  node_geometry_layer_search.cc
  node_geometry_volume_grid_search.cc
  node_gizmo.cc
  node_group.cc
  node_ops.cc
  node_relationships.cc
  node_select.cc
  node_shader_preview.cc
  node_socket_tooltip.cc
  node_sync_sockets.cc
  node_templates.cc
  node_tree_interface_ui.cc
  node_view.cc
  space_node.cc

  node_intern.hh
)

set(LIB
  PRIVATE bf::animrig
  PRIVATE bf::asset_system
  PRIVATE bf::blenfont
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::blenloader
  PRIVATE bf::blentranslation
  PRIVATE bf::depsgraph
  PRIVATE bf::dna
  PRIVATE bf::draw
  bf_editor_screen
  PRIVATE bf::functions
  PRIVATE bf::geometry
  PRIVATE bf::gpu
  PRIVATE bf::imbuf
  PRIVATE bf::intern::guardedalloc
  bf_compositor
  PRIVATE bf::extern::fmtlib
  PRIVATE bf::nodes
  PRIVATE bf::render
  PRIVATE bf::windowmanager
  PRIVATE bf::sequencer
)

if(WITH_OPENIMAGEDENOISE)
  add_definitions(-DWITH_OPENIMAGEDENOISE)
endif()

if(WITH_OPENSUBDIV)
  add_definitions(-DWITH_OPENSUBDIV)
endif()

if(WITH_TBB)
  if(WIN32)
    # TBB includes `Windows.h` which will define min/max macros
    # that will collide with the STL versions.
    add_definitions(-DNOMINMAX)
  endif()
endif()

blender_add_lib(bf_editor_space_node "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

# RNA_prototypes.hh dna_type_offsets.h
add_dependencies(bf_editor_space_node bf_rna)
