# -*- mode: cmake; indent-tabs-mode: t; -*-
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
remove_strict_flags()

# message(STATUS "Configuring makesrna")

file(GLOB DEFSRC RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")
file(GLOB APISRC RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*_api.c")
list(REMOVE_ITEM DEFSRC rna_access.c rna_define.c makesrna.c)
list(REMOVE_ITEM DEFSRC ${APISRC})
 
string(REGEX REPLACE "rna_([a-zA-Z0-9_-]*).c" "${CMAKE_CURRENT_BINARY_DIR}/rna_\\1_gen.c" GENSRC "${DEFSRC}")
set_source_files_properties(GENSRC PROPERTIES GENERATED true)

set(SRC_RNA_INC
	../RNA_access.h
	../RNA_define.h
	../RNA_documentation.h
	../RNA_enum_types.h
	../RNA_types.h
)

set(SRC
	makesrna.c
	rna_define.c
	${DEFSRC}
	${APISRC}
	../../../../intern/guardedalloc/intern/mallocn.c
	../../../../intern/guardedalloc/intern/mmap_win.c
)

set(INC
)

if(WITH_PYTHON)
	add_definitions(-DWITH_PYTHON)
endif()

if(WITH_GAMEENGINE)
	add_definitions(-DWITH_GAMEENGINE)
endif()

if(WITH_IMAGE_OPENEXR)
	add_definitions(-DWITH_OPENEXR)
endif()

if(WITH_IMAGE_TIFF)
	add_definitions(-DWITH_TIFF)
endif()

if(WITH_IMAGE_OPENJPEG)
	add_definitions(-DWITH_OPENJPEG)
endif()

if(WITH_IMAGE_DDS)
	add_definitions(-DWITH_DDS)
endif()

if(WITH_IMAGE_CINEON)
	add_definitions(-DWITH_CINEON)
endif()

if(WITH_IMAGE_HDR)
	add_definitions(-DWITH_HDR)
endif()

if(WITH_CODEC_QUICKTIME)
	list(APPEND INC ../../quicktime)
	add_definitions(-DWITH_QUICKTIME)
endif()

if(WITH_CODEC_FFMPEG)
	list(APPEND INC ${FFMPEG_INC})
	add_definitions(-DWITH_FFMPEG)
endif()

if(WITH_LCMS)
	list(APPEND INC ${LCMS_INCLUDE_DIR})
	add_definitions(-DWITH_LCMS)
endif()

if(NOT WITH_MOD_FLUID)
	add_definitions(-DDISABLE_ELBEEM)
endif()

if(WITH_FFTW3)
	add_definitions(-DFFTW3=1)
endif()

if(WITH_SDL)
	add_definitions(-DWITH_SDL)
endif()

if(WITH_OPENAL)
	add_definitions(-DWITH_OPENAL)
endif()

if(WITH_JACK)
	add_definitions(-DWITH_JACK)
endif()

if(WITH_OPENCOLLADA)
	add_definitions(-DWITH_COLLADA)
endif()

# Build makesrna executable
blender_include_dirs(
	.
	..
	../../blenkernel
	../../blenloader
	../../blenlib
	../../blenfont
	../../gpu
	../../imbuf
	../../ikplugin
	../../makesdna
	../../windowmanager
	../../editors/include
	../../render/extern/include
	../../../../intern/audaspace/intern
	../../../../intern/guardedalloc
	../../../../intern/memutil
    ${GLEW_INCLUDE_PATH}
)

add_executable(makesrna ${SRC} ${SRC_RNA_INC} ${SRC_DNA_INC})
target_link_libraries(makesrna bf_dna)

# Output rna_*_gen.c
# note (linux only): with crashes try add this after COMMAND: valgrind --leak-check=full --track-origins=yes
add_custom_command(
	OUTPUT ${GENSRC}
	COMMAND ${CMAKE_BINARY_DIR}/bin/${CMAKE_CFG_INTDIR}/makesrna ${CMAKE_CURRENT_BINARY_DIR}/
	DEPENDS makesrna
)

# Build bf_rna
set(SRC
	rna_access.c
	${GENSRC}

	${SRC_RNA_INC}
	rna_internal.h
	rna_internal_types.h
	rna_nodetree_types.h
)

blender_add_lib(bf_rna "${SRC}" "${INC}")
