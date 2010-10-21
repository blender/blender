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
	.
	../avi
	../blenlib
	../blenloader
	../gpu
	../ikplugin
	../imbuf
	../makesdna
	../makesrna
	../modifiers
	../nodes
	../editors/include
	../render/extern/include
	../../../extern/glew/include
	../../../intern/audaspace/intern
	../../../intern/bsp/extern ../blenfont
	../../../intern/decimation/extern
	../../../intern/elbeem/extern
	../../../intern/guardedalloc
	../../../intern/iksolver/extern
	../../../intern/memutil
	../../../intern/opennl/extern
	../../../intern/smoke/extern
    ../../../source/blender/windowmanager # XXX - BAD LEVEL CALL WM_api.h
	${ZLIB_INC}
)


ADD_DEFINITIONS(-DGLEW_STATIC)

IF(WITH_BULLET)
	LIST(APPEND INC ../../../extern/bullet2/src)
	ADD_DEFINITIONS(-DUSE_BULLET)
ENDIF(WITH_BULLET)

IF(WITH_IMAGE_OPENEXR)
	ADD_DEFINITIONS(-DWITH_OPENEXR)
ENDIF(WITH_IMAGE_OPENEXR)

IF(WITH_IMAGE_TIFF)
	ADD_DEFINITIONS(-DWITH_TIFF)
ENDIF(WITH_IMAGE_TIFF)

IF(WITH_IMAGE_OPENJPEG)
	ADD_DEFINITIONS(-DWITH_OPENJPEG)
ENDIF(WITH_IMAGE_OPENJPEG)

IF(WITH_IMAGE_DDS)
	ADD_DEFINITIONS(-DWITH_DDS)
ENDIF(WITH_IMAGE_DDS)

IF(WITH_IMAGE_CINEON)
	ADD_DEFINITIONS(-DWITH_CINEON)
ENDIF(WITH_IMAGE_CINEON)

IF(WITH_IMAGE_HDR)
	ADD_DEFINITIONS(-DWITH_HDR)
ENDIF(WITH_IMAGE_HDR)

IF(WITH_QUICKTIME)
	LIST(APPEND INC ../quicktime ${QUICKTIME_INC})
	ADD_DEFINITIONS(-DWITH_QUICKTIME)
ENDIF(WITH_QUICKTIME)

IF(WITH_FFMPEG)
	LIST(APPEND INC ${FFMPEG_INC})
	ADD_DEFINITIONS(-DWITH_FFMPEG)
ENDIF(WITH_FFMPEG)

IF(WITH_LCMS)
	LIST(APPEND INC ${LCMS_INCLUDE_DIR})
	ADD_DEFINITIONS(-DWITH_LCMS)
ENDIF(WITH_LCMS)

IF(WITH_PYTHON)
	LIST(APPEND INC ../python ${PYTHON_INC})
ELSE(WITH_PYTHON)
	ADD_DEFINITIONS(-DDISABLE_PYTHON)
ENDIF(WITH_PYTHON)

IF(WITH_OPENMP)
	ADD_DEFINITIONS(-DPARALLEL=1)
ENDIF(WITH_OPENMP)

IF(NOT WITH_FLUID)
	ADD_DEFINITIONS(-DDISABLE_ELBEEM)
ENDIF(NOT WITH_FLUID)

IF(WITH_JACK)
	ADD_DEFINITIONS(-DWITH_JACK)
ENDIF(WITH_JACK)

IF(WITH_LZO)
	LIST(APPEND INC ../../../extern/lzo/minilzo)
	ADD_DEFINITIONS(-DWITH_LZO)
ENDIF(WITH_LZO)

IF(WITH_LZMA)
	LIST(APPEND INC ../../../extern/lzma)
	ADD_DEFINITIONS(-DWITH_LZMA)
ENDIF(WITH_LZMA)

IF(WIN32)
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /WX")
	LIST(APPEND INC ${PTHREADS_INC})
ENDIF(WIN32)

BLENDERLIB(bf_blenkernel "${SRC}" "${INC}")
