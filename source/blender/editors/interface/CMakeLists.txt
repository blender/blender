# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  ../asset
  ../include
  ../../makesrna
  ../../python
  ../../../../intern/ghost
  # RNA_prototypes.hh
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS
)

set(SRC
  eyedroppers/eyedropper_bone.cc
  eyedroppers/eyedropper_color.cc
  eyedroppers/eyedropper_colorband.cc
  eyedroppers/eyedropper_datablock.cc
  eyedroppers/eyedropper_depth.cc
  eyedroppers/eyedropper_driver.cc
  eyedroppers/eyedropper_grease_pencil_color.cc
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
  templates/interface_template_asset_shelf_popover.cc
  templates/interface_template_attribute_search.cc
  templates/interface_template_bone_collection_tree.cc
  templates/interface_template_cache_file.cc
  templates/interface_template_color_management.cc
  templates/interface_template_color_picker.cc
  templates/interface_template_color_ramp.cc
  templates/interface_template_component_menu.cc
  templates/interface_template_constraint.cc
  templates/interface_template_curve_mapping.cc
  templates/interface_template_curve_profile.cc
  templates/interface_template_event.cc
  templates/interface_template_grease_pencil_layer_search.cc
  templates/interface_template_grease_pencil_layer_tree.cc
  templates/interface_template_icon.cc
  templates/interface_template_id.cc
  templates/interface_template_keymap.cc
  templates/interface_template_layers.cc
  templates/interface_template_light_linking.cc
  templates/interface_template_list.cc
  templates/interface_template_matrix.cc
  templates/interface_template_modifiers.cc
  templates/interface_template_node_inputs.cc
  templates/interface_template_node_tree_interface.cc
  templates/interface_template_operator_property.cc
  templates/interface_template_preview.cc
  templates/interface_template_recent_files.cc
  templates/interface_template_running_jobs.cc
  templates/interface_template_scopes.cc
  templates/interface_template_search.cc
  templates/interface_template_search_menu.cc
  templates/interface_template_search_operator.cc
  templates/interface_template_shader_fx.cc
  templates/interface_template_status.cc
  templates/interface_template_strip_modifiers.cc
  templates/interface_templates.cc
  interface_undo.cc
  interface_utils.cc
  interface_widgets.cc
  resources.cc
  view2d/view2d.cc
  view2d/view2d_draw.cc
  view2d/view2d_edge_pan.cc
  view2d/view2d_gizmo_navigate.cc
  view2d/view2d_ops.cc
  views/abstract_view.cc
  views/abstract_view_item.cc
  views/grid_view.cc
  views/interface_view.cc
  views/tree_view.cc

  eyedroppers/eyedropper_intern.hh
  templates/interface_templates_intern.hh
  interface_intern.hh
  regions/interface_regions_intern.hh
  view2d/view2d_intern.hh
)

set(LIB
  PRIVATE bf::asset_system
  PRIVATE bf::blenfont
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::blenloader
  PRIVATE bf::blentranslation
  PRIVATE bf::bmesh
  PRIVATE bf::depsgraph
  PRIVATE bf::dna
  PRIVATE bf::draw
  bf_editor_datafiles
  bf_editor_id_management
  PRIVATE bf::extern::fmtlib
  PRIVATE bf::functions
  PRIVATE bf::gpu
  PRIVATE bf::imbuf
  PRIVATE bf::imbuf::movie
  PRIVATE bf::intern::clog
  PRIVATE bf::intern::guardedalloc
  PRIVATE bf::animrig
  PRIVATE bf::nodes
  PRIVATE bf::render
  PRIVATE bf::sequencer
  PRIVATE bf::windowmanager
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

blender_add_lib(bf_editor_interface "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

# RNA_prototypes.hh
add_dependencies(bf_editor_interface bf_rna)

if(WITH_GTESTS)
  set(TEST_INC
  )
  set(TEST_SRC
    interface_ops_test.cc
  )

  set(TEST_LIB
    PRIVATE bf::intern::clog
  )

  blender_add_test_suite_lib(interface "${TEST_SRC}" "${INC};${TEST_INC}" "${INC_SYS}" "${LIB};${TEST_LIB}")
endif()
