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
# The Original Code is Copyright (C) 2016, Blender Foundation
# All rights reserved.
#
# The Original Code is: all of this file.
#
# Contributor(s): Blender Institute
#
# ***** END GPL LICENSE BLOCK *****

set(INC
	.
	intern
	nodes
	operations
	../blenkernel
	../blenlib
	../blentranslation
	../imbuf
	../makesdna
	../makesrna
	../windowmanager
	../nodes
	../nodes/composite
	../nodes/intern
	../render/extern/include
	../render/intern/include
	../../../extern/clew/include
	../../../intern/guardedalloc
	../../../intern/atomic
)

set(INC_SYS

)

set(SRC
	DRW_defines.h

)

list(APPEND INC
)

endif()

blender_add_lib(bf_draw "${SRC}" "${INC}" "${INC_SYS}")
