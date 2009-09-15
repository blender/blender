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
	../blender/editors/include
	../blender/makesrna
	../blender/makesrna/intern
	../blender/nodes
	../blender/include
	../blender/blenloader
	../blender/imbuf
	../blender/renderconverter
	../blender/render/extern/include
	../blender/makesdna
	../blender/gpu
	../blender/windowmanager
	../kernel/gen_messaging
	../kernel/gen_system
	../../extern/glew/include
)



IF(WITH_QUICKTIME)
	ADD_DEFINITIONS(-DWITH_QUICKTIME)
ENDIF(WITH_QUICKTIME)

IF(WITH_OPENEXR)
	ADD_DEFINITIONS(-DWITH_OPENEXR)
ENDIF(WITH_OPENEXR)

IF(WITH_PYTHON)
	INCLUDE_DIRECTORIES(../blender/python)
ELSE(WITH_PYTHON)
	ADD_DEFINITIONS(-DDISABLE_PYTHON)
ENDIF(WITH_PYTHON)

IF(NOT WITH_SDL)
	ADD_DEFINITIONS(-DDISABLE_SDL)
ENDIF(NOT WITH_SDL)

IF(CMAKE_SYSTEM_NAME MATCHES "Linux")
	ADD_DEFINITIONS(-DWITH_BINRELOC)
	INCLUDE_DIRECTORIES(${BINRELOC_INC})
ENDIF(CMAKE_SYSTEM_NAME MATCHES "Linux")

MESSAGE(STATUS "Configuring blender")

IF(WIN32)
	ADD_EXECUTABLE(blender ${EXETYPE} creator.c ../icons/winblender.rc)
ELSE(WIN32)
	ADD_EXECUTABLE(blender ${EXETYPE} creator.c)
ENDIF(WIN32)


# Post build steps for bundling/packaging.

SET(TARGETDIR ${EXECUTABLE_OUTPUT_PATH}/${CMAKE_CFG_INTDIR})

