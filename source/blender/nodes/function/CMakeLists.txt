# ***** BEGIN GPL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# The Original Code is Copyright (C) 2021, Blender Foundation
# All rights reserved.
# ***** END GPL LICENSE BLOCK *****

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


set(SRC
  nodes/legacy/node_fn_random_float.cc

  nodes/node_fn_align_euler_to_vector.cc
  nodes/node_fn_boolean_math.cc
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
  nodes/node_fn_slice_string.cc
  nodes/node_fn_string_length.cc
  nodes/node_fn_value_to_string.cc

  node_function_util.cc

  node_function_util.hh
)

set(LIB
  bf_functions
)

if(WITH_INTERNATIONAL)
  add_definitions(-DWITH_INTERNATIONAL)
endif()

blender_add_lib(bf_nodes_function "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

if(WITH_UNITY_BUILD)
  set_target_properties(bf_nodes_function PROPERTIES UNITY_BUILD ON)
  set_target_properties(bf_nodes_function PROPERTIES UNITY_BUILD_BATCH_SIZE 10)
endif()
