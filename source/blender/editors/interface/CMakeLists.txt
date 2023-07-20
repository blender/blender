# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  ../include
  ../../asset_system
  ../../blenfont
  ../../blenloader
  ../../blentranslation
  ../../depsgraph
  ../../draw
  ../../functions
  ../../gpu
  ../../imbuf
  ../../makesrna
  ../../nodes
  ../../python
  ../../render
  ../../../../intern/ghost
  ../../../../extern/fmtlib/include
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
  interface_region_color_picker.cc
  interface_region_hud.cc
  interface_region_menu_pie.cc
  interface_region_menu_popup.cc
  interface_region_popover.cc
  interface_region_popup.cc
  interface_region_search.cc
  interface_region_tooltip.cc
  interface_regions.cc
  interface_style.cc
  interface_template_asset_view.cc
  interface_template_attribute_search.cc
  interface_template_grease_pencil_layer_tree.cc
  interface_template_light_linking.cc
  interface_template_list.cc
  interface_template_search_menu.cc
  interface_template_search_operator.cc
  interface_templates.cc
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
  interface_regions_intern.hh
  view2d_intern.hh
)

set(LIB
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::dna
  bf_editor_datafiles
  PRIVATE bf::intern::guardedalloc
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

if(WIN32 OR APPLE)
  if(WITH_INPUT_IME)
    add_definitions(-DWITH_INPUT_IME)
  endif()
endif()


blender_add_lib(bf_editor_interface "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

# RNA_prototypes.h
add_dependencies(bf_editor_interface bf_rna)
