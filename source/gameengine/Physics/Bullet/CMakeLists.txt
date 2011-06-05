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

# since this includes bullet we get errors from the headers too
remove_strict_flags()

set(INC
	.
	../common
	../../../../extern/bullet2/src
	../../../../intern/moto/include
	../../../../intern/guardedalloc
	../../../../intern/container
	../../../../intern/string
	../../Rasterizer
	../../Ketsji
	../../Expressions
	../../GameLogic
	../../SceneGraph
	../../../../source/blender/makesdna
	../../../../source/blender/blenlib
	../../../../source/blender/blenkernel
)

set(INC_SYS
	${GLEW_INCLUDE_PATH}
	${PYTHON_INCLUDE_DIRS}
)

set(SRC
	CcdPhysicsEnvironment.cpp
	CcdPhysicsController.cpp
	CcdGraphicController.cpp

	CcdGraphicController.h
	CcdPhysicsController.h
	CcdPhysicsEnvironment.h
)

if(WITH_BULLET)
	add_definitions(-DUSE_BULLET)
endif()

blender_add_lib(ge_phys_bullet "${SRC}" "${INC}" "${INC_SYS}")
