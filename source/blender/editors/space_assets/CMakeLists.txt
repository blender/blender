# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../../asset_system
  ../../blenfont
  ../../blenkernel
  ../../blenlib
  ../../blentranslation
  ../../gpu
  ../../makesdna
  ../../makesrna
  ../../windowmanager
  ../../../../intern/guardedalloc
  # RNA_prototypes.h
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(SRC
  asset_browser_draw.cc
  asset_browser_ops.cc
  asset_browser_panels.cc
  asset_catalog_tree_view.cc
  asset_view.cc
  space_assets.cc

  asset_browser_intern.hh
  asset_view.hh
)

set(LIB
)

blender_add_lib(bf_editor_space_assets "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

# RNA_prototypes.h
add_dependencies(bf_editor_space_assets bf_rna)
