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

FILE(GLOB SRC intern/*.c)

SET(INC 
	.
	../makesdna
	../blenkernel
	../include
	../gpu
	../../../intern/ghost
	../../../intern/guardedalloc
	${ZLIB_INC}
	${FREETYPE_INCLUDE_DIRS}
)

# unused
LIST(REMOVE_ITEM SRC "${CMAKE_CURRENT_SOURCE_DIR}/intern/BLI_bfile.c")


IF(CMAKE_SYSTEM_NAME MATCHES "Linux")
	LIST(APPEND INC "${BINRELOC_INC}")
ENDIF(CMAKE_SYSTEM_NAME MATCHES "Linux")

IF(WIN32)
	LIST(APPEND INC ${PTHREADS_INC})
ENDIF(WIN32)

IF(WITH_OPENMP)
	ADD_DEFINITIONS(-DPARALLEL=1)
ENDIF(WITH_OPENMP)

BLENDERLIB(bf_blenlib "${SRC}" "${INC}")
