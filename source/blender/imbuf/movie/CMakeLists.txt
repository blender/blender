# SPDX-FileCopyrightText: 2024 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  PUBLIC .
)

set(INC_SYS
)

set(SRC
  MOV_enums.hh
  MOV_read.hh
  MOV_util.hh
  MOV_write.hh

  intern/ffmpeg_compat.h
  intern/movie_proxy_indexer.cc
  intern/movie_proxy_indexer.hh
  intern/movie_read.cc
  intern/movie_read.hh
  intern/movie_util.cc
  intern/movie_util.hh
  intern/movie_write.cc
  intern/movie_write.hh
  intern/movie_write_audio.cc
)

set(LIB
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PUBLIC  bf::imbuf
  PRIVATE bf::intern::guardedalloc
)

if(WITH_CODEC_FFMPEG)
  list(APPEND SRC
    intern/ffmpeg_swscale.cc
    intern/ffmpeg_swscale.hh
  )
  list(APPEND INC_SYS
    ${FFMPEG_INCLUDE_DIRS}
  )
  list(APPEND LIB
    ${FFMPEG_LIBRARIES}
    ${OPENJPEG_LIBRARIES}
  )
  add_definitions(-DWITH_FFMPEG)
endif()

if(WITH_AUDASPACE)
  list(APPEND INC_SYS
    ${AUDASPACE_C_INCLUDE_DIRS}
  )
  if(WITH_SYSTEM_AUDASPACE)
    list(APPEND LIB
      ${AUDASPACE_C_LIBRARIES}
    )
  endif()
  add_definitions(-DWITH_AUDASPACE)
endif()

blender_add_lib(bf_imbuf_movie "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
add_library(bf::imbuf::movie ALIAS bf_imbuf_movie)

if(WITH_GTESTS AND WITH_CODEC_FFMPEG)
  set(TEST_SRC
    tests/ffmpeg_codecs.cc
    tests/ffmpeg_cpu_flags.cc
  )
  set(TEST_INC
    intern
  )
  set(TEST_INC_SYS
    ${FFMPEG_INCLUDE_DIRS}
  )
  set(TEST_LIB
    ${FFMPEG_LIBRARIES}
  )
  if(WITH_IMAGE_OPENJPEG)
    set(TEST_LIB ${TEST_LIB} ${OPENJPEG_LIBRARIES})
  endif()
  blender_add_test_suite_lib(ffmpeg_libs "${TEST_SRC}" "${TEST_INC}" "${TEST_INC_SYS}" "${TEST_LIB}")

endif()
