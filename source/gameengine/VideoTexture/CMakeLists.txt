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

FILE(GLOB SRC *.cpp)

SET(INC
	.
	../../../source/gameengine/Ketsji
	../../../source/gameengine/Expressions
	../../../source/gameengine/GameLogic
	../../../source/gameengine/SceneGraph
	../../../source/gameengine/Rasterizer
	../../../source/gameengine/Rasterizer/RAS_OpenGLRasterizer
	../../../source/gameengine/BlenderRoutines
	../../../source/blender/include
	../../../source/blender/blenlib
	../../../source/blender/blenkernel
	../../../source/blender/makesdna
	../../../source/blender/editors/include
	../../../source/blender/imbuf
	../../../source/blender/python
	../../../source/blender/gpu
	../../../source/kernel/gen_system
	../../../intern/string
	../../../intern/moto/include
	../../../intern/guardedalloc
	../../../extern/glew/include
)

IF(WITH_FFMPEG)
	SET(INC ${INC} ${FFMPEG_INC} ${PTHREADS_INC})
	ADD_DEFINITIONS(-DWITH_FFMPEG)
	ADD_DEFINITIONS(-D__STDC_CONSTANT_MACROS)
ENDIF(WITH_FFMPEG)

IF(WITH_PYTHON)
	SET(INC ${INC} ${PYTHON_INC})
ELSE(WITH_PYTHON)
	ADD_DEFINITIONS(-DDISABLE_PYTHON)
ENDIF(WITH_PYTHON)

BLENDERLIB(bf_videotex "${SRC}" "${INC}")
#env.BlenderLib ( 'bf_videotex', sources, Split(incs), [], libtype=['game','player'], priority=[25, 72], compileflags = cflags )
