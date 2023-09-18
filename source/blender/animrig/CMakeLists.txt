# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  PUBLIC .
  intern

  ../blenkernel
  ../editors/include
)

set(INC_SYS
)

set(SRC
  intern/bone_collections.cc
  intern/bonecolor.cc

  ANIM_bone_collections.h
  ANIM_bone_collections.hh
  ANIM_bonecolor.hh
)

set(LIB
  bf_blenkernel
  bf::blenlib
  bf::dna
  PRIVATE bf_editor_interface
  PRIVATE bf::intern::guardedalloc
)


blender_add_lib(bf_animrig "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
add_library(bf::animrig ALIAS bf_animrig)

if(WITH_GTESTS)
  set(TEST_SRC
    intern/bone_collections_test.cc
  )
  set(TEST_LIB
    PRIVATE bf::animrig
  )
  include(GTestTesting)
  blender_add_test_lib(bf_animrig_tests "${TEST_SRC}" "${INC};${TEST_INC}" "${INC_SYS}" "${LIB};${TEST_LIB}")
endif()
