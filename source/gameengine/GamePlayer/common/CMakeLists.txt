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
	../../../../intern/moto/include
	../../../../source/gameengine/Rasterizer/RAS_OpenGLRasterizer
	../../../../source/kernel/gen_system
	../../../../source/kernel/gen_messaging
	../../../../source/gameengine/Converter
	../../../../source/blender/imbuf
	../../../../source/gameengine/Ketsji
	../../../../source/blender/blenlib
	../../../../source/blender/blenfont
	../../../../source/blender/blenkernel
	../../../../source/blender
	../../../../source/blender/makesdna
	../../../../source/gameengine/Rasterizer
	../../../../source/gameengine/GameLogic
	../../../../source/gameengine/Expressions
	../../../../source/gameengine/Network
	../../../../source/gameengine/SceneGraph
	../../../../source/gameengine/Physics/common
	../../../../source/gameengine/Network/LoopBackNetwork
	../../../../source/gameengine/GamePlayer/ghost
	../../../../source/blender/blenloader
	../../../../source/blender/gpu
	${GLEW_INCLUDE_PATH}
	${PYTHON_INCLUDE_DIRS}
	${PNG_INCLUDE_DIR}
	${ZLIB_INCLUDE_DIRS}
)

set(SRC 
	bmfont.cpp
	GPC_Canvas.cpp
	GPC_Engine.cpp
	GPC_KeyboardDevice.cpp
	GPC_MouseDevice.cpp
	GPC_RawImage.cpp
	GPC_RawLoadDotBlendArray.cpp
	GPC_RawLogoArrays.cpp
	GPC_RenderTools.cpp
	GPC_System.cpp

	GPC_Canvas.h
	GPC_Engine.h
	GPC_KeyboardDevice.h
	GPC_MouseDevice.h
	GPC_RawImage.h
	GPC_RawLoadDotBlendArray.h
	GPC_RawLogoArrays.h
	GPC_RenderTools.h
	GPC_System.h
)

add_definitions(-DGLEW_STATIC)

blender_add_lib_nolist(ge_player_common "${SRC}" "${INC}")
