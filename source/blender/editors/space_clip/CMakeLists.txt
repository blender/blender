# SPDX-FileCopyrightText: 2011 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../../blenfont
  ../../blenloader
  ../../gpu
  ../../imbuf
  ../../makesrna

  # RNA_prototypes.h
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS
)

set(SRC
  clip_buttons.cc
  clip_dopesheet_draw.cc
  clip_dopesheet_ops.cc
  clip_draw.cc
  clip_editor.cc
  clip_graph_draw.cc
  clip_graph_ops.cc
  clip_ops.cc
  clip_utils.cc
  space_clip.cc
  tracking_ops.cc
  tracking_ops_detect.cc
  tracking_ops_orient.cc
  tracking_ops_plane.cc
  tracking_ops_solve.cc
  tracking_ops_stabilize.cc
  tracking_ops_track.cc
  tracking_ops_utils.cc
  tracking_select.cc

  clip_intern.h
  tracking_ops_intern.h
)

set(LIB
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::blentranslation
  PRIVATE bf::depsgraph
  PRIVATE bf::dna
  PRIVATE bf::intern::guardedalloc
  PRIVATE bf::windowmanager
)


blender_add_lib(bf_editor_space_clip "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

# RNA_prototypes.h
add_dependencies(bf_editor_space_clip bf_rna)
