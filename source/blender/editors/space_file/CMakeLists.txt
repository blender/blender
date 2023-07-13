# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../asset
  ../include
  ../../asset_system
  ../../blenfont
  ../../blenloader
  ../../blentranslation
  ../../gpu
  ../../imbuf
  ../../makesrna
  ../../render
  ../../windowmanager
  ../../../../extern/fmtlib/include

  # RNA_prototypes.h
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS
)

set(SRC
  asset_catalog_tree_view.cc
  file_draw.cc
  file_indexer.cc
  file_ops.cc
  file_panels.cc
  file_utils.cc
  filelist.cc
  filesel.cc
  folder_history.cc
  fsmenu.c
  space_file.cc

  file_indexer.h
  file_intern.h
  filelist.h
  fsmenu.h
)

set(LIB
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::dna
  PRIVATE bf::intern::guardedalloc
  extern_fmtlib
  PRIVATE bf::intern::atomic
)

if(WIN32)
  add_definitions(-DNOMINMAX)
endif()

if(WITH_HEADLESS)
  add_definitions(-DWITH_HEADLESS)
else()
  list(APPEND LIB
    bf_editor_datafiles
  )
endif()

if(WITH_IMAGE_OPENEXR)
  add_definitions(-DWITH_OPENEXR)
endif()

if(WITH_IMAGE_OPENJPEG)
  add_definitions(-DWITH_OPENJPEG)
endif()

if(WITH_IMAGE_CINEON)
  add_definitions(-DWITH_CINEON)
endif()

if(WITH_IMAGE_WEBP)
  add_definitions(-DWITH_WEBP)
endif()

if(WITH_FREESTYLE)
  add_definitions(-DWITH_FREESTYLE)
endif()

blender_add_lib(bf_editor_space_file "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

# RNA_prototypes.h dna_type_offsets.h
add_dependencies(bf_editor_space_file bf_rna)
