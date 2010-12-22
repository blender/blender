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
	../../makesdna
	../../blenkernel
	../../../../intern/guardedalloc
	../../../../extern/glew/include
	${PYTHON_INC}
)

set(SRC
	IDProp.c
	bgl.c
	blf_py_api.c
	bpy_internal_import.c
	mathutils.c
	mathutils_color.c
	mathutils_euler.c
	mathutils_geometry.c
	mathutils_matrix.c
	mathutils_quat.c
	mathutils_vector.c
	noise.c
	py_capi_utils.c

	IDProp.h
	bgl.h
	blf_py_api.h
	bpy_internal_import.h
	mathutils.h
	mathutils_color.h
	mathutils_euler.h
	mathutils_geometry.h
	mathutils_matrix.h
	mathutils_quat.h
	mathutils_vector.h
	py_capi_utils.h
)

blender_add_lib(bf_python_ext "${SRC}" "${INC}")
