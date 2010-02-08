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
	../../../source/kernel/gen_system
	../../../intern/string
	../../../intern/guardedalloc
	../../../source/gameengine/Rasterizer/RAS_OpenGLRasterizer 
	../../../source/gameengine/Converter
	../../../source/blender/imbuf
	../../../intern/ghost/include
	../../../intern/moto/include
	../../../source/gameengine/Ketsji 
	../../../source/blender/blenlib
	../../../source/blender/blenkernel
	../../../source/blender/python
	../../../source/blender/python/generic
	../../../source/blender 
	../../../source/blender/include
	../../../source/blender/makesdna 
	../../../source/gameengine/Rasterizer 
	../../../source/gameengine/GameLogic
	../../../source/gameengine/Expressions 
	../../../source/gameengine/Ketsji/KXNetwork
	../../../source/gameengine/Network
	../../../source/gameengine/SceneGraph 
	../../../source/gameengine/Physics/common
	../../../source/gameengine/Physics/Bullet 
	../../../source/gameengine/Network/LoopBackNetwork
	../../../intern/audaspace/intern
	../../../source/blender/misc 
	../../../source/blender/blenloader
	../../../source/blender/gpu
	../../../extern/bullet2/src
	../../../extern/glew/include
)

ADD_DEFINITIONS(-DGLEW_STATIC)

IF(WITH_SDL)
	SET(INC ${INC} ${SDL_INCLUDE_DIR})  
ELSE(WITH_SDL)
	ADD_DEFINITIONS(-DDISABLE_SDL)
ENDIF(WITH_SDL)

IF(WITH_PYTHON)
	SET(INC ${INC} ${PYTHON_INC})
ELSE(WITH_PYTHON)
	ADD_DEFINITIONS(-DDISABLE_PYTHON)
ENDIF(WITH_PYTHON)

IF(WITH_FFMPEG)
	ADD_DEFINITIONS(-DWITH_FFMPEG)
ENDIF(WITH_FFMPEG)

BLENDERLIB(bf_ketsji "${SRC}" "${INC}")
#env.BlenderLib ( 'bf_ketsji', sources, Split(incs), [], libtype=['game','player'], priority=[25, 72], compileflags = cflags )
