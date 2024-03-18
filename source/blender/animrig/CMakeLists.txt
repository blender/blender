# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  PUBLIC .
  intern

  ../blenkernel
  ../blentranslation
  ../editors/include
  ../makesrna
  ../windowmanager
  # RNA_prototypes.h
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS
)

set(SRC
  intern/action.cc
  intern/anim_rna.cc
  intern/animdata.cc
  intern/bone_collections.cc
  intern/bonecolor.cc
  intern/driver.cc
  intern/fcurve.cc
  intern/keyframing.cc
  intern/keyframing_auto.cc
  intern/visualkey.cc

  ANIM_action.hh
  ANIM_animdata.hh
  ANIM_armature_iter.hh
  ANIM_bone_collections.hh
  ANIM_bonecolor.hh
  ANIM_driver.hh
  ANIM_fcurve.hh
  ANIM_keyframing.hh
  ANIM_rna.hh
  ANIM_visualkey.hh
  intern/bone_collections_internal.hh
)

set(LIB
  bf_blenkernel
  bf::blenlib
  PRIVATE bf::depsgraph
  bf::dna
  PRIVATE bf_editor_interface
  PRIVATE bf::intern::guardedalloc
)


blender_add_lib(bf_animrig "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
add_library(bf::animrig ALIAS bf_animrig)

if(WITH_GTESTS)
  set(TEST_INC
  )
  set(TEST_SRC
    intern/bone_collections_test.cc
  )
  set(TEST_LIB
    PRIVATE bf::animrig
  )
  blender_add_test_suite_lib(animrig "${TEST_SRC}" "${INC};${TEST_INC}" "${INC_SYS}" "${LIB};${TEST_LIB}")
endif()
