# SPDX-FileCopyrightText: 2020-2024 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

if(WITH_GTESTS)
  set(TEST_SRC
    tests/ffmpeg_codecs.cc
    tests/ffmpeg_cpu_flags.cc
  )
  set(TEST_INC
    .
  )
  set(TEST_INC_SYS
    ${FFMPEG_INCLUDE_DIRS}
    ${PNG_INCLUDE_DIRS}
    ${ZLIB_INCLUDE_DIRS}
  )
  set(TEST_LIB
    ${PNG_LIBRARIES}
    ${FFMPEG_LIBRARIES}
    ${ZLIB_LIBRARIES}
  )
  if(WITH_IMAGE_OPENJPEG)
    set(TEST_LIB ${TEST_LIB} ${OPENJPEG_LIBRARIES})
  endif()
  blender_add_test_suite_lib(ffmpeg_libs "${TEST_SRC}" "${TEST_INC}" "${TEST_INC_SYS}" "${TEST_LIB}")
endif()
