# $Id: CMakeLists.txt 17433 2008-11-12 21:16:53Z blendix $
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

# Generated code has some unused vars we can ignore.
REMOVE_STRICT_FLAGS()

MESSAGE(STATUS "Configuring makesrna")

FILE(GLOB DEFSRC RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")
FILE(GLOB APISRC RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*_api.c")
LIST(REMOVE_ITEM DEFSRC rna_access.c rna_define.c makesrna.c)
LIST(REMOVE_ITEM DEFSRC ${APISRC})
 
STRING(REGEX REPLACE "rna_([a-zA-Z0-9_-]*).c" "${CMAKE_CURRENT_BINARY_DIR}/rna_\\1_gen.c" GENSRC "${DEFSRC}")
SET_SOURCE_FILES_PROPERTIES(GENSRC PROPERTIES GENERATED true)


SET(SRC
	makesrna.c
	rna_define.c
	${DEFSRC}
	${APISRC}
	../../../../intern/guardedalloc/intern/mallocn.c
	../../../../intern/guardedalloc/intern/mmap_win.c
)

INCLUDE_DIRECTORIES(
	../../../../intern/audaspace/intern
	../../../../intern/guardedalloc
	../../../../intern/memutil
	..
	../../makesdna
	../../blenkernel
	../../blenlib
	../../blenfont
	../../ikplugin
	../../windowmanager
	../../editors/include
	../../gpu
	../../imbuf
	../../render/extern/include
	../../../../extern/glew/include
	.
)

FILE(GLOB INC_FILES ../*.h ../../makesdna/*.h)

IF(WITH_PYTHON)
	ADD_DEFINITIONS(-DWITH_PYTHON)
ENDIF(WITH_PYTHON)

IF(WITH_GAMEENGINE)
	ADD_DEFINITIONS(-DWITH_GAMEENGINE)
ENDIF(WITH_GAMEENGINE)

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

IF(WITH_CODEC_QUICKTIME)
	LIST(APPEND INC ../../quicktime)
	ADD_DEFINITIONS(-DWITH_QUICKTIME)
ENDIF(WITH_CODEC_QUICKTIME)

IF(WITH_CODEC_FFMPEG)
	LIST(APPEND INC ${FFMPEG_INC})
	ADD_DEFINITIONS(-DWITH_FFMPEG)
ENDIF(WITH_CODEC_FFMPEG)

IF(WITH_LCMS)
	LIST(APPEND INC ${LCMS_INCLUDE_DIR})
	ADD_DEFINITIONS(-DWITH_LCMS)
ENDIF(WITH_LCMS)

IF(NOT WITH_MOD_FLUID)
	ADD_DEFINITIONS(-DDISABLE_ELBEEM)
ENDIF(NOT WITH_MOD_FLUID)

IF(WITH_FFTW3)
	ADD_DEFINITIONS(-DFFTW3=1)
ENDIF(WITH_FFTW3)

IF(WITH_SDL)
	ADD_DEFINITIONS(-DWITH_SDL)
ENDIF(WITH_SDL)

IF(WITH_OPENAL)
	ADD_DEFINITIONS(-DWITH_OPENAL)
ENDIF(WITH_OPENAL)

IF(WITH_JACK)
	ADD_DEFINITIONS(-DWITH_JACK)
ENDIF(WITH_JACK)

IF(WITH_OPENCOLLADA)
	ADD_DEFINITIONS(-DWITH_COLLADA)
ENDIF(WITH_OPENCOLLADA)

# Build makesrna executable
ADD_EXECUTABLE(makesrna ${SRC} ${INC_FILES})
TARGET_LINK_LIBRARIES(makesrna bf_dna)

# Output rna_*_gen.c
# note (linux only): with crashes try add this after COMMAND: valgrind --leak-check=full --track-origins=yes
ADD_CUSTOM_COMMAND(
	OUTPUT ${GENSRC}
	COMMAND ${CMAKE_BINARY_DIR}/bin/${CMAKE_CFG_INTDIR}/makesrna ${CMAKE_CURRENT_BINARY_DIR}/
	DEPENDS makesrna
)

# Build bf_rna
SET(SRC
	rna_access.c
	${GENSRC}

	../RNA_access.h
	../RNA_define.h
	../RNA_enum_types.h
	../RNA_types.h

	rna_internal.h
	rna_internal_types.h
	rna_nodetree_types.h
)

BLENDERLIB(bf_rna "${SRC}" "${INC}")
