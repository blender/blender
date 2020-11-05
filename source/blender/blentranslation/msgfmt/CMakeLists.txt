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
# The Original Code is Copyright (C) 2017, Blender Foundation
# All rights reserved.
# ***** END GPL LICENSE BLOCK *****

# -----------------------------------------------------------------------------
# Build msgfmt executable

blender_include_dirs(
  ../../../../intern/guardedalloc
  ../../blenlib
)

set(SRC
  msgfmt.c
)

setup_libdirs()
add_cc_flags_custom_test(msgfmt)

if(WIN32)
  string(APPEND CMAKE_EXE_LINKER_FLAGS_DEBUG " /nodefaultlib:MSVCRT.lib")
endif()

add_executable(msgfmt ${SRC})
setup_platform_linker_flags(msgfmt)

target_link_libraries(msgfmt bf_blenlib)
target_link_libraries(msgfmt bf_intern_guardedalloc)

if(WIN32)
  target_link_libraries(msgfmt bf_intern_utfconv)
endif()

target_link_libraries(msgfmt ${ZLIB_LIBRARIES})
target_link_libraries(msgfmt ${PLATFORM_LINKLIBS})
