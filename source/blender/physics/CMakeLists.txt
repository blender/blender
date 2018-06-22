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
# The Original Code is Copyright (C) 2014, Blender Foundation
# All rights reserved.
#
# The Original Code is: all of this file.
#
# Contributor(s): Lukas Toenne
#
# ***** END GPL LICENSE BLOCK *****

set(INC
	.
	intern
	../blenlib
	../blenkernel
	../depsgraph
	../imbuf
	../makesdna
	../../../intern/guardedalloc
)

set(INC_SYS
	${EIGEN3_INCLUDE_DIRS}
)

set(SRC
	intern/BPH_mass_spring.cpp
	intern/ConstrainedConjugateGradient.h
	intern/hair_volume.cpp
	intern/implicit.h
	intern/implicit_blender.c
	intern/implicit_eigen.cpp
	intern/eigen_utils.h

	BPH_mass_spring.h
)

blender_add_lib(bf_physics "${SRC}" "${INC}" "${INC_SYS}")
