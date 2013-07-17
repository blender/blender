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
	../moto/include
)

set(SRC
	intern/IK_QJacobian.cpp
	intern/IK_QJacobianSolver.cpp
	intern/IK_QSegment.cpp
	intern/IK_QTask.cpp
	intern/IK_Solver.cpp
	intern/MT_ExpMap.cpp

	extern/IK_solver.h
	intern/IK_QJacobian.h
	intern/IK_QJacobianSolver.h
	intern/IK_QSegment.h
	intern/IK_QTask.h
	intern/MT_ExpMap.h
	intern/TNT/cholesky.h
	intern/TNT/cmat.h
	intern/TNT/fcscmat.h
	intern/TNT/fmat.h
	intern/TNT/fortran.h
	intern/TNT/fspvec.h
	intern/TNT/index.h
	intern/TNT/lapack.h
	intern/TNT/lu.h
	intern/TNT/qr.h
	intern/TNT/region1d.h
	intern/TNT/region2d.h
	intern/TNT/stopwatch.h
	intern/TNT/subscript.h
	intern/TNT/svd.h
	intern/TNT/tnt.h
	intern/TNT/tntmath.h
	intern/TNT/tntreqs.h
	intern/TNT/transv.h
	intern/TNT/triang.h
	intern/TNT/trisolve.h
	intern/TNT/vec.h
	intern/TNT/vecadaptor.h
	intern/TNT/version.h
)

blender_add_lib(bf_intern_iksolver "${SRC}" "${INC}" "${INC_SYS}")