IF(WITH_INSTALL)

	IF(UNIX)
		ADD_CUSTOM_COMMAND(TARGET blender
			POST_BUILD
			MAIN_DEPENDENCY blender
			#COMMAND cp -R ${CMAKE_SOURCE_DIR}/release/plugins ${TARGETDIR}/
			#COMMAND cp -R ${CMAKE_SOURCE_DIR}/release/text/* ${TARGETDIR}/
		)
	ENDIF(UNIX)

	IF(UNIX AND NOT APPLE)
		ADD_CUSTOM_COMMAND(
			TARGET blender POST_BUILD MAIN_DEPENDENCY blender
			COMMAND rm -Rf ${TARGETDIR}/.blender
			COMMAND mkdir ${TARGETDIR}/.blender/
			COMMAND cp ${CMAKE_SOURCE_DIR}/bin/.blender/.bfont.ttf ${TARGETDIR}/.blender/
		)
    
		IF(WITH_INTERNATIONAL)
			ADD_CUSTOM_COMMAND(
				TARGET blender POST_BUILD MAIN_DEPENDENCY blender
				COMMAND cp ${CMAKE_SOURCE_DIR}/bin/.blender/.Blanguages ${TARGETDIR}/.blender/
				COMMAND cp -R ${CMAKE_SOURCE_DIR}/bin/.blender/locale ${TARGETDIR}/.blender/
			)
		ENDIF(WITH_INTERNATIONAL)
    
		IF(WITH_PYTHON)
			ADD_CUSTOM_COMMAND(
				TARGET blender POST_BUILD MAIN_DEPENDENCY blender
				COMMENT "copying blender scripts..."
				COMMAND cp -R ${CMAKE_SOURCE_DIR}/release/ui ${TARGETDIR}/.blender/
				COMMAND cp -R ${CMAKE_SOURCE_DIR}/release/io ${TARGETDIR}/.blender/
				COMMAND find ${TARGETDIR} -name "*.py?" -prune -exec rm -rf {} "\;"
			)
      
			# Copy the systems python into the install directory
			# Scons copy in tools/Blender.py
			ADD_CUSTOM_COMMAND(
				TARGET blender POST_BUILD MAIN_DEPENDENCY blender
				COMMENT "copying a subset of the systems python..."
        
				COMMAND mkdir ${TARGETDIR}/.blender/python # PYTHONPATH and PYTHONHOME is set here
				COMMAND mkdir ${TARGETDIR}/.blender/python/lib/
				COMMAND cp -R ${PYTHON_LIBPATH}/python${PYTHON_VERSION} ${TARGETDIR}/.blender/python/lib/
        
				COMMAND rm -rf ${TARGETDIR}/.blender/python/lib/python${PYTHON_VERSION}/distutils
				COMMAND rm -rf ${TARGETDIR}/.blender/python/lib/python${PYTHON_VERSION}/lib2to3
				COMMAND rm -rf ${TARGETDIR}/.blender/python/lib/python${PYTHON_VERSION}/idlelib
				COMMAND rm -rf ${TARGETDIR}/.blender/python/lib/python${PYTHON_VERSION}/tkinter
				COMMAND rm -rf ${TARGETDIR}/.blender/python/lib/python${PYTHON_VERSION}/config
        
				COMMAND rm -rf ${TARGETDIR}/.blender/python/lib/python${PYTHON_VERSION}/site-packages
				COMMAND mkdir ${TARGETDIR}/.blender/python/lib/python${PYTHON_VERSION}/site-packages    # python needs it.

				COMMAND rm -f ${TARGETDIR}/.blender/python/lib/python${PYTHON_VERSION}/lib-dynload/_tkinter.so
				COMMAND find ${TARGETDIR}/.blender/python/lib/python${PYTHON_VERSION} -name "test" -prune -exec rm -rf {} "\;"
				COMMAND find ${TARGETDIR}/.blender/python/lib/python${PYTHON_VERSION} -name "*.py?" -exec rm -rf {} "\;"
				COMMAND find ${TARGETDIR}/.blender/python/lib/python${PYTHON_VERSION} -name "*.so"-exec strip -s {} "\;"
			)
     
		ENDIF(WITH_PYTHON)
      
		ADD_CUSTOM_COMMAND(
			TARGET blender POST_BUILD MAIN_DEPENDENCY blender
			COMMAND find ${TARGETDIR} -name .svn -prune -exec rm -rf {} "\;"
		)
    
	ENDIF(UNIX AND NOT APPLE)

	IF(APPLE)
		SET(SOURCEDIR ${CMAKE_SOURCE_DIR}/source/darwin/blender.app)
		SET(SOURCEINFO ${SOURCEDIR}/Contents/Info.plist)
		SET(TARGETINFO ${TARGETDIR}/blender.app/Contents/Info.plist)

		ADD_CUSTOM_COMMAND(
			TARGET blender POST_BUILD MAIN_DEPENDENCY blender
			COMMAND cp -R ${SOURCEINFO} ${TARGETDIR}/blender.app/Contents/
			COMMAND cp -R ${SOURCEDIR}/Contents/PkgInfo ${TARGETDIR}/blender.app/Contents/
			COMMAND cp -R ${SOURCEDIR}/Contents/Resources ${TARGETDIR}/blender.app/Contents/
			COMMAND cat ${SOURCEINFO} | sed s/VERSION/`cat ${CMAKE_SOURCE_DIR}/release/VERSION`/ | sed s/DATE/`date +'%Y-%b-%d'`/ > ${TARGETINFO}
			COMMAND rm -Rf ${TARGETDIR}/blender.app/Contents/MacOS/.blender
			COMMAND mkdir ${TARGETDIR}/blender.app/Contents/MacOS/.blender/
			COMMAND cp ${CMAKE_SOURCE_DIR}/bin/.blender/.bfont.ttf ${TARGETDIR}/blender.app/Contents/MacOS/.blender/
		)
      
		IF(WITH_INTERNATIONAL)
			ADD_CUSTOM_COMMAND(
				TARGET blender POST_BUILD MAIN_DEPENDENCY blender
				COMMAND cp ${CMAKE_SOURCE_DIR}/bin/.blender/.Blanguages ${TARGETDIR}/blender.app/Contents/MacOS/.blender/
				COMMAND cp -R ${CMAKE_SOURCE_DIR}/bin/.blender/locale ${TARGETDIR}/blender.app/Contents/Resources/
				COMMAND cp -R ${CMAKE_SOURCE_DIR}/bin/.blender/locale ${TARGETDIR}/blender.app/Contents/MacOS/.blender/
				COMMAND cp ${CMAKE_SOURCE_DIR}/bin/.blender/.Blanguages ${TARGETDIR}/blender.app/Contents/Resources/
			)
		ENDIF(WITH_INTERNATIONAL)
      
		IF(WITH_PYTHON)
			ADD_CUSTOM_COMMAND(
				TARGET blender POST_BUILD MAIN_DEPENDENCY blender
				COMMAND cp -Rf ${CMAKE_SOURCE_DIR}/release/ui ${TARGETDIR}/blender.app/Contents/MacOS/.blender/
				COMMAND cp -Rf ${CMAKE_SOURCE_DIR}/release/io ${TARGETDIR}/blender.app/Contents/MacOS/.blender/
				COMMAND mkdir ${TARGETDIR}/blender.app/Contents/MacOS/.blender/python/
				COMMAND unzip -q ${LIBDIR}/release/python.zip -d ${TARGETDIR}/blender.app/Contents/MacOS/.blender/python/
				COMMAND find ${TARGETDIR}/blender.app -name "*.py?" -prune -exec rm -rf {} "\;"
			)
		ENDIF(WITH_PYTHON)      
    
		ADD_CUSTOM_COMMAND(
			TARGET blender POST_BUILD MAIN_DEPENDENCY blender    
			COMMAND find ${TARGETDIR}/blender.app -name .DS_Store -prune -exec rm -rf {} "\;"
			COMMAND find ${TARGETDIR}/blender.app -name .svn -prune -exec rm -rf {} "\;"
		)
	ENDIF(APPLE)

	IF(WIN32)
		FILE(TO_NATIVE_PATH ${CMAKE_SOURCE_DIR} WIN_SOURCE_DIR)
    
		ADD_CUSTOM_COMMAND(TARGET blender
			POST_BUILD
			MAIN_DEPENDENCY blender
			COMMAND if not exist \"${TARGETDIR}\\.blender\" mkdir \"${TARGETDIR}\\.blender\"
			COMMAND if not exist \"${TARGETDIR}\\.blender\\locale\" mkdir \"${TARGETDIR}\\.blender\\locale\"
			COMMAND if not exist \"${TARGETDIR}\\.blender\\ui\" mkdir \"${TARGETDIR}\\.blender\\ui\"
			COMMAND if not exist \"${TARGETDIR}\\.blender\\io\" mkdir \"${TARGETDIR}\\.blender\\io\"
			COMMAND if not exist \"${TARGETDIR}\\plugins\" mkdir \"${TARGETDIR}\\plugins\"
			COMMAND copy /Y \"${WIN_SOURCE_DIR}\\bin\\.blender\\.Blanguages\" \"${TARGETDIR}\\.blender\\\" 
			COMMAND copy /Y \"${WIN_SOURCE_DIR}\\bin\\.blender\\.bfont.ttf\" \"${TARGETDIR}\\.blender\\\"
			COMMAND xcopy /E /Y \"${WIN_SOURCE_DIR}\\bin\\.blender\\locale\\*.*\" \"${TARGETDIR}\\.blender\\locale\"
			COMMAND xcopy /E /Y \"${WIN_SOURCE_DIR}\\release\\scripts\\*.*\" \"${TARGETDIR}\\.blender\\scripts\"
			COMMAND xcopy /E /Y \"${WIN_SOURCE_DIR}\\release\\ui\\*.*\" \"${TARGETDIR}\\.blender\\ui\"
			COMMAND xcopy /E /Y \"${WIN_SOURCE_DIR}\\release\\io\\*.*\" \"${TARGETDIR}\\.blender\\io\"
			COMMAND xcopy /E /Y \"${WIN_SOURCE_DIR}\\release\\plugins\\*.*\" \"${TARGETDIR}\\plugins\"
			COMMAND copy /Y \"${WIN_SOURCE_DIR}\\release\\text\\*.*\" \"${TARGETDIR}\"
			COMMAND copy /Y \"${WIN_SOURCE_DIR}\\release\\windows\\extra\\python26.zip\" \"${TARGETDIR}\\\"
		)
    
		FILE(TO_NATIVE_PATH "${LIBDIR}" WIN_LIBDIR)
    
		ADD_CUSTOM_COMMAND(TARGET blender
			POST_BUILD
			MAIN_DEPENDENCY blender
			COMMAND copy /Y \"${WIN_LIBDIR}\\release\\python31.zip\" \"${TARGETDIR}\\\"
			COMMAND copy /Y \"${WIN_LIBDIR}\\gettext\\lib\\gnu_gettext.dll\" \"${TARGETDIR}\\\"
			COMMAND copy /Y \"${WIN_LIBDIR}\\png\\lib\\libpng.dll\" \"${TARGETDIR}\\\"
			COMMAND copy /Y \"${WIN_LIBDIR}\\sdl\\lib\\SDL.dll\" \"${TARGETDIR}\\\"
			COMMAND copy /Y \"${WIN_LIBDIR}\\zlib\\lib\\zlib.dll\" \"${TARGETDIR}\\\"
			COMMAND copy /Y \"${WIN_LIBDIR}\\tiff\\lib\\libtiff.dll\" \"${TARGETDIR}\\\"
			COMMAND copy /Y \"${WIN_LIBDIR}\\python\\lib\\python31.dll\" \"${TARGETDIR}\\\"
			COMMAND copy /Y \"${WIN_LIBDIR}\\python\\lib\\python31_d.dll\" \"${TARGETDIR}\\\"
			COMMAND copy /Y \"${WIN_LIBDIR}\\pthreads\\lib\\pthreadVC2.dll\" \"${TARGETDIR}\\\"
			COMMAND copy /Y \"${WIN_LIBDIR}\\samplerate\\lib\\libsamplerate-0.dll\" \"${TARGETDIR}\\\"
		)

		IF(WITH_INTERNATIONAL)
			ADD_CUSTOM_COMMAND(TARGET blender
				POST_BUILD
				MAIN_DEPENDENCY blender
				COMMAND copy /Y \"${WIN_LIBDIR}\\iconv\\lib\\iconv.dll\" \"${TARGETDIR}\\\"
			)
		ENDIF(WITH_INTERNATIONAL)

		IF(WITH_FFMPEG)
			ADD_CUSTOM_COMMAND(TARGET blender
				POST_BUILD
				MAIN_DEPENDENCY blender
				COMMAND copy /Y \"${WIN_LIBDIR}\\ffmpeg\\lib\\avcodec-52.dll\" \"${TARGETDIR}\\\"
				COMMAND copy /Y \"${WIN_LIBDIR}\\ffmpeg\\lib\\avformat-52.dll\" \"${TARGETDIR}\\\"
				COMMAND copy /Y \"${WIN_LIBDIR}\\ffmpeg\\lib\\avdevice-52.dll\" \"${TARGETDIR}\\\"
				COMMAND copy /Y \"${WIN_LIBDIR}\\ffmpeg\\lib\\avutil-50.dll\" \"${TARGETDIR}\\\"
				COMMAND copy /Y \"${WIN_LIBDIR}\\ffmpeg\\lib\\libfaac-0.dll\" \"${TARGETDIR}\\\"
				COMMAND copy /Y \"${WIN_LIBDIR}\\ffmpeg\\lib\\libfaad-2.dll\" \"${TARGETDIR}\\\"
				COMMAND copy /Y \"${WIN_LIBDIR}\\ffmpeg\\lib\\libmp3lame-0.dll\" \"${TARGETDIR}\\\"
				COMMAND copy /Y \"${WIN_LIBDIR}\\ffmpeg\\lib\\libx264-67.dll\" \"${TARGETDIR}\\\"
				COMMAND copy /Y \"${WIN_LIBDIR}\\ffmpeg\\lib\\swscale-0.dll\" \"${TARGETDIR}\\\"
				COMMAND copy /Y \"${WIN_LIBDIR}\\ffmpeg\\lib\\xvidcore.dll\" \"${TARGETDIR}\\\"
			)
		ENDIF(WITH_FFMPEG)

		IF(WITH_SNDFILE)
			ADD_CUSTOM_COMMAND(TARGET blender
				POST_BUILD
				MAIN_DEPENDENCY blender
				COMMAND copy /Y \"${WIN_LIBDIR}\\sndfile\\lib\\libsndfile-1.dll\" \"${TARGETDIR}\\\"
			)
		ENDIF(WITH_SNDFILE)

		IF(WITH_JACK)
			ADD_CUSTOM_COMMAND(TARGET blender
				POST_BUILD
				MAIN_DEPENDENCY blender
				COMMAND copy /Y \"${WIN_LIBDIR}\\jack\\lib\\libjack.dll\" \"${TARGETDIR}\\\"
			)
		ENDIF(WITH_JACK)

		IF(WITH_OPENAL)
			ADD_CUSTOM_COMMAND(TARGET blender
				POST_BUILD
				MAIN_DEPENDENCY blender
				COMMAND copy /Y \"${WIN_LIBDIR}\\openal\\lib\\OpenAL32.dll\" \"${TARGETDIR}\\\"
				COMMAND copy /Y \"${WIN_LIBDIR}\\openal\\lib\\wrap_oal.dll\" \"${TARGETDIR}\\\"

			)
		ENDIF(WITH_OPENAL)


	ENDIF(WIN32)
