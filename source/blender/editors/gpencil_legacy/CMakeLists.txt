# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../../blenfont
  ../../blenkernel
  ../../blentranslation
  ../../bmesh
  ../../depsgraph
  ../../gpu
  ../../imbuf
  ../../makesrna
  ../../windowmanager
  # RNA_prototypes.h
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS
)

set(SRC
  annotate_draw.cc
  annotate_paint.cc
  drawgpencil.cc
  editaction_gpencil.cc
  gpencil_add_blank.cc
  gpencil_add_lineart.cc
  gpencil_add_monkey.cc
  gpencil_add_stroke.cc
  gpencil_armature.cc
  gpencil_bake_animation.cc
  gpencil_convert.cc
  gpencil_data.cc
  gpencil_edit.cc
  gpencil_edit_curve.cc
  gpencil_fill.cc
  gpencil_interpolate.cc
  gpencil_merge.cc
  gpencil_mesh.cc
  gpencil_ops.cc
  gpencil_ops_versioning.cc
  gpencil_paint.cc
  gpencil_primitive.cc
  gpencil_sculpt_paint.cc
  gpencil_select.cc
  gpencil_undo.cc
  gpencil_utils.cc
  gpencil_uv.cc
  gpencil_vertex_ops.cc
  gpencil_vertex_paint.cc
  gpencil_weight_paint.cc

  gpencil_intern.h
  gpencil_trace.h
)

set(LIB
  PRIVATE bf::animrig
  bf_blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::dna
  PRIVATE bf::intern::guardedalloc
)

if(WITH_POTRACE)
  list(APPEND SRC
    gpencil_trace_ops.cc
    gpencil_trace_utils.cc
  )
  list(APPEND INC
    ${POTRACE_INCLUDE_DIRS}
  )
  list(APPEND LIB
    ${POTRACE_LIBRARIES}
  )
  add_definitions(-DWITH_POTRACE)
endif()


blender_add_lib(bf_editor_gpencil_legacy "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

# RNA_prototypes.h
add_dependencies(bf_editor_gpencil_legacy bf_rna)
