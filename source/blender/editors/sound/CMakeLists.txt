# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../../blenkernel
  ../../blenlib
  ../../depsgraph
  ../../makesdna
  ../../makesrna
  ../../sequencer
  ../../windowmanager
  ../../../../intern/guardedalloc
)

set(INC_SYS

)

set(SRC
  sound_ops.c

  sound_intern.h
)

set(LIB
  bf_blenkernel
  bf_blenlib
)

if(WITH_AUDASPACE)
  list(APPEND INC_SYS
    ${AUDASPACE_C_INCLUDE_DIRS}
  )
  list(APPEND LIB
    bf_intern_audaspace

    ${AUDASPACE_C_LIBRARIES}
    ${AUDASPACE_PY_LIBRARIES}
  )
  add_definitions(-DWITH_AUDASPACE)
endif()

if(WITH_CODEC_FFMPEG)
  add_definitions(-DWITH_FFMPEG)
endif()

if(WITH_CODEC_SNDFILE)
  add_definitions(-DWITH_SNDFILE)
endif()

blender_add_lib(bf_editor_sound "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
