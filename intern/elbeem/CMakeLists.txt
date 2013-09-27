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
	extern
	../guardedalloc
)

set(INC_SYS
	${PNG_INCLUDE_DIR}
	${ZLIB_INCLUDE_DIRS}
)

set(SRC
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

	extern/LBM_fluidsim.h
	extern/elbeem.h
	intern/attributes.h
	intern/controlparticles.h
	intern/elbeem_control.h
	intern/isosurface.h
	intern/loop_tools.h
	intern/mcubes_tables.h
	intern/mvmcoords.h
	intern/ntl_blenderdumper.h
	intern/ntl_bsptree.h
	intern/ntl_geometryclass.h
	intern/ntl_geometrymodel.h
	intern/ntl_geometryobject.h
	intern/ntl_geometryshader.h
	intern/ntl_lighting.h
	intern/ntl_matrices.h
	intern/ntl_ray.h
	intern/ntl_vector3dim.h
	intern/ntl_world.h
	intern/paraloopend.h
	intern/parametrizer.h
	intern/particletracer.h
	intern/simulation_object.h
	intern/solver_class.h
	intern/solver_control.h
	intern/solver_interface.h
	intern/solver_relax.h
	intern/utilities.h
	intern/globals.h
)

# elbeem has some harmless UNUSED warnings
remove_strict_flags()

add_definitions(
	-DNOGUI
	-DELBEEM_BLENDER=1
)

# not essential but quiet gcc's -Wundef
add_definitions(
	-DLBM_PRECISION=1
	-DLBM_INCLUDE_TESTSOLVERS=0
	-DFSGR_STRICT_DEBUG=0
	-DELBEEM_MPI=0
	-DNEWDIRVELMOTEST=0
)

if(WITH_OPENMP)
	add_definitions(-DPARALLEL=1)
else()
	add_definitions(-DPARALLEL=0)
endif()

blender_add_lib_nolist(bf_intern_elbeem "${SRC}" "${INC}" "${INC_SYS}")
