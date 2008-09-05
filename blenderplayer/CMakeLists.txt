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
# Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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

IF(WITH_QUICKTIME)
  ADD_DEFINITIONS(-DWITH_QUICKTIME)
ENDIF(WITH_QUICKTIME)

IF(LINUX)
  ADD_DEFINITIONS(-DWITH_BINRELOC)
  INCLUDE_DIRECTORIES(${BINRELOC_INC})
endif(LINUX)

ADD_CUSTOM_COMMAND(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/dna.c
  COMMAND ${CMAKE_BINARY_DIR}/bin/${CMAKE_CFG_INTDIR}/makesdna ${CMAKE_CURRENT_BINARY_DIR}/dna.c ${CMAKE_SOURCE_DIR}/source/blender/makesdna/
  DEPENDS ${CMAKE_BINARY_DIR}/bin/${CMAKE_CFG_INTDIR}/makesdna
)

IF(WIN32)
  ADD_EXECUTABLE(blenderplayer ${EXETYPE} ${CMAKE_CURRENT_BINARY_DIR}/dna.c ../source/icons/winblender.rc)
ELSE(WIN32)
  ADD_EXECUTABLE(blenderplayer ${CMAKE_CURRENT_BINARY_DIR}/dna.c)
ENDIF(WIN32)

ADD_DEPENDENCIES(blenderplayer makesdna)

FILE(READ ${CMAKE_BINARY_DIR}/cmake_blender_libs.txt BLENDER_LINK_LIBS)

SET(BLENDER_LINK_LIBS ${BLENDER_LINK_LIBS} gp_common gp_ghost blenkernel_blc)

IF(CMAKE_SYSTEM_NAME MATCHES "Linux")
  SET(BLENDER_LINK_LIBS ${BLENDER_LINK_LIBS} extern_binreloc)
ENDIF(CMAKE_SYSTEM_NAME MATCHES "Linux")

IF(UNIX)
  # Sort libraries
  SET(BLENDER_SORTED_LIBS
    gp_ghost 
    gp_common 
    bf_string 
    bf_ghost 
    bf_blenkernel 
    verse 
    bf_blenkernel 
    bf_decimation 
    bf_blenloader 
    bf_blenpluginapi 
    bf_blroutines 
    bf_converter 
    bf_sumo 
    bf_ketsji 
    extern_solid 
    extern_qhull 
    bf_bullet 
    bf_common 
    bf_dummy 
    bf_logic 
    bf_rasterizer 
    bf_oglrasterizer 
    bf_expressions 
    bf_scenegraph 
    bf_moto 
    bf_soundsystem 
    bf_kernel 
    bf_nodes
	bf_gpu
    bf_imbuf
    bf_avi 
    kx_network 
    bf_ngnetwork 
    bf_loopbacknetwork 
    extern_bullet 
    bf_guardedalloc 
    bf_memutil 
    bf_bmfont 
    bf_blenlib 
    bf_cineon 
    bf_openexr 
    bf_ftfont 
    extern_ftgl 
    bf_readblenfile 
    blenkernel_blc
    bf_quicktime
    extern_binreloc
	extern_glew
  ) 

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

SETUP_LIBLINKS(blenderplayer)
