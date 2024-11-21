# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../../blenkernel
  ../../blentranslation
  ../../bmesh
  ../../functions
  ../../geometry
  ../../gpu
  ../../makesrna
  ../../windowmanager

  # RNA_prototypes.hh
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS
)

set(SRC
  intern/curves_add.cc
  intern/curves_attribute_set.cc
  intern/curves_data.cc
  intern/curves_draw.cc
  intern/curves_edit.cc
  intern/curves_extrude.cc
  intern/curves_masks.cc
  intern/curves_ops.cc
  intern/curves_selection.cc
  intern/curves_undo.cc
  intern/select_linked_pick.cc
)

set(LIB
  bf_blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::depsgraph
  PRIVATE bf::dna
  PRIVATE bf::extern::curve_fit_nd
  PRIVATE bf::extern::fmtlib
  PRIVATE bf::intern::clog
  PRIVATE bf::intern::guardedalloc
)

if(WITH_TBB)
  if(WIN32)
    # TBB includes Windows.h which will define min/max macros
    # that will collide with the STL versions.
    add_definitions(-DNOMINMAX)
  endif()
endif()

blender_add_lib(bf_editor_curves "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
add_dependencies(bf_editor_curves bf_rna)
