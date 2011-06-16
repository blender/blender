# $Id: CMakeLists.txt 12931 2007-12-17 18:20:48Z theeth $
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
# Contributor(s): Jacques Beaurain.
#
# ***** END GPL LICENSE BLOCK *****

set(INC
	../include
	../../blenfont
	../../blenkernel
	../../blenlib
	../../blenloader
	../../gpu
	../../imbuf
	../../makesdna
	../../makesrna
	../../render/extern/include
	../../windowmanager
	../../../../intern/guardedalloc
)

set(INC_SYS
	${GLEW_INCLUDE_PATH}
)

set(SRC
	render_internal.c
	render_opengl.c
	render_ops.c
	render_preview.c
	render_shading.c
	render_update.c
	render_view.c

	render_intern.h
)

if(WITH_CODEC_QUICKTIME)
	list(APPEND INC ../../quicktime)
	list(APPEND INC_SYS ${QUICKTIME_INC})
	add_definitions(-DWITH_QUICKTIME)
endif()

if(WITH_OPENMP)
	add_definitions(-DPARALLEL=1)
endif()

blender_add_lib(bf_editor_render "${SRC}" "${INC}" "${INC_SYS}")
