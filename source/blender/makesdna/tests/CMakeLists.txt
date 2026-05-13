# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC_SYS
)

if(WITH_GTESTS)
  # Output dna.cc
  add_custom_command(
    OUTPUT
      ${CMAKE_CURRENT_BINARY_DIR}/test_dna.cc
      ${CMAKE_CURRENT_BINARY_DIR}/test_dna_defaults.cc
      ${CMAKE_CURRENT_BINARY_DIR}/test_dna_struct_ids.cc
      ${CMAKE_CURRENT_BINARY_DIR}/test_dna_type_offsets.h
      ${CMAKE_CURRENT_BINARY_DIR}/test_dna_verify.cc
    COMMAND
      ${CMAKE_COMMAND} -E env ${PLATFORM_ENV_BUILD}
      "$<TARGET_FILE:makesdna>"
      --include-file dna_test.h
      ${CMAKE_CURRENT_BINARY_DIR}/test_dna.cc
      ${CMAKE_CURRENT_BINARY_DIR}/test_dna_type_offsets.h
      ${CMAKE_CURRENT_BINARY_DIR}/test_dna_verify.cc
      ${CMAKE_CURRENT_BINARY_DIR}/test_dna_struct_ids.cc
      ${CMAKE_CURRENT_BINARY_DIR}/test_dna_defaults.cc
      ${CMAKE_SOURCE_DIR}/source/blender/makesdna/tests/dna/
    DEPENDS
      makesdna
      dna/dna_test.h
  )

  set_source_files_properties(
    ${CMAKE_CURRENT_BINARY_DIR}/test_dna.cc
    ${CMAKE_CURRENT_BINARY_DIR}/test_dna_defaults.cc
    ${CMAKE_CURRENT_BINARY_DIR}/test_dna_struct_ids.cc
    ${CMAKE_CURRENT_BINARY_DIR}/test_dna_type_offsets.h
    ${CMAKE_CURRENT_BINARY_DIR}/test_dna_verify.cc
    PROPERTIES GENERATED TRUE
  )

  set(TEST_SRC
    makesdna_test.cc

    ${CMAKE_CURRENT_BINARY_DIR}/test_dna.cc
    ${CMAKE_CURRENT_BINARY_DIR}/test_dna_struct_ids.cc
    ${CMAKE_CURRENT_BINARY_DIR}/test_dna_type_offsets.h
    ${CMAKE_CURRENT_BINARY_DIR}/test_dna_verify.cc
  )
  set(TEST_INC
    PRIVATE ..
    PRIVATE ../intern
  )
  set(TEST_LIB
    # bf::blenlib twice to avoid ld link error with circular dependency.
    PRIVATE bf::blenlib
    PRIVATE bf::dna
    PRIVATE bf::blenlib
  )

  blender_add_test_executable(makesdna "${TEST_SRC}" "${TEST_INC}" "${INC_SYS}" "${TEST_LIB}")
endif()
