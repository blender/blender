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
#
# The Original Code is: all of this file.
#
# Contributor(s): Jacques Beaurain.
#
# ***** END GPL LICENSE BLOCK *****

#-----------------------------------------------------------------------------
cmake_minimum_required(VERSION 2.8)
project(BlendThumb)

#Bring the headers, such as Student.h into the project
include_directories(${ZLIB_INCLUDE})

#Can manually add the sources using the set command as follows:
set(SRC
	src/BlenderThumb.cpp
	src/BlendThumb.def
	src/BlendThumb.rc
	src/Dll.cpp
)

add_library(BlendThumb SHARED ${SRC})
target_link_libraries(BlendThumb ${ZLIB_LIBS})
install (TARGETS BlendThumb DESTINATION bin)
