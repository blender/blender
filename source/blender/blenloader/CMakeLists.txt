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
	.
	../blenkernel
	../blenlib
	../makesdna
	../makesrna
	../nodes
	../render/extern/include
	../../../intern/guardedalloc
	../imbuf
)

set(INC_SYS
	${ZLIB_INCLUDE_DIRS}
)

set(SRC
	intern/readblenentry.c
	intern/readfile.c
	intern/runtime.c
	intern/undofile.c
	intern/writefile.c

	BLO_readfile.h
	BLO_runtime.h
	BLO_soundfile.h
	BLO_sys_types.h
	BLO_undofile.h
	BLO_writefile.h
	intern/readfile.h
)

if(WITH_BUILDINFO)
	add_definitions(-DWITH_BUILDINFO)
endif()

blender_add_lib(bf_blenloader "${SRC}" "${INC}" "${INC_SYS}")
