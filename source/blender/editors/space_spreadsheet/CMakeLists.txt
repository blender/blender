# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../../blenfont
  ../../blenloader
  ../../blentranslation
  ../../bmesh
  ../../depsgraph
  ../../functions
  ../../gpu
  ../../makesrna
  ../../nodes
  ../../windowmanager

  # RNA_prototypes.h
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS
)

set(SRC
  space_spreadsheet.cc
  spreadsheet_cache.cc
  spreadsheet_column.cc
  spreadsheet_data_source.cc
  spreadsheet_data_source_geometry.cc
  spreadsheet_dataset_draw.cc
  spreadsheet_draw.cc
  spreadsheet_layout.cc
  spreadsheet_ops.cc
  spreadsheet_panels.cc
  spreadsheet_row_filter.cc
  spreadsheet_row_filter_ui.cc

  spreadsheet_cache.hh
  spreadsheet_column.hh
  spreadsheet_column_values.hh
  spreadsheet_data_source.hh
  spreadsheet_data_source_geometry.hh
  spreadsheet_dataset_draw.hh
  spreadsheet_draw.hh
  spreadsheet_intern.hh
  spreadsheet_layout.hh
  spreadsheet_row_filter.hh
  spreadsheet_row_filter_ui.hh
)

set(LIB
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::dna
  PRIVATE bf::intern::guardedalloc
)

if(WITH_OPENVDB)
  list(APPEND INC_SYS
    ${OPENVDB_INCLUDE_DIRS}
  )
  list(APPEND LIB
    ${OPENVDB_LIBRARIES}
  )
  add_definitions(-DWITH_OPENVDB ${OPENVDB_DEFINITIONS})
endif()

blender_add_lib(bf_editor_space_spreadsheet "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

# RNA_prototypes.h dna_type_offsets.h
add_dependencies(bf_editor_space_spreadsheet bf_rna)
