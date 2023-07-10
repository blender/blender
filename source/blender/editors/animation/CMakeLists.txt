# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../../blenkernel
  ../../blentranslation
  ../../depsgraph
  ../../gpu
  ../../makesrna
  ../../sequencer
  ../../windowmanager
  ../../../../intern/clog
  # RNA_prototypes.h
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS
)

set(SRC
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

  anim_intern.h
)

set(LIB
  bf_blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::dna
  PRIVATE bf::intern::guardedalloc
)

if(WITH_PYTHON)
  add_definitions(-DWITH_PYTHON)
endif()


blender_add_lib(bf_editor_animation "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

# RNA_prototypes.h
add_dependencies(bf_editor_animation bf_rna)

if(WITH_GTESTS)
  set(TEST_SRC
    keyframes_keylist_test.cc
  )
  set(TEST_INC
  )
  set(TEST_LIB
  )
  include(GTestTesting)
  blender_add_test_lib(bf_editor_animation_tests "${TEST_SRC}" "${INC};${TEST_INC}" "${INC_SYS}" "${LIB};${TEST_LIB}")
endif()
