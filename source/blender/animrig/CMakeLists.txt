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
  ../../../extern/fmtlib/include
  # RNA_prototypes.h
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS
)

set(SRC
  intern/action.cc
  intern/anim_rna.cc
  intern/animation.cc
  intern/animdata.cc
  intern/bone_collections.cc
  intern/bonecolor.cc
  intern/driver.cc
  intern/evaluation.cc
  intern/fcurve.cc
  intern/keyframing.cc
  intern/keyframing_auto.cc
  intern/visualkey.cc

  ANIM_action.hh
  ANIM_animation.hh
  ANIM_animdata.hh
  ANIM_armature_iter.hh
  ANIM_bone_collections.hh
  ANIM_bonecolor.hh
  ANIM_driver.hh
  ANIM_evaluation.hh
  ANIM_fcurve.hh
  ANIM_keyframing.hh
  ANIM_keyingsets.hh
  ANIM_rna.hh
  ANIM_visualkey.hh
  intern/bone_collections_internal.hh
  intern/evaluation_internal.hh
)

set(LIB
  bf_blenkernel
  bf::blenlib
  PRIVATE bf::depsgraph
  bf::dna
  PRIVATE bf_editor_interface
  PRIVATE bf::intern::guardedalloc
  PRIVATE bf::intern::atomic
  PRIVATE bf::intern::clog
)


blender_add_lib(bf_animrig "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
add_library(bf::animrig ALIAS bf_animrig)

if(WITH_GTESTS)
  set(TEST_INC
  )
  set(TEST_SRC
    intern/animation_test.cc
    intern/bone_collections_test.cc
    intern/evaluation_test.cc
  )
  set(TEST_LIB
    PRIVATE bf::animrig
  )
  blender_add_test_suite_lib(animrig "${TEST_SRC}" "${INC};${TEST_INC}" "${INC_SYS}" "${LIB};${TEST_LIB}")
endif()
