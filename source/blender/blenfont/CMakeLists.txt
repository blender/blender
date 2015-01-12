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
# The Original Code is Copyright (C) 2008, Blender Foundation
# All rights reserved.
#
# The Original Code is: all of this file.
#
# ***** END GPL LICENSE BLOCK *****

set(INC
	.
	../blenkernel
	../blenlib
	../editors/include
	../gpu
	../makesdna
	../makesrna
	../python
	../imbuf
	../../../intern/guardedalloc
	../../../intern/glew-mx
	../../../intern/locale
)

set(INC_SYS
	${GLEW_INCLUDE_PATH}
	${FREETYPE_INCLUDE_DIRS}
)

set(SRC
	intern/blf.c
	intern/blf_dir.c
	intern/blf_font.c
	intern/blf_glyph.c
	intern/blf_lang.c
	intern/blf_translation.c
	intern/blf_util.c

	BLF_api.h
	BLF_translation.h
	intern/blf_internal.h
	intern/blf_internal_types.h
)

if(WIN32)
	list(APPEND SRC
		intern/blf_font_win32_compat.c
	)
endif()

if(WITH_INTERNATIONAL)
	add_definitions(-DWITH_INTERNATIONAL)
endif()

add_definitions(${GL_DEFINITIONS})

blender_add_lib(bf_blenfont "${SRC}" "${INC}" "${INC_SYS}")

