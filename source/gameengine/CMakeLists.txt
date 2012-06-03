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

# there are too many inter-includes so best define here
if(WITH_PYTHON)
	blender_include_dirs("${PYTHON_INCLUDE_DIRS}")
	add_definitions(-DWITH_PYTHON)
endif()

add_subdirectory(BlenderRoutines)
add_subdirectory(Converter)
add_subdirectory(Expressions)
add_subdirectory(GameLogic)
add_subdirectory(Ketsji)
add_subdirectory(Ketsji/KXNetwork)
add_subdirectory(Network)
add_subdirectory(Network/LoopBackNetwork)
add_subdirectory(Physics/common)
add_subdirectory(Physics/Dummy)
add_subdirectory(Rasterizer)
add_subdirectory(Rasterizer/RAS_OpenGLRasterizer)
add_subdirectory(SceneGraph)

if(WITH_BULLET)
	add_subdirectory(Physics/Bullet)
endif()

if(WITH_PYTHON)
	add_subdirectory(VideoTexture)
endif()

if(WITH_PLAYER)
	add_subdirectory(GamePlayer)
endif()
