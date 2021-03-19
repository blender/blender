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
# ***** END GPL LICENSE BLOCK *****

set(INC
  .
  ../blenlib
  ../makesdna
  ../../../intern/guardedalloc
)

set(INC_SYS
)

set(SRC
  intern/cpp_types.cc
  intern/multi_function.cc
  intern/multi_function_builder.cc
  intern/multi_function_network.cc
  intern/multi_function_network_evaluation.cc
  intern/multi_function_network_optimization.cc

  FN_array_spans.hh
  FN_cpp_type.hh
  FN_generic_pointer.hh
  FN_generic_value_map.hh
  FN_generic_vector_array.hh
  FN_multi_function.hh
  FN_multi_function_builder.hh
  FN_multi_function_context.hh
  FN_multi_function_data_type.hh
  FN_multi_function_network.hh
  FN_multi_function_network_evaluation.hh
  FN_multi_function_network_optimization.hh
  FN_multi_function_param_type.hh
  FN_multi_function_params.hh
  FN_multi_function_signature.hh
  FN_spans.hh
)

set(LIB
  bf_blenlib
)

blender_add_lib(bf_functions "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

if(WITH_GTESTS)
  set(TEST_SRC
    tests/FN_array_spans_test.cc
    tests/FN_cpp_type_test.cc
    tests/FN_generic_vector_array_test.cc
    tests/FN_multi_function_network_test.cc
    tests/FN_multi_function_test.cc
    tests/FN_spans_test.cc
  )
  set(TEST_LIB
    bf_functions
  )
  include(GTestTesting)
  blender_add_test_lib(bf_functions_tests "${TEST_SRC}" "${INC};${TEST_INC}" "${INC_SYS}" "${LIB};${TEST_LIB}")
endif()
