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
# Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# The Original Code is Copyright (C) 2006, Blender Foundation
# All rights reserved.
#
# The Original Code is: all of this file.
#
# Contributor(s): Jacques Beaurain
#                 Ben Batt
#
# ***** END GPL LICENSE BLOCK *****

FILE(GLOB SRC intern/*.c)

SET(INC 
	. ./intern
	../../../intern/guardedalloc
	../include
	../blenlib
	../makesdna
	../blenkernel
	../blenkernel/intern
	../render/extern/include
	../../../intern/decimation/extern
	../../../intern/elbeem/extern
	../../../intern/bsp/extern
	${ZLIB_INC}
)

IF(WITH_LZO)
	SET(INC ${INC} ../../../extern/lzo/minilzo)
	ADD_DEFINITIONS(-DWITH_LZO)
ENDIF(WITH_LZO)

IF(WITH_LZMA)
	SET(INC ${INC} ../../../extern/lzma)
	ADD_DEFINITIONS(-DWITH_LZMA)
ENDIF(WITH_LZMA)

IF(WITH_OPENMP)
		ADD_DEFINITIONS(-DPARALLEL=1)
ENDIF(WITH_OPENMP)

IF(NOT WITH_ELBEEM)
	ADD_DEFINITIONS(-DDISABLE_ELBEEM)
ENDIF(NOT WITH_ELBEEM)

BLENDERLIB(bf_modifiers "${SRC}" "${INC}")

