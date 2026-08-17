# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../asset
  ../include
  ../../makesrna
)

set(INC_SYS
)

set(SRC
  scene_compositor.cc
  scene_edit.cc
  scene_fps.cc
)

set(LIB
  PRIVATE bf::asset_system
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::blentranslation
  PRIVATE bf::depsgraph
  PRIVATE bf::dna
  PRIVATE bf::intern::guardedalloc
  PRIVATE bf::nodes
  PRIVATE bf::sequencer
  PRIVATE bf::windowmanager
)


blender_add_lib(bf_editor_scene "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
