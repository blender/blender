# $Id$
# ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version. The Blender
# Foundation also sells licenses for use in proprietary software under
# the Blender License.  See http://www.blender.org/BL/ for information
# about this.
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
# ***** END GPL/BL DUAL LICENSE BLOCK *****

SETUP_LIBDIRS()

FILE(APPEND ../source/blender/makesdna/intern/dna.c "")

INCLUDE_DIRECTORIES(../../intern/guardedalloc
  ../blender/blenlib
  ../blender/blenkernel
  ../blender/include
  ../blender/blenloader
  ../blender/imbuf
  ../blender/renderconverter
  ../blender/render/extern/include
  ../blender/python
  ../blender/makesdna
  ../kernel/gen_messaging
  ../kernel/gen_system
)

IF(WITH_QUICKTIME)
  ADD_DEFINITIONS(-DWITH_QUICKTIME)
ENDIF(WITH_QUICKTIME)

IF(WIN32)
  ADD_EXECUTABLE(blender ${EXETYPE} creator.c ../blender/makesdna/intern/dna.c ../icons/winblender.rc)
ELSE(WIN32)
  ADD_EXECUTABLE(blender ${EXETYPE} creator.c ../blender/makesdna/intern/dna.c)
ENDIF(WIN32)

ADD_DEPENDENCIES(blender makesdna)

SETUP_LIBLINKS(blender)
FILE(READ ${CMAKE_SOURCE_DIR}/cmake_blender_libs.txt BLENDER_LINK_LIBS)

SET(BLENDER_LINK_LIBS ${BLENDER_LINK_LIBS} src blender_python blender_render blender_radiosity blender_IK bf_elbeem)

IF(UNIX)
  # Sort libraries
  SET(BLENDER_SORTED_LIBS
    src 
    bf_decimation 
    blender_BSP 
    bf_soundsystem 
    bf_ghost 
    bf_string 
    blender_img 
    blender_render 
    blender_radiosity 
    blender_ONL 
    blender_python 
    bf_blenkernel 
    bf_blenloader 
    bf_blenpluginapi 
    bf_imbuf 
    bf_blenlib 
    bf_avi 
    bf_cineon 
    bf_openexr 
    bf_readblenfile 
    blender_bop 
    bf_kernel 
    bf_decimation 
    bf_elbeem 
    bf_yafray 
    blender_IK 
    bf_memutil 
    bf_guardedalloc 
    blender_CTR 
    bf_moto 
    bf_bmfont 
    verse 
    src 
    bf_ftfont 
    extern_ftgl 
    bf_blroutines 
    bf_converter 
    bf_dummy 
    bf_bullet 
    bf_common 
    bf_ketsji 
    bf_logic 
    bf_rasterizer 
    bf_oglrasterizer 
    bf_expressions 
    bf_scenegraph 
    bf_moto 
    bf_blroutines 
    kx_network 
    bf_kernel 
    bf_ngnetwork 
    extern_bullet 
    bf_loopbacknetwork 
    bf_sumo 
    bf_common 
    extern_solid 
    extern_qhull 
    bf_moto 
    blender_python
  )

  FOREACH(SORTLIB ${BLENDER_SORTED_LIBS})
    SET(REMLIB ${SORTLIB})
    FOREACH(SEARCHLIB ${BLENDER_LINK_LIBS})
      IF(${SEARCHLIB} STREQUAL ${SORTLIB})
        SET(REMLIB "")
      ENDIF(${SEARCHLIB} STREQUAL ${SORTLIB})
    ENDFOREACH(SEARCHLIB)
    IF(REMLIB)
      MESSAGE("Removing library ${REMLIB} from blender linking because: not configured")
      LIST(REMOVE_ITEM BLENDER_SORTED_LIBS ${REMLIB})
    ENDIF(REMLIB)
  ENDFOREACH(SORTLIB)
  TARGET_LINK_LIBRARIES(blender ${BLENDER_SORTED_LIBS})
ELSE(UNIX)
  TARGET_LINK_LIBRARIES(blender ${BLENDER_LINK_LIBS})
ENDIF(UNIX)

MESSAGE(STATUS "Configuring blender")
