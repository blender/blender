# ***** BEGIN LGPL LICENSE BLOCK *****
#
# Copyright 2009 Jrg Hermann Mller
#
# This file is part of AudaSpace.
#
# AudaSpace is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# AudaSpace is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with AudaSpace.  If not, see <http://www.gnu.org/licenses/>.
#
# ***** END LGPL LICENSE BLOCK *****

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
    ${AUDASPACE_PY_LIBRARIES}
  )
endif()

if(WITH_PYTHON)
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
