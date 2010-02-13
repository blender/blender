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

FILE(GLOB SRC *.cpp)

SET(INC
	.
	../../../source/kernel/gen_system
	../../../intern/string
	../../../intern/guardedalloc
	../../../source/gameengine/Rasterizer/RAS_OpenGLRasterizer
	../../../intern/audaspace/intern
	../../../source/gameengine/Converter
	../../../source/gameengine/BlenderRoutines
	../../../source/blender/imbuf
	../../../intern/moto/include
	../../../source/gameengine/Ketsji
	../../../source/gameengine/Ketsji/KXNetwork
	../../../source/blender/blenlib
	../../../source/blender/blenkernel
	../../../source/blender/windowmanager
	../../../source/blender
	../../../source/blender/include
	../../../source/blender/makesdna
	../../../source/blender/makesrna
	../../../source/gameengine/Rasterizer
	../../../source/gameengine/Rasterizer/RAS_OpenGLRasterizer
	../../../source/gameengine/GameLogic
	../../../source/gameengine/Expressions
	../../../source/gameengine/Network
	../../../source/gameengine/SceneGraph
	../../../source/gameengine/Physics/common
	../../../source/gameengine/Physics/Bullet
	../../../source/gameengine/Physics/Dummy
	../../../source/gameengine/Network/LoopBackNetwork
	../../../source/blender/misc
	../../../source/blender/blenloader
	../../../source/blender/gpu
	../../../source/blender/ikplugin
	../../../extern/bullet2/src
)

IF(WITH_PYTHON)
	SET(INC ${INC} ${PYTHON_INC})
ELSE(WITH_PYTHON)
	ADD_DEFINITIONS(-DDISABLE_PYTHON)
ENDIF(WITH_PYTHON)

BLENDERLIB(bf_converter "${SRC}" "${INC}")
#env.BlenderLib ( 'bf_converter', sources, Split(incs), [], libtype=['game','player'], priority=[5,70] )
