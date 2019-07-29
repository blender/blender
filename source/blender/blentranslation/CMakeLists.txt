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
# The Original Code is Copyright (C) 2008, Blender Foundation
# All rights reserved.
# ***** END GPL LICENSE BLOCK *****

set(INC
  .
  ../blenkernel
  ../blenlib
  ../imbuf
  ../makesdna
  ../makesrna
  ../../../intern/guardedalloc
  ../../../intern/locale
)

set(INC_SYS
)

set(SRC
  intern/blt_lang.c
  intern/blt_translation.c

  BLT_lang.h
  BLT_translation.h
)

set(LIB
  bf_blenkernel
  bf_blenlib
)

if(WITH_INTERNATIONAL)
  add_definitions(-DWITH_INTERNATIONAL)
  list(APPEND LIB
    bf_intern_locale
  )
endif()

if(WITH_PYTHON)
  add_definitions(-DWITH_PYTHON)
  list(APPEND INC
    ../python
  )
endif()

if(WIN32)
  if(WITH_INPUT_IME)
    add_definitions(-DWITH_INPUT_IME)
  endif()
endif()

blender_add_lib(bf_blentranslation "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

if(WITH_INTERNATIONAL)
  add_subdirectory(msgfmt)
endif()
