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
	../blenkernel
	../blenlib
	../makesdna
	../../../intern/guardedalloc
)

set(INC_SYS

)

set(SRC
	intern/ikplugin_api.c

	BIK_api.h
	intern/ikplugin_api.h
)

if(WITH_IK_SOLVER)
	add_definitions(-DWITH_IK_SOLVER)
	list(APPEND INC
		../../../intern/iksolver/extern
	)
	list(APPEND SRC
		intern/iksolver_plugin.c
		intern/iksolver_plugin.h
	)
endif()

if(WITH_IK_ITASC)
	add_definitions(-DWITH_IK_ITASC)
	list(APPEND INC
		../../../intern/itasc
	)
	list(APPEND INC_SYS
		../../../extern/Eigen3
	)
	list(APPEND SRC
		intern/itasc_plugin.cpp
		intern/itasc_plugin.h
	)
endif()

blender_add_lib(bf_ikplugin "${SRC}" "${INC}" "${INC_SYS}")
