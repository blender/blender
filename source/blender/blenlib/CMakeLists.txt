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
	. ../makesdna ../blenkernel ../../../intern/guardedalloc ../include
	../gpu ../../../intern/ghost
	${FREETYPE_INCLUDE_DIRS}
	${ZLIB_INC}
)

IF(CMAKE_SYSTEM_NAME MATCHES "Linux")
SET(INC
	${INC}
	${BINRELOC_INC}
)
ENDIF(CMAKE_SYSTEM_NAME MATCHES "Linux")

IF(WIN32)
	SET(INC ${INC} ${PTHREADS_INC})
ENDIF(WIN32)

BLENDERLIB(bf_blenlib "${SRC}" "${INC}")
#if env['OURPLATFORM'] == 'linux2':
#    cflags='-pthread'
#
#env.BlenderLib ( 'bf_blenlib', sources, Split(incs), Split(defs), libtype=['core','player'], priority = [85,195], compileflags =cflags )
