# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  PUBLIC .
  intern

  ../editors/include
  ../makesrna
  # RNA_prototypes.hh
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS
  ../../../extern/fmtlib/include
)

set(SRC
  intern/action.cc
  intern/action_iterators.cc
  intern/action_legacy.cc
  intern/action_runtime.cc
  intern/action_selection.cc
  intern/anim_rna.cc
  intern/animdata.cc
  intern/bone_collections.cc
  intern/bonecolor.cc
  intern/driver.cc
  intern/evaluation.cc
  intern/fcurve.cc
  intern/keyframing.cc
  intern/keyframing_auto.cc
  intern/keyingsets.cc
  intern/nla.cc
  intern/pose.cc
  intern/versioning.cc
  intern/visualkey.cc

  ANIM_action.hh
  ANIM_action_iterators.hh
  ANIM_action_legacy.hh
  ANIM_animdata.hh
  ANIM_armature.hh
  ANIM_armature_iter.hh
  ANIM_bone_collections.hh
  ANIM_bonecolor.hh
  ANIM_driver.hh
  ANIM_evaluation.hh
  ANIM_fcurve.hh
  ANIM_keyframing.hh
  ANIM_keyingsets.hh
  ANIM_nla.hh
  ANIM_pose.hh
  ANIM_rna.hh
  ANIM_versioning.hh
  ANIM_visualkey.hh
  intern/action_runtime.hh
  intern/bone_collections_internal.hh
  intern/evaluation_internal.hh
)

set(LIB
  PRIVATE bf::blenkernel
  bf::blenlib
  PRIVATE bf::blenloader
  PRIVATE bf::blentranslation
  PRIVATE bf::depsgraph
  bf::dna
  PRIVATE bf::intern::guardedalloc
  PRIVATE bf::intern::atomic
  PRIVATE bf::intern::clog
  PRIVATE bf::windowmanager
)

blender_add_lib(bf_animrig "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
add_library(bf::animrig ALIAS bf_animrig)

if(WITH_GTESTS)
  set(TEST_INC
  )
  set(TEST_SRC
    intern/action_iterators_test.cc
    intern/action_legacy_test.cc
    intern/action_test.cc
    intern/bone_collections_test.cc
    intern/evaluation_test.cc
    intern/keyframing_test.cc
    intern/nla_test.cc
    intern/pose_test.cc
    intern/versioning_test.cc
  )
  set(TEST_LIB
    PRIVATE bf::animrig
  )
  blender_add_test_suite_lib(animrig "${TEST_SRC}" "${INC};${TEST_INC}" "${INC_SYS}" "${LIB};${TEST_LIB}")
endif()
