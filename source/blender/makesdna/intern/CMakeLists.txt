# SPDX-FileCopyrightText: 2006 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  PUBLIC ..
  PUBLIC ${CMAKE_CURRENT_BINARY_DIR}
  ../../blenlib
  ../../imbuf
  ../../imbuf/movie
)

set(INC_SYS

)

set(LIB
  PRIVATE bf::intern::atomic
  PRIVATE bf::intern::guardedalloc
  PRIVATE bf::dependencies::optional::tbb
  PRIVATE bf::extern::fmtlib
)

add_definitions(-DWITH_DNA_GHASH)

# Needed for `mallocn.cc`.
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
# Build `makesdna` executable.

# Subset of `blenlib` used by `makesdna`.
set(BLENLIB_SRC
  ../../blenlib/intern/BLI_assert.cc
  ../../blenlib/intern/BLI_ghash.cc
  ../../blenlib/intern/BLI_ghash_utils.cc
  ../../blenlib/intern/BLI_linklist.cc
  ../../blenlib/intern/BLI_memarena.cc
  ../../blenlib/intern/BLI_mempool.cc
  ../../blenlib/intern/listbase.cc
  ../../blenlib/intern/string.cc

  # Dependency of BLI_ghash.cc
  ../../blenlib/intern/hash_mm2a.cc

  # Dependencies of BLI_mempool.cc when ASAN is enabled.
  ../../blenlib/intern/gsqueue.cc
  ../../blenlib/intern/threads.cc
  ../../blenlib/intern/time.cc
)

set(SRC
  dna_utils.cc
  makesdna.cc
  ${BLENLIB_SRC}
  ${dna_header_include_file}
  ${dna_header_string_file}
)

# BLI_mempool.cc needs these files for ThreadMutex when
# compiling in address sanitizer mode.
if(WITH_COMPILER_ASAN)
  list(APPEND SRC
  )
endif()

# `SRC_DNA_INC` is defined in the parent directory.

add_cc_flags_custom_test(makesdna)

add_executable(makesdna ${SRC} ${SRC_DNA_INC})
setup_platform_linker_flags(makesdna)
setup_platform_linker_libs(makesdna)
blender_target_include_dirs(makesdna ${INC})
blender_target_include_dirs_sys(makesdna ${INC_SYS})
target_link_libraries(makesdna ${LIB})

if(WIN32 AND NOT UNIX)
  if(DEFINED PTHREADS_LIBRARIES)
    target_link_libraries(makesdna PRIVATE ${PTHREADS_LIBRARIES})
  endif()
endif()

# Output dna.cc
add_custom_command(
  OUTPUT
    ${CMAKE_CURRENT_BINARY_DIR}/dna.cc
    ${CMAKE_CURRENT_BINARY_DIR}/dna_struct_ids.cc
    ${CMAKE_CURRENT_BINARY_DIR}/dna_type_offsets.h
    ${CMAKE_CURRENT_BINARY_DIR}/dna_verify.cc
  COMMAND
    ${CMAKE_COMMAND} -E env ${PLATFORM_ENV_BUILD}
    "$<TARGET_FILE:makesdna>"
    ${CMAKE_CURRENT_BINARY_DIR}/dna.cc
    ${CMAKE_CURRENT_BINARY_DIR}/dna_type_offsets.h
    ${CMAKE_CURRENT_BINARY_DIR}/dna_verify.cc
    ${CMAKE_CURRENT_BINARY_DIR}/dna_struct_ids.cc
    ${CMAKE_SOURCE_DIR}/source/blender/makesdna/
  DEPENDS makesdna
)


# -----------------------------------------------------------------------------
# Build bf_dna library
set(SRC
  dna_defaults.c
  dna_genfile.cc
  dna_utils.cc
  ${CMAKE_CURRENT_BINARY_DIR}/dna.cc
  ${CMAKE_CURRENT_BINARY_DIR}/dna_struct_ids.cc
  ${CMAKE_CURRENT_BINARY_DIR}/dna_verify.cc
  ${SRC_DNA_INC}

  ${CMAKE_CURRENT_BINARY_DIR}/dna_type_offsets.h
  dna_rename_defs.h
  dna_utils.h
)

set_source_files_properties(
  ${CMAKE_CURRENT_BINARY_DIR}/dna.cc
  ${CMAKE_CURRENT_BINARY_DIR}/dna_struct_ids.cc
  ${CMAKE_CURRENT_BINARY_DIR}/dna_type_offsets.h
  ${CMAKE_CURRENT_BINARY_DIR}/dna_verify.cc
  PROPERTIES GENERATED TRUE
)

blender_add_lib(bf_dna "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
add_library(bf::dna ALIAS bf_dna)



# -----------------------------------------------------------------------------
# Build bf_dna_blenlib library
set(INC
  ..
  ../../blenlib
)

set(INC_SYS

)

set(SRC
  ${BLENLIB_SRC}
  ${SRC_DNA_DEFAULTS_INC}
  ${SRC_DNA_OTHER_INC}
)

set(LIB
  PRIVATE bf::intern::atomic
  PRIVATE bf::intern::guardedalloc
  PRIVATE bf::dependencies::optional::tbb
)

blender_add_lib(bf_dna_blenlib "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
