# SPDX-FileCopyrightText: 2006-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../../makesrna
  ../curves
  ../../../../extern/curve_fit_nd
  ../sculpt_paint
  ../../modifiers/intern/lineart
  # RNA_prototypes.hh
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS
)

set(SRC
  intern/grease_pencil_add.cc
  intern/grease_pencil_bake_animation.cc
  intern/grease_pencil_edit.cc
  intern/grease_pencil_frames.cc
  intern/grease_pencil_geom.cc
  intern/grease_pencil_image_render.cc
  intern/grease_pencil_join_selection.cc
  intern/grease_pencil_layers.cc
  intern/grease_pencil_lineart.cc
  intern/grease_pencil_material.cc
  intern/grease_pencil_merge.cc
  intern/grease_pencil_modes.cc
  intern/grease_pencil_ops.cc
  intern/grease_pencil_pen.cc
  intern/grease_pencil_primitive.cc
  intern/grease_pencil_randomize.cc
  intern/grease_pencil_select.cc
  intern/grease_pencil_trim.cc
  intern/grease_pencil_undo.cc
  intern/grease_pencil_utils.cc
  intern/grease_pencil_vertex_paint.cc
  intern/grease_pencil_weight_paint.cc
)

set(LIB
  PRIVATE bf::blenkernel
  PRIVATE bf::animrig
  PRIVATE bf::blenlib
  PRIVATE bf::blentranslation
  PRIVATE bf::bmesh
  PRIVATE bf::depsgraph
  PRIVATE bf::dna
  PRIVATE bf::functions
  PRIVATE bf::geometry
  PRIVATE bf::gpu
  PRIVATE bf::imbuf
  PRIVATE bf::intern::guardedalloc
  PRIVATE bf::intern::clog
  PRIVATE bf::windowmanager
  extern_curve_fit_nd
  extern_fmtlib
)

blender_add_lib(bf_editor_grease_pencil "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
add_dependencies(bf_editor_curves bf_rna)

if(WITH_GTESTS)
  set(TEST_SRC
    tests/grease_pencil_merge_test.cc
  )
  set(TEST_INC
  )
  set(TEST_LIB
  )
  blender_add_test_suite_lib(editor_grease_pencil "${TEST_SRC}" "${INC};${TEST_INC}" "${INC_SYS}" "${LIB};${TEST_LIB}")
endif()
