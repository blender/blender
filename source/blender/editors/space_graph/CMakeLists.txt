# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../../blenkernel
  ../../blenlib
  ../../blenloader
  ../../blentranslation
  ../../depsgraph
  ../../gpu
  ../../makesrna
  ../../windowmanager
  ../../../../intern/guardedalloc

  # RNA_prototypes.h
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS
)

set(SRC
  graph_buttons.c
  graph_draw.c
  graph_edit.c
  graph_ops.c
  graph_select.c
  graph_slider_ops.c
  graph_utils.c
  graph_view.c
  space_graph.c

  graph_intern.h
)

set(LIB
  bf_blenkernel
  bf_blenlib
  PRIVATE bf::dna
)

if(WITH_AUDASPACE)
  list(APPEND LIB
    bf_intern_audaspace
  )
  list(APPEND INC_SYS
    ${AUDASPACE_C_INCLUDE_DIRS}
  )
  if(WITH_SYSTEM_AUDASPACE)
    list(APPEND LIB
      ${AUDASPACE_C_LIBRARIES}
      ${AUDASPACE_PY_LIBRARIES}
    )
  endif()
  add_definitions(-DWITH_AUDASPACE)
endif()


blender_add_lib(bf_editor_space_graph "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

# RNA_prototypes.h dna_type_offsets.h
add_dependencies(bf_editor_space_graph bf_rna)
