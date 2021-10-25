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
	intern
	../memutil
)

set(INC_SYS
	${EIGEN3_INCLUDE_DIRS}
)

set(SRC
	intern/IK_QJacobian.cpp
	intern/IK_QJacobianSolver.cpp
	intern/IK_QSegment.cpp
	intern/IK_QTask.cpp
	intern/IK_Solver.cpp

	extern/IK_solver.h
	intern/IK_Math.h
	intern/IK_QJacobian.h
	intern/IK_QJacobianSolver.h
	intern/IK_QSegment.h
	intern/IK_QTask.h
)

blender_add_lib(bf_intern_iksolver "${SRC}" "${INC}" "${INC_SYS}")
