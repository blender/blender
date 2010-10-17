# $Id$
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

# remove warning until render branch merged.
STRING(REGEX REPLACE "-Wunused-parameter" "" CMAKE_C_FLAGS ${CMAKE_C_FLAGS})

FILE(GLOB SRC intern/source/*.c intern/raytrace/*.cpp)

SET(INC 
	intern/include
	extern/include
	../blenlib
	../blenloader
	../makesdna
	../makesrna
	../blenkernel
	../imbuf
	../include
	../../kernel/gen_messaging
	../../../intern/smoke/extern
	../../../intern/guardedalloc
)

IF(WIN32)
	LIST(APPEND INC ${PTHREADS_INC})
ENDIF(WIN32)

IF(WITH_IMAGE_OPENEXR)
	ADD_DEFINITIONS(-DWITH_OPENEXR)
ENDIF(WITH_IMAGE_OPENEXR)

IF(WITH_QUICKTIME)
	LIST(APPEND INC ../quicktime ${QUICKTIME_INC})
	ADD_DEFINITIONS(-DWITH_QUICKTIME)
ENDIF(WITH_QUICKTIME)

IF(APPLE)
	IF(CMAKE_OSX_ARCHITECTURES MATCHES "i386" OR CMAKE_OSX_ARCHITECTURES MATCHES "x86_64")
		SET(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -mfpmath=sse")
		SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -mfpmath=sse")
	ENDIF(CMAKE_OSX_ARCHITECTURES MATCHES "i386" OR CMAKE_OSX_ARCHITECTURES MATCHES "x86_64")
ENDIF(APPLE)

BLENDERLIB_NOLIST(bf_render "${SRC}" "${INC}")
