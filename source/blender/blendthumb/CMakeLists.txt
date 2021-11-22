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
# The Original Code is Copyright (C) 2006, Blender Foundation
# All rights reserved.
# ***** END GPL LICENSE BLOCK *****

#-----------------------------------------------------------------------------
# Shared Thumbnail Extraction Logic

include_directories(
  ../blenlib
  ../makesdna
  ../../../intern/guardedalloc
)

include_directories(
  SYSTEM
  ${ZLIB_INCLUDE_DIRS}
)

set(SRC
  src/blendthumb.hh
  src/blendthumb_extract.cc
  src/blendthumb_png.cc
)

if(WIN32)
  # -----------------------------------------------------------------------------
  # Build `BlendThumb.dll`

  set(SRC_WIN32
    src/blendthumb_win32.cc
    src/blendthumb_win32.def
    src/blendthumb_win32.rc
    src/blendthumb_win32_dll.cc
  )

  add_definitions(-DNOMINMAX)

  add_library(BlendThumb SHARED ${SRC} ${SRC_WIN32})

  target_link_libraries(BlendThumb bf_blenlib dbghelp.lib Version.lib)
  set_target_properties(BlendThumb PROPERTIES LINK_FLAGS_DEBUG "/NODEFAULTLIB:msvcrt")

else()
  # -----------------------------------------------------------------------------
  # Build `blender-thumbnailer` executable

  set(SRC_CMD
    src/blender_thumbnailer.cc
  )

  add_executable(blender-thumbnailer ${SRC} ${SRC_CMD})
  target_link_libraries(blender-thumbnailer bf_blenlib)
  target_link_libraries(blender-thumbnailer ${PTHREADS_LIBRARIES})
endif()
