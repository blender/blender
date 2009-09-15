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

SET(SRC 
	GPG_Application.cpp
	GPG_Canvas.cpp
	GPG_ghost.cpp
	GPG_KeyboardDevice.cpp
	GPG_System.cpp
)

SET(INC
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
	../../../../source/blender/blenkernel
	../../../../source/blender/readblenfile
	../../../../source/blender
	../../../../source/blender/include
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
	../../../../source/blender/misc
	../../../../source/blender/blenloader
	../../../../source/blender/gpu
	../../../../extern/glew/include
	${PYTHON_INC}
)

IF(WITH_FFMPEG)
	ADD_DEFINITIONS(-DWITH_FFMPEG)
ENDIF(WITH_FFMPEG)

BLENDERLIB_NOLIST(gp_ghost "${SRC}" "${INC}")
#env.BlenderLib (libname='gp_ghost', sources=source_files, includes = incs, defines = [], libtype='player',priority=0, compileflags=cflags)
