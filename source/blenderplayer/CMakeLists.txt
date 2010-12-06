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

MESSAGE(STATUS "Configuring blenderplayer")

SETUP_LIBDIRS()

IF(WITH_CODEC_QUICKTIME)
	ADD_DEFINITIONS(-DWITH_QUICKTIME)
ENDIF(WITH_CODEC_QUICKTIME)

IF(CMAKE_SYSTEM_NAME MATCHES "Linux")
	ADD_DEFINITIONS(-DWITH_BINRELOC)
	INCLUDE_DIRECTORIES(${BINRELOC_INC})
ENDIF(CMAKE_SYSTEM_NAME MATCHES "Linux")

ADD_CUSTOM_COMMAND(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/dna.c
	COMMAND ${CMAKE_BINARY_DIR}/bin/${CMAKE_CFG_INTDIR}/makesdna ${CMAKE_CURRENT_BINARY_DIR}/dna.c ${CMAKE_SOURCE_DIR}/source/blender/makesdna/
	DEPENDS ${CMAKE_BINARY_DIR}/bin/${CMAKE_CFG_INTDIR}/makesdna
)

IF(WIN32 AND NOT UNIX)
	ADD_EXECUTABLE(blenderplayer ${EXETYPE} ${CMAKE_CURRENT_BINARY_DIR}/dna.c ../icons/winblender.rc)
ELSE(WIN32 AND NOT UNIX)
	ADD_EXECUTABLE(blenderplayer ${CMAKE_CURRENT_BINARY_DIR}/dna.c)
ENDIF(WIN32 AND NOT UNIX)

ADD_DEPENDENCIES(blenderplayer makesdna)

GET_PROPERTY(BLENDER_LINK_LIBS GLOBAL PROPERTY BLENDER_LINK_LIBS)

LIST(APPEND BLENDER_LINK_LIBS
	ge_player_common
	ge_player_ghost
	blenkernel_blc
)

IF(CMAKE_SYSTEM_NAME MATCHES "Linux")
	LIST(APPEND BLENDER_LINK_LIBS extern_binreloc)
ENDIF(CMAKE_SYSTEM_NAME MATCHES "Linux")

IF(UNIX)
	# Sort libraries
	SET(BLENDER_SORTED_LIBS
		ge_player_ghost 
		ge_player_common 
		bf_intern_string 
		bf_intern_ghost
		bf_rna
		bf_blenkernel
		bf_blenloader 
		bf_blenpluginapi 
		ge_blen_routines
		bf_editor_datafiles
		ge_converter 
		ge_logic_ketsji 
		ge_phys_bullet 
		ge_phys_dummy
		ge_phys_common 
		ge_logic 
		ge_rasterizer 
		ge_oglrasterizer 
		ge_logic_expressions 
		ge_scenegraph
		bf_ikplugin
		bf_intern_itasc
		bf_intern_ik
		bf_intern_smoke
		bf_modifiers
		bf_intern_moto 
		bf_gen_system 
		bf_nodes
		bf_gpu
		bf_imbuf
		bf_avi 
		ge_logic_network 
		ge_logic_ngnetwork 
		ge_logic_loopbacknetwork 
		extern_bullet 
		bf_intern_guardedalloc 
		bf_intern_memutil 
		bf_python
		bf_python_ext
		bf_blenlib 
		bf_imbuf_cineon
		bf_imbuf_openexr 
		extern_openjpeg 
		bf_imbuf_dds
		bf_readblenfile 
		bf_dna
		ge_videotex
		bf_blenfont
		bf_intern_audaspace
		blenkernel_blc
		extern_binreloc
		extern_glew
		extern_minilzo
		bf_intern_ghost # duplicate for linking
		bf_blenkernel # duplicate for linking
	)
  
	IF(WITH_CODEC_QUICKTIME)
		LIST(APPEND BLENDER_SORTED_LIBS bf_quicktime)
	ENDIF(WITH_CODEC_QUICKTIME)

	IF(WITH_CXX_GUARDEDALLOC)
		LIST(APPEND BLENDER_SORTED_LIBS bf_intern_guardedalloc_cpp)
	ENDIF(WITH_CXX_GUARDEDALLOC)

	FOREACH(SORTLIB ${BLENDER_SORTED_LIBS})
		SET(REMLIB ${SORTLIB})
		FOREACH(SEARCHLIB ${BLENDER_LINK_LIBS})
			IF(${SEARCHLIB} STREQUAL ${SORTLIB})
				SET(REMLIB "")
			ENDIF(${SEARCHLIB} STREQUAL ${SORTLIB})
		ENDFOREACH(SEARCHLIB)
		IF(REMLIB)
			MESSAGE(STATUS "Removing library ${REMLIB} from blenderplayer linking because: not configured")
			LIST(REMOVE_ITEM BLENDER_SORTED_LIBS ${REMLIB})
		ENDIF(REMLIB)
	ENDFOREACH(SORTLIB)

	TARGET_LINK_LIBRARIES(blenderplayer ${BLENDER_SORTED_LIBS})
ELSE(UNIX)
	TARGET_LINK_LIBRARIES(blenderplayer ${BLENDER_LINK_LIBS})
ENDIF(UNIX)

IF(WITH_PLAYER)
	ADD_SUBDIRECTORY(bad_level_call_stubs)
ENDIF(WITH_PLAYER)

SETUP_LIBLINKS(blenderplayer)
