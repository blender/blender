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
# The Original Code is Copyright (C) 2014, Blender Foundation
# All rights reserved.
# ***** END GPL LICENSE BLOCK *****

set(INC
  .
  ..

  ${FFMPEG_INCLUDE_DIRS}
  ${PNG_INCLUDE_DIRS}
  ${ZLIB_INCLUDE_DIRS}
)

set(LIB
  ${PNG_LIBRARIES}
  ${FFMPEG_LIBRARIES}
  ${ZLIB_LIBRARIES}
)

if(WITH_IMAGE_OPENJPEG)
  set(LIB ${LIB} ${OPENJPEG_LIBRARIES})
endif()

setup_platform_linker_flags()
link_directories(${FFMPEG_LIBPATH} ${PNG_LIBPATH} ${ZLIB_LIBPATH})
include_directories(${INC})

BLENDER_SRC_GTEST(ffmpeg "ffmpeg_codecs.cc" "${LIB}")