ENDIF(WITH_INSTALL)

ADD_DEPENDENCIES(blender makesdna)

FILE(READ ${CMAKE_BINARY_DIR}/cmake_blender_libs.txt BLENDER_LINK_LIBS)

SET(BLENDER_LINK_LIBS bf_nodes ${BLENDER_LINK_LIBS} bf_windowmanager bf_editors blender_render)

IF(WITH_ELBEEM)
	SET(BLENDER_LINK_LIBS ${BLENDER_LINK_LIBS} bf_elbeem)
ENDIF(WITH_ELBEEM)

IF(CMAKE_SYSTEM_NAME MATCHES "Linux")
	SET(BLENDER_LINK_LIBS ${BLENDER_LINK_LIBS} extern_binreloc)
ENDIF(CMAKE_SYSTEM_NAME MATCHES "Linux")

IF(UNIX)
	# Sort libraries
	SET(BLENDER_SORTED_LIBS
		bf_windowmanager
		bf_editors
		bf_decimation 
		blender_BSP 
		bf_ghost 
		bf_string 
		blender_render 
		blender_ONL 
		bf_python 
		bf_gen_python 
		bf_blenkernel 
		bf_nodes
		bf_gpu
		bf_blenloader 
		bf_blenpluginapi 
		bf_imbuf 
		bf_blenlib 
		bf_avi 
		bf_cineon 
		bf_openexr 
		bf_dds
		bf_readblenfile 
		blender_bop 
		bf_kernel 
		bf_decimation 
		bf_elbeem 
		bf_IK 
		bf_memutil 
		bf_guardedalloc 
		blender_CTR 
		bf_moto 
		bf_windowmanager
		bf_editors
		bf_blroutines 
		bf_converter 
		bf_dummy 
		bf_bullet 
		bf_smoke
		bf_minilzo
		bf_lzma
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
		bf_common 
		bf_moto 
		bf_python
		bf_gen_python
		extern_binreloc
		extern_glew
		extern_libopenjpeg
		bf_videotex
		bf_rna
		bf_dna
		bf_blenfont
		bf_audaspace
	)

	IF(WITH_CXX_GUARDEDALLOC)
		SET(BLENDER_SORTED_LIBS ${BLENDER_SORTED_LIBS} bf_guardedalloc_cpp)
	ENDIF(WITH_CXX_GUARDEDALLOC)

	IF(WITH_QUICKTIME)
		SET(BLENDER_SORTED_LIBS ${BLENDER_SORTED_LIBS} bf_quicktime)
	ENDIF(WITH_QUICKTIME)


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
