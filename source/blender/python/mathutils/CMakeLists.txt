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
# Contributor(s): Campbell Barton
#
# ***** END GPL LICENSE BLOCK *****

set(INC 
	.
	../../blenlib
	../../blenkernel
	../../makesdna
	../../../../intern/guardedalloc
)

set(INC_SYS
	${PYTHON_INCLUDE_DIRS}
)

set(SRC
	mathutils.c
	mathutils_Color.c
	mathutils_Euler.c
	mathutils_Matrix.c
	mathutils_Quaternion.c
	mathutils_Vector.c
	mathutils_geometry.c
	mathutils_noise.c

	mathutils.h
	mathutils_Color.h
	mathutils_Euler.h
	mathutils_Matrix.h
	mathutils_Quaternion.h
	mathutils_Vector.h
	mathutils_geometry.h
	mathutils_noise.h
)


blender_add_lib(bf_python_mathutils "${SRC}" "${INC}" "${INC_SYS}")
