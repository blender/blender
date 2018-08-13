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

# add_subdirectory(atomic)  # header only
add_subdirectory(clog)
add_subdirectory(string)
add_subdirectory(ghost)
add_subdirectory(guardedalloc)
add_subdirectory(libmv)
add_subdirectory(memutil)
add_subdirectory(opencolorio)
add_subdirectory(opensubdiv)
add_subdirectory(mikktspace)
add_subdirectory(glew-mx)
add_subdirectory(eigen)

if(WITH_AUDASPACE)
	add_subdirectory(audaspace)
endif()

if(WITH_MOD_REMESH)
	add_subdirectory(dualcon)
endif()

if(WITH_MOD_FLUID)
	add_subdirectory(elbeem)
endif()

if(WITH_MOD_SMOKE)
	add_subdirectory(smoke)
endif()

if(WITH_IK_SOLVER)
	add_subdirectory(iksolver)
endif()

if(WITH_IK_ITASC)
	add_subdirectory(itasc)
endif()

if(WITH_CYCLES)
	add_subdirectory(cycles)
endif()

if(WITH_INTERNATIONAL)
	add_subdirectory(locale)
endif()

if(WITH_BULLET)
	add_subdirectory(rigidbody)
endif()

# only windows needs utf16 converter
if(WIN32)
	add_subdirectory(utfconv)
endif()

if(WITH_OPENVDB)
	add_subdirectory(openvdb)
endif()
