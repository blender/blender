# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../../blenkernel
  ../../blenlib
  ../../blentranslation
  ../../depsgraph
  ../../gpu
  ../../makesdna
  ../../makesrna
  ../../windowmanager
  ../../../../intern/glew-mx
  ../../../../intern/guardedalloc
  # RNA_prototypes.h
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
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
)

if(WITH_AUDASPACE)
  list(APPEND INC_SYS
    ${AUDASPACE_C_INCLUDE_DIRS}
  )
  list(APPEND LIB
    bf_intern_audaspace

    ${AUDASPACE_C_LIBRARIES}
    ${AUDASPACE_PY_LIBRARIES}
  )
  add_definitions(-DWITH_AUDASPACE)
endif()


blender_add_lib(bf_editor_space_graph "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

# RNA_prototypes.h
add_dependencies(bf_editor_space_graph bf_rna)
