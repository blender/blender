# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../io
  ../../asset_system
  ../../gpu
  ../../makesrna
)

set(INC_SYS

)

set(SRC
  spacetypes.cc
)

set(LIB
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::dna
  bf_editor_geometry
  bf_editor_space_action
  bf_editor_space_buttons
  bf_editor_space_clip
  bf_editor_space_console
  bf_editor_space_file
  bf_editor_space_graph
  bf_editor_space_image
  bf_editor_space_info
  bf_editor_space_nla
  bf_editor_space_node
  bf_editor_space_outliner
  bf_editor_space_script
  bf_editor_space_sequencer
  bf_editor_space_spreadsheet
  bf_editor_space_statusbar
  bf_editor_space_text
  bf_editor_space_topbar
  bf_editor_space_userpref
  bf_editor_space_view3d
  PRIVATE bf::intern::guardedalloc
  PRIVATE bf::windowmanager
)

blender_add_lib(bf_editor_space_api "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
