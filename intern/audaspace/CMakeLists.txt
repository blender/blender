# SPDX-FileCopyrightText: 2009 Jrg Hermann Mller
#
# SPDX-License-Identifier: GPL-2.0-or-later

remove_strict_flags()

if(CMAKE_COMPILER_IS_GNUCC)
  remove_cc_flag("-Wunused-macros")
endif()
  set(INC
    .
  )

  set(INC_SYS
    ${AUDASPACE_C_INCLUDE_DIRS}
    ${AUDASPACE_PY_INCLUDE_DIRS}
  )

  set(SRC
    intern/AUD_Set.cpp
    intern/AUD_Set.h
  )

set(LIB
)

if(NOT WITH_SYSTEM_AUDASPACE)
  list(APPEND LIB
    audaspace
  )
  if(WITH_SDL_DYNLOAD)
    list(APPEND LIB
      extern_sdlew
    )
  endif()
else()
  list(APPEND LIB
    ${AUDASPACE_C_LIBRARIES}
  )
  if(WITH_PYTHON AND WITH_PYTHON_NUMPY)
    list(APPEND LIB
      ${AUDASPACE_PY_LIBRARIES}
    )
  endif()
endif()

if(WITH_PYTHON AND WITH_PYTHON_NUMPY)
  list(APPEND INC_SYS
    ${PYTHON_INCLUDE_DIRS}
  )
  list(APPEND SRC
    intern/AUD_PyInit.cpp
    intern/AUD_PyInit.h
  )
  list(APPEND LIB
      ${PYTHON_LINKFLAGS}
      ${PYTHON_LIBRARIES}
  )
  if(NOT WITH_SYSTEM_AUDASPACE)
    list(APPEND LIB
      audaspace-py
    )
  endif()

  add_definitions(-DWITH_PYTHON)
endif()

blender_add_lib(bf_intern_audaspace "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
