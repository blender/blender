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
	../../../../intern/string
	../../../../intern/container
	../../../../intern/moto/include
	../../../../source/gameengine/Rasterizer
	../../../../source/gameengine/Ketsji
	../../../../source/gameengine/SceneGraph
	../../../../source/gameengine/BlenderRoutines
	../../../../source/blender/gpu
	../../../../source/blender/makesdna
	../../../../source/blender/blenkernel
	../../../../source/blender/blenlib
	../../../../source/blender/blenloader
)

set(INC_SYS
	${GLEW_INCLUDE_PATH}
)

set(SRC
	RAS_GLExtensionManager.cpp
	RAS_ListRasterizer.cpp
	RAS_OpenGLRasterizer.cpp
	RAS_VAOpenGLRasterizer.cpp

	RAS_GLExtensionManager.h
	RAS_ListRasterizer.h
	RAS_OpenGLRasterizer.h
	RAS_VAOpenGLRasterizer.h
)

add_definitions(-DGLEW_STATIC)

blender_add_lib(ge_oglrasterizer "${SRC}" "${INC}" "${INC_SYS}")
