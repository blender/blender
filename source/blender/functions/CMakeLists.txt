# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
)

set(INC_SYS
)

set(SRC
  intern/cpp_types.cc
  intern/field.cc
  intern/field_cpp_type.cc
  intern/lazy_function.cc
  intern/lazy_function_execute.cc
  intern/lazy_function_graph.cc
  intern/lazy_function_graph_executor.cc
  intern/multi_function.cc
  intern/multi_function_builder.cc
  intern/multi_function_params.cc
  intern/multi_function_procedure.cc
  intern/multi_function_procedure_builder.cc
  intern/multi_function_procedure_executor.cc
  intern/multi_function_procedure_optimization.cc

  FN_field.hh
  FN_field_cpp_type.hh
  FN_field_cpp_type_make.hh
  FN_init.h
  FN_lazy_function.hh
  FN_lazy_function_execute.hh
  FN_lazy_function_graph.hh
  FN_lazy_function_graph_executor.hh
  FN_multi_function.hh
  FN_multi_function_builder.hh
  FN_multi_function_context.hh
  FN_multi_function_data_type.hh
  FN_multi_function_param_type.hh
  FN_multi_function_params.hh
  FN_multi_function_procedure.hh
  FN_multi_function_procedure_builder.hh
  FN_multi_function_procedure_executor.hh
  FN_multi_function_procedure_optimization.hh
  FN_multi_function_signature.hh
)

set(LIB
  PRIVATE bf::blenlib
  PRIVATE bf::dna
  PRIVATE bf::intern::guardedalloc
)

if(WITH_TBB)
  add_definitions(-DWITH_TBB)
  if(WIN32)
    # TBB includes Windows.h which will define min/max macros
    # that will collide with the stl versions.
    add_definitions(-DNOMINMAX)
  endif()
  list(APPEND INC_SYS
    ${TBB_INCLUDE_DIRS}
  )

  list(APPEND LIB
    ${TBB_LIBRARIES}
  )
endif()

blender_add_lib(bf_functions "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

if(WITH_GTESTS)
  set(TEST_SRC
    tests/FN_field_test.cc
    tests/FN_lazy_function_test.cc
    tests/FN_multi_function_procedure_test.cc
    tests/FN_multi_function_test.cc

    tests/FN_multi_function_test_common.hh
  )
  set(TEST_LIB
    bf_functions
  )
  include(GTestTesting)
  blender_add_test_lib(bf_functions_tests "${TEST_SRC}" "${INC};${TEST_INC}" "${INC_SYS}" "${LIB};${TEST_LIB}")
endif()
