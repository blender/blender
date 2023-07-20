# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../../blenloader
  ../../blentranslation
  ../../bmesh
  ../../draw
  ../../gpu
  ../../imbuf
  ../../makesrna
  ../../nodes
  ../../render
  ../../sequencer
  # RNA_prototypes.h
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(SRC
  render_internal.cc
  render_opengl.cc
  render_ops.cc
  render_preview.cc
  render_shading.cc
  render_update.cc
  render_view.cc

  render_intern.hh
)

set(LIB
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::depsgraph
  PRIVATE bf::dna
  bf_draw
  PRIVATE bf::intern::guardedalloc
  PRIVATE bf::windowmanager
)

if(WITH_HEADLESS)
  add_definitions(-DWITH_HEADLESS)
else()
  list(APPEND LIB
    bf_editor_datafiles
  )
endif()


if(WITH_FREESTYLE)
  list(APPEND INC
    ../../freestyle
  )
  add_definitions(-DWITH_FREESTYLE)
endif()

if(WITH_TBB)
  list(APPEND INC_SYS
    ${TBB_INCLUDE_DIRS}
  )
  add_definitions(-DWITH_TBB)
  if(WIN32)
    # TBB includes Windows.h which will define min/max macros
    # that will collide with the stl versions.
    add_definitions(-DNOMINMAX)
  endif()
endif()

blender_add_lib(bf_editor_render "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

# RNA_prototypes.h
add_dependencies(bf_editor_render bf_rna)
