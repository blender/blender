# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../../makesrna

  # RNA_prototypes.hh
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS
)

set(SRC
  sequencer_add.cc
  sequencer_add_menu_scene.cc
  sequencer_buttons.cc
  sequencer_channels_draw.cc
  sequencer_channels_edit.cc
  sequencer_clipboard.cc
  sequencer_drag_drop.cc
  sequencer_edit.cc
  sequencer_modifier.cc
  sequencer_ops.cc
  sequencer_preview.cc
  sequencer_preview_draw.cc
  sequencer_proxy.cc
  sequencer_quads_batch.cc
  sequencer_retiming.cc
  sequencer_retiming_draw.cc
  sequencer_scopes.cc
  sequencer_select.cc
  sequencer_strips_batch.cc
  sequencer_text_edit.cc
  sequencer_thumbnails.cc
  sequencer_timeline_draw.cc
  sequencer_view.cc
  space_sequencer.cc

  sequencer_intern.hh
  sequencer_quads_batch.hh
  sequencer_scopes.hh
  sequencer_strips_batch.hh
)

set(LIB
  PRIVATE bf::asset_system
  PRIVATE bf::blenfont
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::blenloader
  PRIVATE bf::blentranslation
  PRIVATE bf::depsgraph
  PRIVATE bf::dna
  PRIVATE bf::draw
  bf_editor_interface
  bf_editor_util
  PRIVATE bf::gpu
  PRIVATE bf::imbuf
  PRIVATE bf::imbuf::movie
  PRIVATE bf::intern::atomic
  PRIVATE bf::intern::guardedalloc
  PRIVATE bf::extern::fmtlib
  PRIVATE bf::animrig
  PRIVATE bf::sequencer
  PRIVATE bf::windowmanager
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

# RNA_prototypes.hh dna_type_offsets.h
add_dependencies(bf_editor_space_sequencer bf_rna)
