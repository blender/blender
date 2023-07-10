# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../../blenfont
  ../../blenkernel
  ../../blenlib
  ../../blenloader
  ../../blentranslation
  ../../depsgraph
  ../../draw
  ../../gpu
  ../../imbuf
  ../../makesdna
  ../../makesrna
  ../../sequencer
  ../../windowmanager
  ../../../../intern/guardedalloc

  # dna_type_offsets.h
  ${CMAKE_CURRENT_BINARY_DIR}/../../makesdna/intern
  # RNA_prototypes.h
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS
)

set(SRC
  sequencer_add.cc
  sequencer_buttons.cc
  sequencer_channels_draw.cc
  sequencer_channels_edit.cc
  sequencer_drag_drop.cc
  sequencer_draw.cc
  sequencer_edit.cc
  sequencer_gizmo_retime.cc
  sequencer_gizmo_retime_type.cc
  sequencer_modifier.cc
  sequencer_ops.cc
  sequencer_preview.cc
  sequencer_proxy.cc
  sequencer_retiming.cc
  sequencer_scopes.cc
  sequencer_select.cc
  sequencer_thumbnails.cc
  sequencer_view.cc
  space_sequencer.cc

  sequencer_intern.h
)

set(LIB
  bf_editor_interface
  bf_editor_util
  PRIVATE bf::intern::atomic
)

if(WITH_AUDASPACE)
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


blender_add_lib(bf_editor_space_sequencer "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

# RNA_prototypes.h dna_type_offsets.h
add_dependencies(bf_editor_space_sequencer bf_rna)
