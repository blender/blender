# SPDX-FileCopyrightText: 2006 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

# message(STATUS "Configuring makesdna")

set(INC
  ..
  ../../blenlib
  ../../imbuf
  ../../../../intern/guardedalloc
  ${CMAKE_CURRENT_BINARY_DIR}
)

set(INC_SYS

)

set(LIB
)

add_definitions(-DWITH_DNA_GHASH)

# Needed for `mallocn.c`.
if(HAVE_MALLOC_STATS_H)
  add_definitions(-DHAVE_MALLOC_STATS_H)
endif()

set(dna_header_include_file "${CMAKE_CURRENT_BINARY_DIR}/dna_includes_all.h")
set(dna_header_string_file "${CMAKE_CURRENT_BINARY_DIR}/dna_includes_as_strings.h")

set(DNA_INCLUDE_TEXT "/* Do not edit manually, changes will be overwritten. */\n")
set(DNA_FILE_LIST "/* Do not edit manually, changes will be overwritten. */\n")
foreach(header ${SRC_DNA_INC})
  get_filename_component(dna_header_file ${header} NAME)
  string(APPEND DNA_INCLUDE_TEXT "#include \"${header}\"\n")
  string(APPEND DNA_FILE_LIST "\t\"${dna_header_file}\",\n")
endforeach()

file(GENERATE OUTPUT ${dna_header_include_file} CONTENT "${DNA_INCLUDE_TEXT}")
file(GENERATE OUTPUT ${dna_header_string_file} CONTENT "${DNA_FILE_LIST}")

# -----------------------------------------------------------------------------
# Build makesdna executable
set(SRC
  dna_utils.c
  makesdna.c
  ../../blenlib/intern/BLI_assert.c
  ../../blenlib/intern/BLI_ghash.c
  ../../blenlib/intern/BLI_ghash_utils.c
  ../../blenlib/intern/BLI_memarena.c
  ../../blenlib/intern/BLI_mempool.c
  ../../blenlib/intern/hash_mm2a.c  # needed by 'BLI_ghash_utils.c', not used directly.
  ../../../../intern/guardedalloc/intern/leak_detector.cc
  ../../../../intern/guardedalloc/intern/mallocn.c
  ../../../../intern/guardedalloc/intern/mallocn_guarded_impl.c
  ../../../../intern/guardedalloc/intern/mallocn_lockfree_impl.c
  ../../../../intern/guardedalloc/intern/memory_usage.cc
  ${dna_header_include_file}
  ${dna_header_string_file}
)

# BLI_mempool.c needs these files for ThreadMutex when
# compiling in address sanitizer mode.
if(WITH_COMPILER_ASAN)
  list(APPEND SRC
    ../../blenlib/intern/gsqueue.c
    ../../blenlib/intern/listbase.cc
    ../../blenlib/intern/threads.cc
    ../../blenlib/intern/time.c
  )
endif()

# SRC_DNA_INC is defined in the parent dir

add_cc_flags_custom_test(makesdna)

add_executable(makesdna ${SRC} ${SRC_DNA_INC})
setup_platform_linker_flags(makesdna)
blender_target_include_dirs(makesdna ${INC})
blender_target_include_dirs_sys(makesdna ${INC_SYS})
target_link_libraries(makesdna PRIVATE bf_intern_atomic)

if(WIN32 AND NOT UNIX)
  if(DEFINED PTHREADS_LIBRARIES)
    target_link_libraries(makesdna PRIVATE ${PTHREADS_LIBRARIES})
  endif()
endif()

# Output dna.c
add_custom_command(
  OUTPUT
    ${CMAKE_CURRENT_BINARY_DIR}/dna.c
    ${CMAKE_CURRENT_BINARY_DIR}/dna_type_offsets.h
    ${CMAKE_CURRENT_BINARY_DIR}/dna_verify.c
  COMMAND
    "$<TARGET_FILE:makesdna>"
    ${CMAKE_CURRENT_BINARY_DIR}/dna.c
    ${CMAKE_CURRENT_BINARY_DIR}/dna_type_offsets.h
    ${CMAKE_CURRENT_BINARY_DIR}/dna_verify.c
    ${CMAKE_SOURCE_DIR}/source/blender/makesdna/
  DEPENDS makesdna
)


# -----------------------------------------------------------------------------
# Build bf_dna library
set(SRC
  dna_defaults.c
  dna_genfile.c
  dna_utils.c
  ${CMAKE_CURRENT_BINARY_DIR}/dna.c
  ${CMAKE_CURRENT_BINARY_DIR}/dna_verify.c
  ${SRC_DNA_INC}

  ${CMAKE_CURRENT_BINARY_DIR}/dna_type_offsets.h
  dna_rename_defs.h
  dna_utils.h
)

set_source_files_properties(
  ${CMAKE_CURRENT_BINARY_DIR}/dna.c
  ${CMAKE_CURRENT_BINARY_DIR}/dna_type_offsets.h
  ${CMAKE_CURRENT_BINARY_DIR}/dna_verify.c
  PROPERTIES GENERATED TRUE
)

blender_add_lib(bf_dna "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")


# -----------------------------------------------------------------------------
# Build bf_dna_blenlib library
set(INC
  ..
  ../../blenlib
  ../../../../intern/guardedalloc
)

set(INC_SYS

)

set(SRC
  ../../blenlib/intern/BLI_assert.c
  ../../blenlib/intern/BLI_ghash.c
  ../../blenlib/intern/BLI_ghash_utils.c
  ../../blenlib/intern/BLI_linklist.c
  ../../blenlib/intern/BLI_memarena.c
  ../../blenlib/intern/BLI_mempool.c
  ../../blenlib/intern/endian_switch.c
  ../../blenlib/intern/hash_mm2a.c
  ../../blenlib/intern/listbase.cc

  ${SRC_DNA_DEFAULTS_INC}
)

set(LIB
  PRIVATE bf_intern_atomic
)

if(WITH_COMPILER_ASAN)
  list(APPEND SRC
    ../../blenlib/intern/gsqueue.c
    ../../blenlib/intern/threads.cc
    ../../blenlib/intern/time.c
  )
endif()

blender_add_lib(bf_dna_blenlib "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
