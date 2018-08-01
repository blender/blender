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
# The Original Code is Copyright (C) 2018, Blender Foundation
# All rights reserved.
#
# ***** END GPL LICENSE BLOCK *****

set(INC
	.
	intern
	../blenkernel
	../blenlib
	../blenfont
	../depsgraph
	../makesdna
	../makesrna
	../bmesh
	../render/extern/include
	../../../intern/elbeem/extern
	../../../intern/guardedalloc
	../../../intern/eigen
)

set(INC_SYS
	${ZLIB_INCLUDE_DIRS}
)

set(SRC
	intern/MOD_gpencil_util.h

	intern/MOD_gpencil_util.c
	intern/MOD_gpencilnoise.c
	intern/MOD_gpencilsubdiv.c
	intern/MOD_gpencilsimplify.c
	intern/MOD_gpencilthick.c
	intern/MOD_gpenciltint.c
	intern/MOD_gpencilcolor.c
	intern/MOD_gpencilinstance.c
	intern/MOD_gpencilbuild.c
	intern/MOD_gpencilopacity.c
	intern/MOD_gpencillattice.c
	intern/MOD_gpencilmirror.c
	intern/MOD_gpencilsmooth.c
	intern/MOD_gpencilhook.c
	intern/MOD_gpenciloffset.c

	MOD_gpencil_modifiertypes.h
)

if(WITH_INTERNATIONAL)
	add_definitions(-DWITH_INTERNATIONAL)
endif()

add_definitions(${GL_DEFINITIONS})

blender_add_lib(bf_gpencil_modifiers "${SRC}" "${INC}" "${INC_SYS}")
