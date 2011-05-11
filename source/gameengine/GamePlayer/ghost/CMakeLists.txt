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

set(INC
	.
	../../../../intern/string
	../../../../intern/ghost
	../../../../intern/guardedalloc
	../../../../intern/container
	../../../../intern/moto/include
	../../../../source/gameengine/Rasterizer/RAS_OpenGLRasterizer
	../../../../source/gameengine/BlenderRoutines
	../../../../source/gameengine/Converter
	../../../../source/blender/imbuf
	../../../../source/gameengine/Ketsji
	../../../../source/blender/blenfont
	../../../../source/blender/blenlib
	../../../../source/blender/blenkernel
	../../../../source/blender
	../../../../source/blender/makesdna
	../../../../source/blender/makesrna
	../../../../source/gameengine/Rasterizer
	../../../../source/gameengine/GameLogic
	../../../../source/gameengine/Expressions
	../../../../source/gameengine/Network
	../../../../source/gameengine/SceneGraph
	../../../../source/gameengine/Physics/common
	../../../../source/gameengine/Network/LoopBackNetwork
	../../../../source/gameengine/GamePlayer/common
	../../../../source/blender/blenloader
	../../../../source/blender/gpu
	${GLEW_INCLUDE_PATH}
	${PYTHON_INCLUDE_DIRS}
)

set(SRC 
	GPG_Application.cpp
	GPG_Canvas.cpp
	GPG_ghost.cpp
	GPG_KeyboardDevice.cpp
	GPG_System.cpp

	GPG_Application.h
	GPG_Canvas.h
	GPG_KeyboardDevice.h
	GPG_System.h
)

add_definitions(-DGLEW_STATIC)

if(WITH_CODEC_FFMPEG)
	add_definitions(-DWITH_FFMPEG)
endif()

blender_add_lib_nolist(ge_player_ghost "${SRC}" "${INC}")
