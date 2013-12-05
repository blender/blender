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
	../../imbuf
	../../bmesh
	../../makesdna
	../../makesrna
	../../render/extern/include
	../../windowmanager
	../../../../intern/guardedalloc
	../../gpu
)

set(INC_SYS
	${GLEW_INCLUDE_PATH}
)

set(SRC
	image_buttons.c
	image_draw.c
	image_edit.c
	image_ops.c
	space_image.c

	image_intern.h
)

if(WITH_INTERNATIONAL)
	add_definitions(-DWITH_INTERNATIONAL)
endif()

if(WITH_OPENIMAGEIO)
	add_definitions(-DWITH_OPENIMAGEIO)
endif()

if(WITH_IMAGE_OPENJPEG)
	add_definitions(-DWITH_OPENJPEG)
endif()

if(WITH_IMAGE_OPENEXR)
	add_definitions(-DWITH_OPENEXR)
endif()

if(WITH_IMAGE_TIFF)
	add_definitions(-DWITH_TIFF)
endif()

if(WITH_IMAGE_CINEON)
	add_definitions(-DWITH_CINEON)
endif()

blender_add_lib(bf_editor_space_image "${SRC}" "${INC}" "${INC_SYS}")
