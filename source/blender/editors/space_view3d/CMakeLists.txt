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
# The Original Code is Copyright (C) 2006, Blender Foundation
# All rights reserved.
#
# The Original Code is: all of this file.
#
# Contributor(s): Jacques Beaurain.
#
# ***** END GPL LICENSE BLOCK *****

FILE(GLOB SRC *.c)

SET(INC
	../../windowmanager
	../../editors/include
	../../editors/interface
	../../../../intern/guardedalloc
	../../../../intern/memutil
	../../blenlib ../../makesdna
	../../makesrna ../../blenkernel
	../../include ../../imbuf
	../../render/extern/include
	../../../../intern/bsp/extern
	../../../../intern/decimation/extern
	../../blenloader
	../../python
	../../../kernel/gen_system
	../../readstreamglue
	../../../../intern/elbeem/extern
	../../../../intern/ghost
	../../../../intern/opennl/extern
	../../../../extern/glew/include
	../../../../intern/smoke/extern
	../../../../intern/audaspace/intern
	../../nodes
	../../gpu
	../../blenfont
	../../ikplugin
)

ADD_DEFINITIONS(-DGLEW_STATIC)

IF(WITH_GAMEENGINE)
	ADD_DEFINITIONS(-DGAMEBLENDER)
ENDIF(WITH_GAMEENGINE)

IF(WITH_INTERNATIONAL)
	ADD_DEFINITIONS(-DINTERNATIONAL)
ENDIF(WITH_INTERNATIONAL)

IF(WITH_OPENEXR)
	ADD_DEFINITIONS(-DWITH_OPENEXR)
ENDIF(WITH_OPENEXR)

IF(WITH_TIFF)
	ADD_DEFINITIONS(-DWITH_TIFF)
ENDIF(WITH_TIFF)

IF(WITH_OPENJPEG)
	ADD_DEFINITIONS(-DWITH_OPENJPEG)
ENDIF(WITH_OPENJPEG)

IF(WITH_QUICKTIME)
	SET(INC ${INC} ../quicktime ${QUICKTIME_INC})
	ADD_DEFINITIONS(-DWITH_QUICKTIME)
ENDIF(WITH_QUICKTIME)

IF(WITH_FFMPEG)
	SET(INC ${INC} ${FFMPEG_INC})
	ADD_DEFINITIONS(-DWITH_FFMPEG)
ENDIF(WITH_FFMPEG)

IF(WITH_OPENMP)
		ADD_DEFINITIONS(-DPARALLEL=1)
ENDIF(WITH_OPENMP)

IF(NOT WITH_ELBEEM)
	ADD_DEFINITIONS(-DDISABLE_ELBEEM)
ENDIF(NOT WITH_ELBEEM)

IF(WITH_PYTHON)
	SET(INC ${INC} ${PYTHON_INC})
ELSE(WITH_PYTHON)
	ADD_DEFINITIONS(-DDISABLE_PYTHON)
ENDIF(WITH_PYTHON)

IF(WIN32)
	SET(INC ${INC} ${PTHREADS_INC})
ENDIF(WIN32)

IF(WITH_BUILDINFO)
	ADD_DEFINITIONS(-DNAN_BUILDINFO)
ENDIF(WITH_BUILDINFO)

BLENDERLIB(bf_editor_space_view3d "${SRC}" "${INC}")
