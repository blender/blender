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
# Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# The Original Code is Copyright (C) 2006, Blender Foundation
# All rights reserved.
#
# The Original Code is: all of this file.
#
# Contributor(s): Jacques Beaurain.
#
# ***** END GPL LICENSE BLOCK *****

FILE(GLOB SRC intern/source/*.c)

SET(INC 
	intern/include ../../../intern/guardedalloc ../blenlib ../makesdna
	extern/include ../blenkernel ../imbuf
	../include ../../kernel/gen_messaging ../blenloader
	../../../intern/smoke/extern
	../makesrna
)

IF(WIN32)
	SET(INC ${INC} ${PTHREADS_INC})
ENDIF(WIN32)

IF(WITH_OPENEXR)
	ADD_DEFINITIONS(-DWITH_OPENEXR)
ENDIF(WITH_OPENEXR)

IF(WITH_QUICKTIME)
	SET(INC ${INC} ../quicktime ${QUICKTIME_INC})
	ADD_DEFINITIONS(-DWITH_QUICKTIME)
ENDIF(WITH_QUICKTIME)

#TODO
#if env['OURPLATFORM']=='linux2':
#    cflags='-pthread'

BLENDERLIB_NOLIST(blender_render "${SRC}" "${INC}")
#env.BlenderLib ( libname = 'blender_render', sources = sources, includes = Split(incs), defines=defs, libtype='core', priority=45, compileflags=cflags )
