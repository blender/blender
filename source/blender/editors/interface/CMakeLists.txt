# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  ../include
  ../../blenfont
  ../../blenkernel
  ../../blenlib
  ../../blenloader
  ../../blentranslation
  ../../depsgraph
  ../../draw
  ../../functions
  ../../gpu
  ../../imbuf
  ../../makesdna
  ../../makesrna
  ../../nodes
  ../../python
  ../../render
  ../../windowmanager
  ../../../../intern/guardedalloc
  ../../bmesh
  # RNA_prototypes.h
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(SRC
  eyedroppers/eyedropper_color.c
  eyedroppers/eyedropper_colorband.c
  eyedroppers/eyedropper_datablock.c
  eyedroppers/eyedropper_depth.c
  eyedroppers/eyedropper_driver.c
  eyedroppers/eyedropper_gpencil_color.c
  eyedroppers/interface_eyedropper.c
  interface.cc
  interface_align.c
  interface_anim.cc
  interface_button_group.c
  interface_context_menu.c
  interface_context_path.cc
  interface_drag.cc
  interface_draw.c
  interface_dropboxes.cc
  interface_handlers.c
  interface_icons.c
  interface_icons_event.c
  interface_layout.c
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
  interface_template_list.cc
  interface_template_search_menu.cc
  interface_template_search_operator.cc
  interface_templates.c
  interface_undo.cc
  interface_utils.cc
  interface_widgets.c
  resources.c
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

  eyedroppers/eyedropper_intern.h
  interface_intern.h
  interface_regions_intern.hh
)

set(LIB
  bf_blenkernel
  bf_blenlib
  bf_editor_datafiles
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
