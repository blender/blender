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

SETUP_LIBDIRS()

INCLUDE_DIRECTORIES(../../intern/guardedalloc
  ../blender/blenlib
  ../blender/blenkernel
  ../blender/nodes
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

IF(LINUX)
  ADD_DEFINITIONS(-DWITH_BINRELOC)
  INCLUDE_DIRECTORIES(${BINRELOC_INC})
endif(LINUX)

IF(YESIAMSTUPID)
  ADD_DEFINITIONS(-DYESIAMSTUPID)
ENDIF(YESIAMSTUPID)

MESSAGE(STATUS "Configuring blender")

ADD_CUSTOM_COMMAND(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/dna.c
  COMMAND ${CMAKE_BINARY_DIR}/bin/${CMAKE_CFG_INTDIR}/makesdna ${CMAKE_CURRENT_BINARY_DIR}/dna.c ${CMAKE_SOURCE_DIR}/source/blender/makesdna/
  DEPENDS ${CMAKE_BINARY_DIR}/bin/${CMAKE_CFG_INTDIR}/makesdna
)

IF(WIN32)
  ADD_EXECUTABLE(blender ${EXETYPE} creator.c ${CMAKE_CURRENT_BINARY_DIR}/dna.c ../icons/winblender.rc)
ELSE(WIN32)
  ADD_EXECUTABLE(blender ${EXETYPE} creator.c ${CMAKE_CURRENT_BINARY_DIR}/dna.c)
ENDIF(WIN32)


# Post build steps for bundling/packaging.

SET(TARGETDIR ${EXECUTABLE_OUTPUT_PATH}/${CMAKE_CFG_INTDIR})


IF(UNIX)
  ADD_CUSTOM_COMMAND(TARGET blender
    POST_BUILD
    MAIN_DEPENDENCY blender
    #COMMAND cp -R ${CMAKE_SOURCE_DIR}/release/plugins ${TARGETDIR}/
    #COMMAND cp -R ${CMAKE_SOURCE_DIR}/release/text/* ${TARGETDIR}/
  )
ENDIF(UNIX)

IF(UNIX AND NOT APPLE)
  ADD_CUSTOM_COMMAND(TARGET blender
    POST_BUILD
    MAIN_DEPENDENCY blender
    COMMAND rm -Rf ${TARGETDIR}/.blender
    COMMAND mkdir ${TARGETDIR}/.blender/
    COMMAND cp ${CMAKE_SOURCE_DIR}/bin/.blender/.bfont.ttf ${TARGETDIR}/.blender/
    COMMAND cp ${CMAKE_SOURCE_DIR}/bin/.blender/.Blanguages ${TARGETDIR}/.blender/
    COMMAND cp -R ${CMAKE_SOURCE_DIR}/bin/.blender/locale ${TARGETDIR}/.blender/
    COMMAND cp -R ${CMAKE_SOURCE_DIR}/release/scripts ${TARGETDIR}/.blender/
    COMMAND find ${TARGETDIR} -name CVS -prune -exec rm -rf {} "\;"
  )
ENDIF(UNIX AND NOT APPLE)

IF(APPLE)
  SET(SOURCEDIR ${CMAKE_SOURCE_DIR}/source/darwin/blender.app)
  SET(SOURCEINFO ${SOURCEDIR}/Contents/Info.plist)
  SET(TARGETINFO ${TARGETDIR}/blender.app/Contents/Info.plist)

  ADD_CUSTOM_COMMAND(TARGET blender
    POST_BUILD
    MAIN_DEPENDENCY blender
    COMMAND cp -R ${SOURCEINFO} ${TARGETDIR}/blender.app/Contents/
    COMMAND cp -R ${SOURCEDIR}/Contents/PkgInfo ${TARGETDIR}/blender.app/Contents/
    COMMAND cp -R ${SOURCEDIR}/Contents/Resources ${TARGETDIR}/blender.app/Contents/
    COMMAND cat ${SOURCEINFO} | sed s/VERSION/`cat ${CMAKE_SOURCE_DIR}/release/VERSION`/ | sed s/DATE/`date +'%Y-%b-%d'`/ > ${TARGETINFO}
    COMMAND rm -Rf ${TARGETDIR}/blender.app/Contents/MacOS/.blender
    COMMAND mkdir ${TARGETDIR}/blender.app/Contents/MacOS/.blender/
    COMMAND cp ${CMAKE_SOURCE_DIR}/bin/.blender/.bfont.ttf ${TARGETDIR}/blender.app/Contents/MacOS/.blender/
    COMMAND cp ${CMAKE_SOURCE_DIR}/bin/.blender/.Blanguages ${TARGETDIR}/blender.app/Contents/MacOS/.blender/
    COMMAND cp -R ${CMAKE_SOURCE_DIR}/bin/.blender/locale ${TARGETDIR}/blender.app/Contents/Resources/
    COMMAND cp -R ${CMAKE_SOURCE_DIR}/bin/.blender/locale ${TARGETDIR}/blender.app/Contents/MacOS/.blender/
    COMMAND cp ${CMAKE_SOURCE_DIR}/bin/.blender/.Blanguages ${TARGETDIR}/blender.app/Contents/Resources/
    COMMAND cp -R ${CMAKE_SOURCE_DIR}/release/scripts ${TARGETDIR}/blender.app/Contents/MacOS/.blender/
    COMMAND find ${TARGETDIR}/blender.app -name CVS -prune -exec rm -rf {} "\;"
    COMMAND find ${TARGETDIR}/blender.app -name CVS.sandboxinfo -prune -exec rm -rf {} "\;"
    COMMAND find ${TARGETDIR}/blender.app -name .DS_Store -prune -exec rm -rf {} "\;"
  )
ENDIF(APPLE)

IF(WIN32)
  FILE(TO_NATIVE_PATH ${CMAKE_SOURCE_DIR} WIN_SOURCE_DIR)
  
  ADD_CUSTOM_COMMAND(TARGET blender
    POST_BUILD
    MAIN_DEPENDENCY blender
    COMMAND if not exist \"$\(TargetDir\)\\.blender\" mkdir \"$\(TargetDir\)\\.blender\"
    COMMAND if not exist \"$\(TargetDir\)\\.blender\\locale\" mkdir \"$\(TargetDir\)\\.blender\\locale\"
    COMMAND if not exist \"$\(TargetDir\)\\.blender\\scripts\" mkdir \"$\(TargetDir\)\\.blender\\scripts\"
    COMMAND if not exist \"$\(TargetDir\)\\plugins\" mkdir \"$\(TargetDir\)\\plugins\"
    COMMAND copy /Y \"${WIN_SOURCE_DIR}\\bin\\.blender\\.Blanguages\" \"$\(TargetDir\)\\.blender\\\"
    COMMAND copy /Y \"${WIN_SOURCE_DIR}\\bin\\.blender\\.bfont.ttf\" \"$\(TargetDir\)\\.blender\\\"
    COMMAND xcopy /E /Y \"${WIN_SOURCE_DIR}\\bin\\.blender\\locale\\*.*\" \"$\(TargetDir\)\\.blender\\locale\"
    COMMAND xcopy /E /Y \"${WIN_SOURCE_DIR}\\release\\scripts\\*.*\" \"$\(TargetDir\)\\.blender\\scripts\"
    COMMAND xcopy /E /Y \"${WIN_SOURCE_DIR}\\release\\plugins\\*.*\" \"$\(TargetDir\)\\plugins\"
    COMMAND copy /Y \"${WIN_SOURCE_DIR}\\release\\text\\*.*\" \"$\(TargetDir\)\"
  )
  
  FILE(TO_NATIVE_PATH "${LIBDIR}" WIN_LIBDIR)
  
  ADD_CUSTOM_COMMAND(TARGET blender
    POST_BUILD
    MAIN_DEPENDENCY blender
    COMMAND copy /Y \"${WIN_LIBDIR}\\gettext\\lib\\gnu_gettext.dll\" \"$\(TargetDir\)\\\"
    COMMAND copy /Y \"${WIN_LIBDIR}\\png\\lib\\libpng.dll\" \"$\(TargetDir\)\\\"
    COMMAND copy /Y \"${WIN_LIBDIR}\\sdl\\lib\\SDL.dll\" \"$\(TargetDir\)\\\"
    COMMAND copy /Y \"${WIN_LIBDIR}\\zlib\\lib\\zlib.dll\" \"$\(TargetDir\)\\\"
    COMMAND copy /Y \"${WIN_LIBDIR}\\tiff\\lib\\libtiff.dll\" \"$\(TargetDir\)\\\"
    COMMAND if $\(ConfigurationName\)==Debug copy /Y \"${WIN_LIBDIR}\\python\\lib\\python25_D.dll\" \"$\(TargetDir\)\\\"
    COMMAND if $\(ConfigurationName\)==Debug copy /Y \"${WIN_LIBDIR}\\CRTL\\lib\\msvcrtd.dll\" \"$\(TargetDir\)\\\"
    COMMAND if NOT $\(ConfigurationName\)==Debug copy /Y \"${WIN_LIBDIR}\\python\\lib\\python25.dll\" \"$\(TargetDir\)\\\"
    COMMAND copy /Y \"${WIN_LIBDIR}\\pthreads\\lib\\pthreadVC2.dll\" \"$\(TargetDir\)\\\"
  )

  IF(WITH_INTERNATIONAL)
    ADD_CUSTOM_COMMAND(TARGET blender
      POST_BUILD
      MAIN_DEPENDENCY blender
      COMMAND copy /Y \"${WIN_LIBDIR}\\iconv\\lib\\iconv.dll\" \"$\(TargetDir\)\\\"
    )
  ENDIF(WITH_INTERNATIONAL)

  IF(WITH_FFMPEG)
    ADD_CUSTOM_COMMAND(TARGET blender
      POST_BUILD
      MAIN_DEPENDENCY blender
      COMMAND copy /Y \"${WIN_LIBDIR}\\ffmpeg\\lib\\avcodec-51.dll\" \"$\(TargetDir\)\\\"
      COMMAND copy /Y \"${WIN_LIBDIR}\\ffmpeg\\lib\\avformat-52.dll\" \"$\(TargetDir\)\\\"
      COMMAND copy /Y \"${WIN_LIBDIR}\\ffmpeg\\lib\\avdevice-52.dll\" \"$\(TargetDir\)\\\"
      COMMAND copy /Y \"${WIN_LIBDIR}\\ffmpeg\\lib\\avutil-49.dll\" \"$\(TargetDir\)\\\"
      COMMAND copy /Y \"${WIN_LIBDIR}\\ffmpeg\\lib\\libfaac-0.dll\" \"$\(TargetDir\)\\\"
      COMMAND copy /Y \"${WIN_LIBDIR}\\ffmpeg\\lib\\libfaad-0.dll\" \"$\(TargetDir\)\\\"
      COMMAND copy /Y \"${WIN_LIBDIR}\\ffmpeg\\lib\\libmp3lame-0.dll\" \"$\(TargetDir\)\\\"
      COMMAND copy /Y \"${WIN_LIBDIR}\\ffmpeg\\lib\\libx264-59.dll\" \"$\(TargetDir\)\\\"
      COMMAND copy /Y \"${WIN_LIBDIR}\\ffmpeg\\lib\\swscale-0.dll\" \"$\(TargetDir\)\\\"
      COMMAND copy /Y \"${WIN_LIBDIR}\\ffmpeg\\lib\\xvidcore.dll\" \"$\(TargetDir\)\\\"
    )
  ENDIF(WITH_FFMPEG)
ENDIF(WIN32)

ADD_DEPENDENCIES(blender makesdna)

FILE(READ ${CMAKE_BINARY_DIR}/cmake_blender_libs.txt BLENDER_LINK_LIBS)

SET(BLENDER_LINK_LIBS bf_nodes ${BLENDER_LINK_LIBS} src blender_python blender_render blender_radiosity blender_IK bf_elbeem)

IF(CMAKE_SYSTEM_NAME MATCHES "Linux")
  SET(BLENDER_LINK_LIBS ${BLENDER_LINK_LIBS} extern_binreloc)
ENDIF(CMAKE_SYSTEM_NAME MATCHES "Linux")

IF(UNIX)
  # Sort libraries
  SET(BLENDER_SORTED_LIBS
    src 
    bf_decimation 
    blender_BSP 
    bf_soundsystem 
    bf_ghost 
    bf_string 
    blender_render 
    blender_radiosity 
    blender_ONL 
    blender_python 
    bf_blenkernel 
    bf_nodes
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
    bf_quicktime
    extern_binreloc
  )

  FOREACH(SORTLIB ${BLENDER_SORTED_LIBS})
    SET(REMLIB ${SORTLIB})
    FOREACH(SEARCHLIB ${BLENDER_LINK_LIBS})
      IF(${SEARCHLIB} STREQUAL ${SORTLIB})
        SET(REMLIB "")
      ENDIF(${SEARCHLIB} STREQUAL ${SORTLIB})
    ENDFOREACH(SEARCHLIB)
    IF(REMLIB)
      MESSAGE(STATUS "Removing library ${REMLIB} from blender linking because: not configured")
      LIST(REMOVE_ITEM BLENDER_SORTED_LIBS ${REMLIB})
    ENDIF(REMLIB)
  ENDFOREACH(SORTLIB)
  TARGET_LINK_LIBRARIES(blender ${BLENDER_SORTED_LIBS})
ELSE(UNIX)
  TARGET_LINK_LIBRARIES(blender ${BLENDER_LINK_LIBS})
ENDIF(UNIX)

SETUP_LIBLINKS(blender)
