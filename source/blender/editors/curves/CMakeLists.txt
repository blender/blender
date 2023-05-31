# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../../blenkernel
  ../../blenlib
  ../../blentranslation
  ../../depsgraph
  ../../functions
  ../../geometry
  ../../gpu
  ../../makesdna
  ../../makesrna
  ../../windowmanager
  ../../../../intern/clog
  ../../../../intern/guardedalloc
  ../../bmesh

  # RNA_prototypes.h
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(SRC
  intern/curves_add.cc
  intern/curves_data.cc
  intern/curves_edit.cc
  intern/curves_ops.cc
  intern/curves_selection.cc
  intern/curves_undo.cc
)

set(LIB
  bf_blenkernel
  bf_blenlib
)

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

blender_add_lib(bf_editor_curves "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
add_dependencies(bf_editor_curves bf_rna)
