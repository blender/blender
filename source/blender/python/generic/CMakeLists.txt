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

SET(INC 
	.
	../../blenlib
	../../makesdna
	../../blenkernel
	../../editors/include
	../../../../intern/guardedalloc
	../../../../extern/glew/include
	${PYTHON_INC}
)

SET(SRC
	IDProp.c
	bgl.c
	blf_api.c
	bpy_internal_import.c
	geometry.c
	mathutils.c
	mathutils_color.c
	mathutils_euler.c
	mathutils_matrix.c
	mathutils_quat.c
	mathutils_vector.c
	noise.c
	py_capi_utils.c
)

BLENDERLIB(bf_python_ext "${SRC}" "${INC}")
