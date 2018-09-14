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
	../../blenkernel
	../../blenlib
	../../gpu
	../../makesdna
	../../../../intern/guardedalloc
	../../../../intern/glew-mx
)

set(INC_SYS
	${GLEW_INCLUDE_PATH}
	${PYTHON_INCLUDE_DIRS}
)

set(SRC
	gpu_py_api.c
	gpu_py_batch.c
	gpu_py_matrix.c
	gpu_py_offscreen.c
	gpu_py_select.c
	gpu_py_shader.c
	gpu_py_types.c
	gpu_py_vertex_buffer.c
	gpu_py_vertex_format.c

	gpu_py_api.h
	gpu_py_batch.h
	gpu_py_matrix.h
	gpu_py_offscreen.h
	gpu_py_select.h
	gpu_py_shader.h
	gpu_py_types.h
	gpu_py_vertex_buffer.h
	gpu_py_vertex_format.h
)

add_definitions(${GL_DEFINITIONS})

blender_add_lib(bf_python_gpu "${SRC}" "${INC}" "${INC_SYS}")
