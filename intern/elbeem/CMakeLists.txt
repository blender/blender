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

SET(INC
	extern
	${PNG_INC}
	${ZLIB_INC}
)

SET(SRC
	intern/attributes.cpp
	intern/controlparticles.cpp
	intern/elbeem.cpp
	intern/elbeem_control.cpp
	intern/isosurface.cpp
	intern/mvmcoords.cpp
	intern/ntl_blenderdumper.cpp
	intern/ntl_bsptree.cpp
	intern/ntl_geometrymodel.cpp
	intern/ntl_geometryobject.cpp
	intern/ntl_lighting.cpp
	intern/ntl_ray.cpp
	intern/ntl_world.cpp
	intern/parametrizer.cpp
	intern/particletracer.cpp
	intern/simulation_object.cpp
	intern/solver_adap.cpp
	intern/solver_control.cpp
	intern/solver_init.cpp
	intern/solver_interface.cpp
	intern/solver_main.cpp
	intern/solver_util.cpp
	intern/utilities.cpp
)

ADD_DEFINITIONS(-DNOGUI -DELBEEM_BLENDER=1)
IF(WINDOWS)
		ADD_DEFINITIONS(-DUSE_MSVC6FIXES)
ENDIF(WINDOWS)

IF(WITH_OPENMP)
		ADD_DEFINITIONS(-DPARALLEL=1)
ENDIF(WITH_OPENMP)

BLENDERLIB_NOLIST(bf_intern_elbeem "${SRC}" "${INC}")
