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
  ../../makesrna
  ../../windowmanager
  ../../../../intern/clog
  # dna_type_offsets.h
  ${CMAKE_CURRENT_BINARY_DIR}/../../makesdna/intern
  # RNA_prototypes.h
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS
)

set(SRC
  intern/asset_catalog.cc
  intern/asset_filter.cc
  intern/asset_handle.cc
  intern/asset_import.cc
  intern/asset_indexer.cc
  intern/asset_library_reference.cc
  intern/asset_library_reference_enum.cc
  intern/asset_list.cc
  intern/asset_mark_clear.cc
  intern/asset_menu_utils.cc
  intern/asset_ops.cc
  intern/asset_shelf.cc
  intern/asset_shelf_asset_view.cc
  intern/asset_shelf_catalog_selector.cc
  intern/asset_shelf_regiondata.cc
  intern/asset_shelf_settings.cc
  intern/asset_temp_id_consumer.cc
  intern/asset_type.cc

  ED_asset_catalog.h
  ED_asset_catalog.hh
  ED_asset_filter.hh
  ED_asset_handle.h
  ED_asset_import.hh
  ED_asset_indexer.h
  ED_asset_library.h
  ED_asset_list.h
  ED_asset_list.hh
  ED_asset_mark_clear.h
  ED_asset_shelf.h
  ED_asset_temp_id_consumer.h
  ED_asset_type.h
  intern/asset_library_reference.hh
  intern/asset_shelf.hh
)

set(LIB
  bf_blenkernel
  PRIVATE bf::blenlib
  bf_blenloader
  PRIVATE bf::dna
  PRIVATE bf::intern::guardedalloc
)

blender_add_lib(bf_editor_asset "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

# RNA_prototypes.h
add_dependencies(bf_editor_asset bf_rna)
