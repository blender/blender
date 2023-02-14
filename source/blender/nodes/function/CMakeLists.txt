# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright 2021 Blender Foundation. All rights reserved.

set(INC
  .
  ..
  ../intern
  ../../blenkernel
  ../../blenlib
  ../../blentranslation
  ../../editors/include
  ../../functions
  ../../makesdna
  ../../makesrna
  ../../windowmanager
  ../../../../intern/guardedalloc
)

set(INC_SYS
)

set(SRC
  nodes/node_fn_align_euler_to_vector.cc
  nodes/node_fn_boolean_math.cc
  nodes/node_fn_combine_color.cc
  nodes/node_fn_compare.cc
  nodes/node_fn_float_to_int.cc
  nodes/node_fn_input_bool.cc
  nodes/node_fn_input_color.cc
  nodes/node_fn_input_int.cc
  nodes/node_fn_input_special_characters.cc
  nodes/node_fn_input_string.cc
  nodes/node_fn_input_vector.cc
  nodes/node_fn_random_value.cc
  nodes/node_fn_replace_string.cc
  nodes/node_fn_rotate_euler.cc
  nodes/node_fn_separate_color.cc
  nodes/node_fn_slice_string.cc
  nodes/node_fn_string_length.cc
  nodes/node_fn_value_to_string.cc

  node_function_register.cc
  node_function_util.cc

  node_function_register.hh
  node_function_util.hh
)

set(LIB
  bf_functions
)

blender_add_lib(bf_nodes_function "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

if(WITH_UNITY_BUILD)
  set_target_properties(bf_nodes_function PROPERTIES UNITY_BUILD ON)
  set_target_properties(bf_nodes_function PROPERTIES UNITY_BUILD_BATCH_SIZE 10)
endif()
