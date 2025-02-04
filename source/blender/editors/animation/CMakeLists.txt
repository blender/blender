# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../asset
  ../include
  ../../makesrna
  ../../asset_system
  ../../../../extern/fmtlib/include
  # RNA_prototypes.hh
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS
)

set(SRC
  anim_asset_ops.cc
  anim_channels_defines.cc
  anim_channels_edit.cc
  anim_deps.cc
  anim_draw.cc
  anim_filter.cc
  anim_ipo_utils.cc
  anim_markers.cc
  anim_motion_paths.cc
  anim_ops.cc
  drivers.cc
  fmodifier_ui.cc
  keyframes_draw.cc
  keyframes_edit.cc
  keyframes_general.cc
  keyframes_keylist.cc
  keyframing.cc
  keyingsets.cc
  time_scrub_ui.cc

  anim_intern.hh
)

set(LIB
  PRIVATE bf::blenkernel
  PRIVATE bf::animrig
  PRIVATE bf::blenlib
  PRIVATE bf::blentranslation
  PRIVATE bf::depsgraph
  PRIVATE bf::dna
  PRIVATE bf::gpu
  PRIVATE bf::intern::clog
  PRIVATE bf::intern::guardedalloc
  PRIVATE bf::sequencer
  PRIVATE bf::windowmanager
)

if(WITH_PYTHON)
  add_definitions(-DWITH_PYTHON)
endif()

blender_add_lib(bf_editor_animation "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

# RNA_prototypes.hh
add_dependencies(bf_editor_animation bf_rna)

if(WITH_GTESTS)
  set(TEST_SRC
    anim_filter_test.cc
    keyframes_general_test.cc
    keyframes_keylist_test.cc
  )
  set(TEST_INC
  )
  set(TEST_LIB
  )
  blender_add_test_suite_lib(editor_animation "${TEST_SRC}" "${INC};${TEST_INC}" "${INC_SYS}" "${LIB};${TEST_LIB}")
endif()
