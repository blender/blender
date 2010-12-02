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

SET(INC
	../include
	../../blenfont
	../../blenkernel
	../../blenlib
	../../imbuf
	../../makesdna
	../../makesrna
	../../windowmanager
	../../render/extern/include
	../../../../intern/guardedalloc
)

SET(SRC
	image_buttons.c
	image_draw.c
	image_header.c
	image_ops.c
	image_render.c
	space_image.c

	image_intern.h
)

IF(WITH_IMAGE_OPENJPEG)
	ADD_DEFINITIONS(-DWITH_OPENJPEG)
ENDIF(WITH_IMAGE_OPENJPEG)

IF(WITH_IMAGE_OPENEXR)
	ADD_DEFINITIONS(-DWITH_OPENEXR)
ENDIF(WITH_IMAGE_OPENEXR)

IF(WITH_IMAGE_TIFF)
	ADD_DEFINITIONS(-DWITH_TIFF)
ENDIF(WITH_IMAGE_TIFF)

IF(WITH_IMAGE_CINEON)
	ADD_DEFINITIONS(-DWITH_CINEON)
ENDIF(WITH_IMAGE_CINEON)

IF(WITH_LCMS)
	SET(INC ${INC} ${LCMS_INCLUDE_DIR})
	ADD_DEFINITIONS(-DWITH_LCMS)
ENDIF(WITH_LCMS)

BLENDERLIB(bf_editor_space_image "${SRC}" "${INC}")
