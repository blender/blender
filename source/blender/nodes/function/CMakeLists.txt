# SPDX-FileCopyrightText: 2021 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  ..
  ../intern
  ../../blenkernel
  ../../blentranslation
  ../../editors/include
  ../../functions
  ../../makesrna
  ../../windowmanager
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

  node_function_util.cc

  node_function_util.hh
)

set(LIB
  PRIVATE bf::blenlib
  PRIVATE bf::dna
  bf_functions
  PRIVATE bf::intern::guardedalloc
)

add_node_discovery(
  bf_nodes_functions_generated
  "${SRC}"
  ${CMAKE_CURRENT_BINARY_DIR}/register_function_nodes.cc
  register_function_nodes
)

list(APPEND LIB
  bf_nodes_functions_generated
)


blender_add_lib(bf_nodes_function "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
target_link_libraries(bf_nodes_functions_generated bf_nodes_function)

if(WITH_UNITY_BUILD)
  set_target_properties(bf_nodes_function PROPERTIES UNITY_BUILD ON)
  set_target_properties(bf_nodes_function PROPERTIES UNITY_BUILD_BATCH_SIZE 10)
endif()
