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
  ../blentranslation
  ../gpu
  ../imbuf
  ../makesdna
  ../makesrna
  ../../../intern/glew-mx
  ../../../intern/guardedalloc
)

set(INC_SYS
  ${FREETYPE_INCLUDE_DIRS}
)

set(SRC
  intern/blf.c
  intern/blf_default.c
  intern/blf_dir.c
  intern/blf_font.c
  intern/blf_font_default.c
  intern/blf_glyph.c
  intern/blf_thumbs.c
  intern/blf_util.c

  BLF_api.h
  intern/blf_internal.h
  intern/blf_internal_types.h
)

set(LIB
  bf_gpu
  bf_intern_guardedalloc

  ${FREETYPE_LIBRARIES} ${BROTLI_LIBRARIES}
)

if(WIN32)
  list(APPEND SRC
    intern/blf_font_win32_compat.c
  )
endif()

if(WITH_INTERNATIONAL)
  add_definitions(-DWITH_INTERNATIONAL)
endif()

if(WITH_PYTHON)
  add_definitions(-DWITH_PYTHON)
  list(APPEND INC
    ../python
  )
endif()


blender_add_lib(bf_blenfont "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
