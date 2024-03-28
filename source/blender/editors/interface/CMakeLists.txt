# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  ../include
  ../../asset_system
  ../../blenkernel
  ../../blenloader
  ../../blentranslation
  ../../draw
  ../../functions
  ../../gpu
  ../../imbuf
  ../../makesrna
  ../../nodes
  ../../python
  ../../render
  ../../windowmanager
  ../../../../intern/ghost
  ../../bmesh
  # RNA_prototypes.h
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS
)

set(SRC
  eyedroppers/eyedropper_color.cc
  eyedroppers/eyedropper_colorband.cc
  eyedroppers/eyedropper_datablock.cc
  eyedroppers/eyedropper_depth.cc
  eyedroppers/eyedropper_driver.cc
  eyedroppers/eyedropper_gpencil_color.cc
  eyedroppers/interface_eyedropper.cc
  interface.cc
  interface_align.cc
  interface_anim.cc
  interface_button_group.cc
  interface_button_sections.cc
  interface_context_menu.cc
  interface_context_path.cc
  interface_drag.cc
  interface_draw.cc
  interface_drop.cc
  interface_dropboxes.cc
  interface_handlers.cc
  interface_icons.cc
  interface_icons_event.cc
  interface_layout.cc
  interface_ops.cc
  interface_panel.cc
  interface_query.cc
  regions/interface_region_color_picker.cc
  regions/interface_region_hud.cc
  regions/interface_region_menu_pie.cc
  regions/interface_region_menu_popup.cc
  regions/interface_region_popover.cc
  regions/interface_region_popup.cc
  regions/interface_region_search.cc
  regions/interface_region_tooltip.cc
  regions/interface_regions.cc
  interface_string_search.cc
  interface_style.cc
  templates/interface_template_asset_view.cc
  templates/interface_template_attribute_search.cc
  templates/interface_template_bone_collection_tree.cc
  templates/interface_template_light_linking.cc
  templates/interface_template_list.cc
  templates/interface_template_node_inputs.cc
  templates/interface_template_node_tree_interface.cc
  templates/interface_template_search_menu.cc
  templates/interface_template_search_operator.cc
  templates/interface_templates.cc
  interface_undo.cc
  interface_utils.cc
  interface_widgets.cc
  resources.cc
  view2d.cc
  view2d_draw.cc
  view2d_edge_pan.cc
  view2d_gizmo_navigate.cc
  view2d_ops.cc
  views/abstract_view.cc
  views/abstract_view_item.cc
  views/grid_view.cc
  views/interface_view.cc
  views/tree_view.cc

  eyedroppers/eyedropper_intern.hh
  interface_intern.hh
  regions/interface_regions_intern.hh
  view2d_intern.hh
)

set(LIB
  PRIVATE bf::blenfont
  bf_blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::depsgraph
  PRIVATE bf::dna
  bf_editor_datafiles
  PRIVATE bf::extern::fmtlib
  PRIVATE bf::intern::guardedalloc
  PRIVATE bf::animrig
)

if(WITH_INTERNATIONAL)
  add_definitions(-DWITH_INTERNATIONAL)
endif()

if(WITH_HEADLESS)
  add_definitions(-DWITH_HEADLESS)
endif()

if(WITH_PYTHON)
  add_definitions(-DWITH_PYTHON)
endif()

if(WITH_INPUT_IME)
  add_definitions(-DWITH_INPUT_IME)
endif()

if(WITH_EXPERIMENTAL_FEATURES)
  add_definitions(-DWITH_GREASE_PENCIL_V3)
  list(APPEND SRC
    templates/interface_template_grease_pencil_layer_tree.cc
  )
endif()

blender_add_lib(bf_editor_interface "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

# RNA_prototypes.h
add_dependencies(bf_editor_interface bf_rna)
