# $Id$
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

FILE(GLOB SRC intern/*.c)

SET(INC 
	. ../../../intern/guardedalloc ../../../intern/memutil ../editors/include ../blenlib ../makesdna ../modifiers
	../render/extern/include ../../../intern/decimation/extern
	../imbuf ../avi ../../../intern/elbeem/extern ../../../intern/opennl/extern
	../../../intern/iksolver/extern ../blenloader ../ikplugin
	../nodes ../../../extern/glew/include ../gpu ../makesrna ../../../intern/smoke/extern
	../../../intern/bsp/extern ../blenfont
	../../../intern/audaspace/intern
    ../../../source/blender/windowmanager # XXX - BAD LEVEL CALL WM_api.h
	${ZLIB_INC}
)


ADD_DEFINITIONS(-DGLEW_STATIC)

IF(WITH_BULLET)
	SET(INC ${INC} ../../../extern/bullet2/src)
	ADD_DEFINITIONS(-DUSE_BULLET)
ENDIF(WITH_BULLET)

IF(WITH_OPENEXR)
	ADD_DEFINITIONS(-DWITH_OPENEXR)
ENDIF(WITH_OPENEXR)

IF(WITH_OPENJPEG)
	ADD_DEFINITIONS(-DWITH_OPENJPEG)
ENDIF(WITH_OPENJPEG)

IF(WITH_DDS)
	ADD_DEFINITIONS(-DWITH_DDS)
ENDIF(WITH_DDS)

IF(WITH_QUICKTIME)
	SET(INC ${INC} ../quicktime ${QUICKTIME_INC})
	ADD_DEFINITIONS(-DWITH_QUICKTIME)
ENDIF(WITH_QUICKTIME)

IF(WITH_FFMPEG)
	SET(INC ${INC} ${FFMPEG_INC})
	ADD_DEFINITIONS(-DWITH_FFMPEG)
ENDIF(WITH_FFMPEG)

IF(WITH_PYTHON)
	SET(INC ${INC} ../python ${PYTHON_INC})
ELSE(WITH_PYTHON)
	ADD_DEFINITIONS(-DDISABLE_PYTHON)
ENDIF(WITH_PYTHON)

IF(WITH_OPENMP)
		ADD_DEFINITIONS(-DPARALLEL=1)
ENDIF(WITH_OPENMP)

IF(NOT WITH_ELBEEM)
	ADD_DEFINITIONS(-DDISABLE_ELBEEM)
ENDIF(NOT WITH_ELBEEM)

IF(WITH_LZO)
	SET(INC ${INC} ../../../extern/lzo/minilzo)
	ADD_DEFINITIONS(-DWITH_LZO)
ENDIF(WITH_LZO)

IF(WITH_LZMA)
	SET(INC ${INC} ../../../extern/lzma)
	ADD_DEFINITIONS(-DWITH_LZMA)
ENDIF(WITH_LZMA)

IF(WIN32)
	SET(INC ${INC} ${PTHREADS_INC})
ENDIF(WIN32)

BLENDERLIB(bf_blenkernel "${SRC}" "${INC}")

