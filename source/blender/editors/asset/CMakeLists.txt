# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  ../include
  ../../asset_system
  ../../blenkernel
  ../../blenlib
  ../../blenloader
  ../../blentranslation
  ../../makesdna
  ../../makesrna
  ../../windowmanager
  ../../../../intern/clog
  ../../../../intern/guardedalloc
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
  intern/asset_ops.cc
  intern/asset_temp_id_consumer.cc
  intern/asset_type.cc
  intern/asset_view_catalog_filter.cc

  ED_asset_catalog.h
  ED_asset_catalog.hh
  ED_asset_filter.h
  ED_asset_handle.h
  ED_asset_import.h
  ED_asset_indexer.h
  ED_asset_library.h
  ED_asset_list.h
  ED_asset_list.hh
  ED_asset_mark_clear.h
  ED_asset_temp_id_consumer.h
  ED_asset_type.h
  ED_asset_view_catalog_filter.h
  intern/asset_library_reference.hh
)

set(LIB
  bf_blenkernel
  bf_blenloader
)

blender_add_lib(bf_editor_asset "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

# RNA_prototypes.h
add_dependencies(bf_editor_asset bf_rna)
