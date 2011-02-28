# -*- mode: cmake; indent-tabs-mode: t; -*-
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

# So BUILDINFO and BLENDERPATH strings are automatically quoted
cmake_policy(SET CMP0005 NEW)

setup_libdirs()

blender_include_dirs(
	../../intern/guardedalloc
	../blender/blenlib
	../blender/blenkernel
	../blender/blenloader
	../blender/editors/include
	../blender/makesrna
	../blender/imbuf
	../blender/render/extern/include
	../blender/makesdna
	../blender/gpu
	../blender/windowmanager
)

if(WITH_CODEC_QUICKTIME)
	add_definitions(-DWITH_QUICKTIME)
endif()

if(WITH_IMAGE_OPENEXR)
	add_definitions(-DWITH_OPENEXR)
endif()

if(WITH_IMAGE_OPENJPEG)
	add_definitions(-DWITH_OPENJPEG)
endif()

if(WITH_IMAGE_TIFF)
	add_definitions(-DWITH_TIFF)
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

if(WITH_PYTHON)
	blender_include_dirs(../blender/python)
	add_definitions(-DWITH_PYTHON)
endif()

if(WITH_GAMEENGINE)
	blender_include_dirs(
		../kernel/gen_messaging
		../kernel/gen_system
	)

	add_definitions(-DWITH_GAMEENGINE)
endif()

if(NOT WITH_SDL)
	add_definitions(-DDISABLE_SDL)
endif()

if(WITH_BINRELOC)
	add_definitions(-DWITH_BINRELOC)
	blender_include_dirs(${BINRELOC_INC})
endif()

# Setup the exe sources and buildinfo
set(SRC
	creator.c
)

if(WIN32 AND NOT UNIX)
	list(APPEND SRC
		../icons/winblender.rc
	)
endif()

if(WITH_BUILDINFO)
	# --------------------------------------------------------------------------
	# These defines could all be moved into the header below
	string(REPLACE " " "\ " BUILDINFO_CFLAGS "${CMAKE_C_FLAGS}")
	string(REPLACE " " "\ " BUILDINFO_CXXFLAGS "${CMAKE_CXX_FLAGS}")
	string(REPLACE " " "\ " BUILDINFO_LINKFLAGS "${PLATFORM_LINKFLAGS}")
	add_definitions(
		# # define in header now, else these get out of date on rebuilds.
		# -DBUILD_DATE="${BUILD_DATE}"
		# -DBUILD_TIME="${BUILD_TIME}"
		# -DBUILD_REV="${BUILD_REV}"
		-DWITH_BUILDINFO_HEADER # alternative to lines above
		-DBUILD_PLATFORM="${CMAKE_SYSTEM_NAME}"
		-DBUILD_TYPE="${CMAKE_BUILD_TYPE}"
		-DBUILD_CFLAGS="${BUILDINFO_CFLAGS}"
		-DBUILD_CXXFLAGS="${BUILDINFO_CXXFLAGS}"
		-DBUILD_LINKFLAGS="${BUILDINFO_LINKFLAGS}"
		-DBUILD_SYSTEM="CMake"
	)

	# --------------------------------------------------------------------------
	# write header for values that change each build
	# note, generaed file is in build dir's source/creator
	#       except when used as an include path.

	# include the output directory, where the buildinfo.h file is generated
	include_directories(${CMAKE_BINARY_DIR}/source/creator)

	# a custom target that is always built
	add_custom_target(buildinfo ALL
		DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/buildinfo.h)

	# creates svnheader.h using cmake script
	add_custom_command(OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/buildinfo.h
		COMMAND ${CMAKE_COMMAND}
		-DSOURCE_DIR=${CMAKE_SOURCE_DIR}
		-P ${CMAKE_SOURCE_DIR}/build_files/cmake/buildinfo.cmake)

	# buildinfo.h is a generated file
	set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/buildinfo.h
		PROPERTIES GENERATED TRUE
		HEADER_FILE_ONLY TRUE)
		
	# add deps below, after adding blender
	# -------------- done with header values.

	list(APPEND SRC
		buildinfo.c
	)
endif()

# message(STATUS "Configuring blender")
if(WITH_PYTHON_MODULE)
	add_definitions(-DWITH_PYTHON_MODULE)

	# creates ./bin/bpy.so which can be imported as a python module.
	add_library(blender SHARED ${SRC})
	set_target_properties(
		blender
		PROPERTIES
			 PREFIX ""
			 OUTPUT_NAME bpy
			 LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/
	)
else()
	add_executable(blender ${EXETYPE} ${SRC})
endif()

if(WITH_BUILDINFO)
	# explicitly say that the executable depends on the buildinfo
	add_dependencies(blender buildinfo)
endif()

# Post build steps for bundling/packaging.

set(TARGETDIR ${EXECUTABLE_OUTPUT_PATH}/${CMAKE_CFG_INTDIR})

if(WITH_INSTALL)

	if(UNIX)
		add_custom_command(TARGET blender
			POST_BUILD
			MAIN_DEPENDENCY blender
			#COMMAND cp -R ${CMAKE_SOURCE_DIR}/release/plugins ${TARGETDIR}/
			#COMMAND cp -R ${CMAKE_SOURCE_DIR}/release/text/* ${TARGETDIR}/
		)
	endif()

	if(UNIX AND NOT APPLE)
		
		# Local installation, "make install" can be done after this optionally
		
		add_custom_command(
			TARGET blender POST_BUILD MAIN_DEPENDENCY blender
			COMMAND rm -Rf ${TARGETDIR}/${BLENDER_VERSION}
			COMMAND mkdir ${TARGETDIR}/${BLENDER_VERSION}/
			COMMAND cp ${CMAKE_SOURCE_DIR}/release/bin/.blender/.bfont.ttf ${TARGETDIR}/${BLENDER_VERSION}/
		)
	
		if(WITH_INTERNATIONAL)
			add_custom_command(
				TARGET blender POST_BUILD MAIN_DEPENDENCY blender
				COMMAND cp ${CMAKE_SOURCE_DIR}/release/bin/.blender/.Blanguages ${TARGETDIR}/${BLENDER_VERSION}/
				COMMAND cp -R ${CMAKE_SOURCE_DIR}/release/bin/.blender/locale ${TARGETDIR}/${BLENDER_VERSION}/
			)
		endif()
	
		if(WITH_PYTHON)
			add_custom_command(
				TARGET blender POST_BUILD MAIN_DEPENDENCY blender
				COMMENT "copying blender scripts..."
				COMMAND cp -R ${CMAKE_SOURCE_DIR}/release/scripts ${TARGETDIR}/${BLENDER_VERSION}/
				COMMAND find ${TARGETDIR} -name '*.py[co]' -prune -exec rm -rf {} '\;'
				COMMAND find ${TARGETDIR} -name '__pycache__' -exec rmdir {} '+'
			)
			if(WITH_PYTHON_INSTALL)
				# Copy the systems python into the install directory
				# Scons copy in tools/Blender.py
				add_custom_command(
					TARGET blender POST_BUILD MAIN_DEPENDENCY blender
					COMMENT "copying a subset of the systems python..."

					COMMAND mkdir ${TARGETDIR}/${BLENDER_VERSION}/python # PYTHONPATH and PYTHONHOME is set here
					COMMAND mkdir ${TARGETDIR}/${BLENDER_VERSION}/python/lib/
					COMMAND cp -R ${PYTHON_LIBPATH}/python${PYTHON_VERSION} ${TARGETDIR}/${BLENDER_VERSION}/python/lib/

					COMMAND rm -rf ${TARGETDIR}/${BLENDER_VERSION}/python/lib/python${PYTHON_VERSION}/distutils
					COMMAND rm -rf ${TARGETDIR}/${BLENDER_VERSION}/python/lib/python${PYTHON_VERSION}/lib2to3
					COMMAND rm -rf ${TARGETDIR}/${BLENDER_VERSION}/python/lib/python${PYTHON_VERSION}/idlelib
					COMMAND rm -rf ${TARGETDIR}/${BLENDER_VERSION}/python/lib/python${PYTHON_VERSION}/tkinter
					COMMAND rm -rf ${TARGETDIR}/${BLENDER_VERSION}/python/lib/python${PYTHON_VERSION}/config

					COMMAND rm -rf ${TARGETDIR}/${BLENDER_VERSION}/python/lib/python${PYTHON_VERSION}/site-packages
					COMMAND mkdir ${TARGETDIR}/${BLENDER_VERSION}/python/lib/python${PYTHON_VERSION}/site-packages	# python needs it.

					COMMAND rm -f ${TARGETDIR}/${BLENDER_VERSION}/python/lib/python${PYTHON_VERSION}/lib-dynload/_tkinter.so
					COMMAND find ${TARGETDIR}/${BLENDER_VERSION}/python/lib/python${PYTHON_VERSION} -type d -name 'test' -prune -exec rm -rf {} '\;'
					COMMAND find ${TARGETDIR}/${BLENDER_VERSION}/python/lib/python${PYTHON_VERSION} -type d -name 'turtledemo' -prune -exec rm -rf {} '\;'
					COMMAND find ${TARGETDIR}/${BLENDER_VERSION}/python/lib/python${PYTHON_VERSION} -type d -name 'config-*' -prune -exec rm -rf {} '\;'
					COMMAND find ${TARGETDIR}/${BLENDER_VERSION}/python/lib/python${PYTHON_VERSION} -name '*.py[co]' -exec rm -rf {} '\;'
					COMMAND find ${TARGETDIR}/${BLENDER_VERSION}/python/lib/python${PYTHON_VERSION} -name '__pycache__' -exec rmdir {} '+'
					COMMAND find ${TARGETDIR}/${BLENDER_VERSION}/python/lib/python${PYTHON_VERSION} -name '*.so' -exec strip -s {} '\;'
				)
			endif()
		endif()
	  
		add_custom_command(
			TARGET blender POST_BUILD MAIN_DEPENDENCY blender
			COMMAND find ${TARGETDIR} -name .svn -prune -exec rm -rf {} "\;"
		)
	
	
		# Above we bundle a portable distribution in ./bin
		# This is an optional "make install" which installs blender on the system.
		install(
			PROGRAMS ${TARGETDIR}/blender
			DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
		)

		if(WITH_GAMEENGINE AND WITH_PLAYER)
			install(
				PROGRAMS ${TARGETDIR}/blenderplayer
				DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
			)
		endif()

		install(
			FILES ${CMAKE_SOURCE_DIR}/release/freedesktop/blender.desktop
			DESTINATION ${CMAKE_INSTALL_PREFIX}/share/applications
		)
		install(
			FILES ${CMAKE_SOURCE_DIR}/release/freedesktop/icons/scalable/blender.svg
			DESTINATION ${CMAKE_INSTALL_PREFIX}/share/pixmaps
		)
		install(
			PROGRAMS ${CMAKE_SOURCE_DIR}/release/bin/blender-thumbnailer.py
			DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
		)
		install(
			FILES ${CMAKE_SOURCE_DIR}/doc/manpage/blender.1
			DESTINATION ${CMAKE_INSTALL_PREFIX}/share/man/man1
		)
		install(
			DIRECTORY ${CMAKE_SOURCE_DIR}/release/text/
			DESTINATION ${CMAKE_INSTALL_PREFIX}/share/doc/blender
			PATTERN ".svn" EXCLUDE
		)
		install(
			DIRECTORY ${CMAKE_SOURCE_DIR}/release/scripts/
			DESTINATION ${CMAKE_INSTALL_PREFIX}/share/blender/${BLENDER_VERSION}/scripts
			PATTERN ".svn" EXCLUDE
			PATTERN "*.pyc" EXCLUDE
			PATTERN "__pycache__" EXCLUDE
		)
		install(
			DIRECTORY ${CMAKE_SOURCE_DIR}/release/datafiles/brushicons/
			DESTINATION ${CMAKE_INSTALL_PREFIX}/share/blender/${BLENDER_VERSION}/datafiles/brushicons
			PATTERN ".svn" EXCLUDE
		)
		if(WITH_INTERNATIONAL)
			install(
				DIRECTORY ${CMAKE_SOURCE_DIR}/release/bin/.blender/locale/
				DESTINATION ${CMAKE_INSTALL_PREFIX}/share/blender/${BLENDER_VERSION}/datafiles/locale
				PATTERN ".svn" EXCLUDE
			)
		endif()
		
		# end "make install"
	
	elseif(WIN32)
		# notice 'xcopy /Y /H' on .bfont.ttf, this is needed when building over samba
		add_custom_command(TARGET blender
			POST_BUILD
			MAIN_DEPENDENCY blender
			COMMAND if not exist \"${TARGETDIR}\\${BLENDER_VERSION}\" mkdir \"${TARGETDIR}\\${BLENDER_VERSION}\"
			COMMAND if not exist \"${TARGETDIR}\\${BLENDER_VERSION}\\scripts\" mkdir \"${TARGETDIR}\\${BLENDER_VERSION}\\scripts\"
			COMMAND if not exist \"${TARGETDIR}\\${BLENDER_VERSION}\\config\" mkdir \"${TARGETDIR}\\${BLENDER_VERSION}\\config\"
			COMMAND if not exist \"${TARGETDIR}\\plugins\" mkdir \"${TARGETDIR}\\plugins\"
			COMMAND xcopy /Y /H \"${CMAKE_SOURCE_DIR}\\release\\bin\\.blender\\.bfont.ttf\" \"${TARGETDIR}\\${BLENDER_VERSION}\\config\\\"
			COMMAND xcopy /E /Y \"${CMAKE_SOURCE_DIR}\\release\\scripts\\*.*\" \"${TARGETDIR}\\${BLENDER_VERSION}\\scripts\\\"
			COMMAND xcopy /E /Y \"${CMAKE_SOURCE_DIR}\\release\\plugins\\*.*\" \"${TARGETDIR}\\${BLENDER_VERSION}\\plugins\\\"
			COMMAND copy /Y \"${CMAKE_SOURCE_DIR}\\release\\text\\*.*\" \"${TARGETDIR}\\\"
		)
	
		if(CMAKE_CL_64)
			# gettext and png are statically linked on win64
			add_custom_command(TARGET blender
				POST_BUILD
				MAIN_DEPENDENCY blender
				COMMAND copy /Y \"${LIBDIR}\\zlib\\lib\\zlib.dll\" \"${TARGETDIR}\\\"
			)
		else()
			add_custom_command(TARGET blender
				POST_BUILD
				MAIN_DEPENDENCY blender
				COMMAND copy /Y \"${LIBDIR}\\gettext\\lib\\gnu_gettext.dll\" \"${TARGETDIR}\\\"
				COMMAND copy /Y \"${LIBDIR}\\png\\lib\\libpng.dll\" \"${TARGETDIR}\\\"
				COMMAND copy /Y \"${LIBDIR}\\zlib\\lib\\zlib.dll\" \"${TARGETDIR}\\\"
			)
		endif()
		
		
		if(MSVC)
			add_custom_command(TARGET blender
				POST_BUILD
				MAIN_DEPENDENCY blender
				COMMAND copy /Y \"${LIBDIR}\\pthreads\\lib\\pthreadVC2.dll\" \"${TARGETDIR}\\\"
			)
		else()
			add_custom_command(TARGET blender
				POST_BUILD
				MAIN_DEPENDENCY blender
				COMMAND copy /Y \"${LIBDIR}\\pthreads\\lib\\pthreadGC2.dll\" \"${TARGETDIR}\\\"
			)
		endif()

		if(WITH_PYTHON)
			# note, as far as python is concerned 'RelWithDebInfo' is not debug since its without debug flags.
			if(NOT CMAKE_BUILD_TYPE) # hack: with multi-configuration generator this is "", so for now copy both python31.dll/zip and python31_d.dll/zip
				add_custom_command(TARGET blender
					POST_BUILD
					MAIN_DEPENDENCY blender
					COMMAND if not exist \"${TARGETDIR}\\${BLENDER_VERSION}\\python\\lib\" mkdir \"${TARGETDIR}\\${BLENDER_VERSION}\\python\\lib\"
					COMMAND if \"$(ConfigurationName)\" == \"\" copy /Y \"${LIBDIR}\\python\\lib\\python31.dll\" \"${TARGETDIR}\\\"
					COMMAND if \"$(ConfigurationName)\" == \"\" xcopy /E /Y \"${LIBDIR}\\release\\python31\\*.*\" \"${TARGETDIR}\\${BLENDER_VERSION}\\python\\lib\\\"
					COMMAND if \"$(ConfigurationName)\" == \"Debug\" copy /Y \"${LIBDIR}\\python\\lib\\python31_d.dll\" \"${TARGETDIR}\\\"
					COMMAND if \"$(ConfigurationName)\" == \"Debug\" xcopy /E /Y \"${LIBDIR}\\release\\python31_d\\*.*\" \"${TARGETDIR}\\${BLENDER_VERSION}\\python\\lib\\\"
					COMMAND if \"$(ConfigurationName)\" == \"RelWithDebInfo\" copy /Y \"${LIBDIR}\\python\\lib\\python31.dll\" \"${TARGETDIR}\\\"
					COMMAND if \"$(ConfigurationName)\" == \"RelWithDebInfo\" xcopy /E /Y \"${LIBDIR}\\release\\python31\\*.*\" \"${TARGETDIR}\\${BLENDER_VERSION}\\python\\lib\\\"
					COMMAND if \"$(ConfigurationName)\" == \"Release\" copy /Y \"${LIBDIR}\\python\\lib\\python31.dll\" \"${TARGETDIR}\\\"
					COMMAND if \"$(ConfigurationName)\" == \"Release\" xcopy /E /Y \"${LIBDIR}\\release\\python31\\*.*\" \"${TARGETDIR}\\${BLENDER_VERSION}\\python\\lib\\\"
					COMMAND if \"$(ConfigurationName)\" == \"MinSizeRel\" copy /Y \"${LIBDIR}\\python\\lib\\python31.dll\" \"${TARGETDIR}\\\"
					COMMAND if \"$(ConfigurationName)\" == \"MinSizeRel\" xcopy /E /Y \"${LIBDIR}\\release\\python31\\*.*\" \"${TARGETDIR}\\${BLENDER_VERSION}\\python\\lib\\\"
				)
			else()
				if(CMAKE_BUILD_TYPE STREQUAL Debug)
					add_custom_command(TARGET blender
						POST_BUILD
						MAIN_DEPENDENCY blender
						COMMAND copy /Y \"${LIBDIR}\\python\\lib\\python31_d.dll\" \"${TARGETDIR}\\\"
						COMMAND xcopy /E /Y \"${LIBDIR}\\release\\python31_d\\*.*\" \"${TARGETDIR}\\${BLENDER_VERSION}\\python\\lib\\\"
					)
				else()
					add_custom_command(TARGET blender
						POST_BUILD
						MAIN_DEPENDENCY blender
						COMMAND copy /Y \"${LIBDIR}\\python\\lib\\python31.dll\" \"${TARGETDIR}\\\"
						COMMAND xcopy /E /Y \"${LIBDIR}\\release\\python31\\*.*\" \"${TARGETDIR}\\${BLENDER_VERSION}\\python\\lib\\\"
					)
				endif()
			endif()
		endif()

		if(WITH_INTERNATIONAL)
			if(CMAKE_CL_64)
				# iconv is statically linked on win64
				add_custom_command(TARGET blender
					POST_BUILD
					MAIN_DEPENDENCY blender
					COMMAND if not exist \"${TARGETDIR}\\${BLENDER_VERSION}\\config\\locale\" mkdir \"${TARGETDIR}\\${BLENDER_VERSION}\\config\\locale\"
					COMMAND copy /Y \"${CMAKE_SOURCE_DIR}\\release\\bin\\.blender\\.Blanguages\" \"${TARGETDIR}\\${BLENDER_VERSION}\\config\\\" 
					COMMAND xcopy /E /Y \"${CMAKE_SOURCE_DIR}\\release\\bin\\.blender\\locale\\*.*\" \"${TARGETDIR}\\${BLENDER_VERSION}\\config\\locale\\\"
				)
			else()
				add_custom_command(TARGET blender
					POST_BUILD
					MAIN_DEPENDENCY blender
					COMMAND copy /Y \"${LIBDIR}\\iconv\\lib\\iconv.dll\" \"${TARGETDIR}\\\"
					COMMAND if not exist \"${TARGETDIR}\\${BLENDER_VERSION}\\config\\locale\" mkdir \"${TARGETDIR}\\${BLENDER_VERSION}\\config\\locale\"
					COMMAND copy /Y \"${CMAKE_SOURCE_DIR}\\release\\bin\\.blender\\.Blanguages\" \"${TARGETDIR}\\${BLENDER_VERSION}\\config\\\" 
					COMMAND xcopy /E /Y \"${CMAKE_SOURCE_DIR}\\release\\bin\\.blender\\locale\\*.*\" \"${TARGETDIR}\\${BLENDER_VERSION}\\config\\locale\\\"
				)
			endif()
		endif()

		if(WITH_CODEC_FFMPEG)
			add_custom_command(TARGET blender
				POST_BUILD
				MAIN_DEPENDENCY blender
				COMMAND copy /Y \"${LIBDIR}\\ffmpeg\\lib\\avcodec-52.dll\" \"${TARGETDIR}\\\"
				COMMAND copy /Y \"${LIBDIR}\\ffmpeg\\lib\\avformat-52.dll\" \"${TARGETDIR}\\\"
				COMMAND copy /Y \"${LIBDIR}\\ffmpeg\\lib\\avdevice-52.dll\" \"${TARGETDIR}\\\"
				COMMAND copy /Y \"${LIBDIR}\\ffmpeg\\lib\\avutil-50.dll\" \"${TARGETDIR}\\\"
				COMMAND copy /Y \"${LIBDIR}\\ffmpeg\\lib\\swscale-0.dll\" \"${TARGETDIR}\\\"
			)
		endif()

		if(WITH_CODEC_SNDFILE)
			add_custom_command(TARGET blender
				POST_BUILD
				MAIN_DEPENDENCY blender
				COMMAND copy /Y \"${LIBDIR}\\sndfile\\lib\\libsndfile-1.dll\" \"${TARGETDIR}\\\"
			)
		endif()

		if(WITH_OPENAL)
			add_custom_command(TARGET blender
				POST_BUILD
				MAIN_DEPENDENCY blender
				COMMAND copy /Y \"${LIBDIR}\\openal\\lib\\OpenAL32.dll\" \"${TARGETDIR}\\\"
				COMMAND copy /Y \"${LIBDIR}\\openal\\lib\\wrap_oal.dll\" \"${TARGETDIR}\\\"

			)
		endif()

		if(WITH_SDL)
			if(NOT CMAKE_CL_64)
				add_custom_command(TARGET blender
					POST_BUILD
					MAIN_DEPENDENCY blender
					COMMAND copy /Y \"${LIBDIR}\\sdl\\lib\\SDL.dll\" \"${TARGETDIR}\\\"
				)
			endif()
		endif()

	elseif(APPLE)
		set(SOURCEDIR ${CMAKE_SOURCE_DIR}/source/darwin/blender.app)
		set(SOURCEINFO ${SOURCEDIR}/Contents/Info.plist)
		set(TARGETINFO ${TARGETDIR}/blender.app/Contents/Info.plist)

		add_custom_command(
			TARGET blender POST_BUILD MAIN_DEPENDENCY blender
			COMMAND cp -Rf ${SOURCEINFO} ${TARGETDIR}/blender.app/Contents/
			COMMAND cp -Rf ${SOURCEDIR}/Contents/PkgInfo ${TARGETDIR}/blender.app/Contents/
			COMMAND cp -Rf ${SOURCEDIR}/Contents/Resources ${TARGETDIR}/blender.app/Contents/
			COMMAND cat ${SOURCEINFO} | sed s/VERSION/${BLENDER_VERSION}${BLENDER_VERSION_CHAR}/ | sed s/DATE/`date +'%Y-%b-%d'`/ > ${TARGETINFO}
			COMMAND rm -Rf ${TARGETDIR}/blender.app/Contents/MacOS/${BLENDER_VERSION}
			COMMAND mkdir ${TARGETDIR}/blender.app/Contents/MacOS/${BLENDER_VERSION}
			COMMAND mkdir ${TARGETDIR}/blender.app/Contents/MacOS/${BLENDER_VERSION}/datafiles/
			COMMAND cp ${CMAKE_SOURCE_DIR}/release/bin/.blender/.bfont.ttf ${TARGETDIR}/blender.app/Contents/MacOS/${BLENDER_VERSION}/datafiles/
		)
	  
		if(WITH_INTERNATIONAL)
			add_custom_command(
				TARGET blender POST_BUILD MAIN_DEPENDENCY blender
				COMMAND cp ${CMAKE_SOURCE_DIR}/release/bin/.blender/.Blanguages ${TARGETDIR}/blender.app/Contents/MacOS/${BLENDER_VERSION}/datafiles/
				COMMAND cp -Rf ${CMAKE_SOURCE_DIR}/release/bin/.blender/locale ${TARGETDIR}/blender.app/Contents/MacOS/${BLENDER_VERSION}/datafiles/
			)
		endif()
	  
		if(WITH_PYTHON)
			set(PYTHON_ZIP "python_${CMAKE_OSX_ARCHITECTURES}.zip")
			add_custom_command(
				TARGET blender POST_BUILD MAIN_DEPENDENCY blender
				COMMAND cp -Rf ${CMAKE_SOURCE_DIR}/release/scripts ${TARGETDIR}/blender.app/Contents/MacOS/${BLENDER_VERSION}/
				COMMAND mkdir ${TARGETDIR}/blender.app/Contents/MacOS/${BLENDER_VERSION}/python/
				COMMAND unzip -q ${LIBDIR}/release/${PYTHON_ZIP} -d ${TARGETDIR}/blender.app/Contents/MacOS/${BLENDER_VERSION}/python/
				COMMAND find ${TARGETDIR}/blender.app -name '*.py[co]' -prune -exec rm -rf {} ';'
				COMMAND find ${TARGETDIR}/blender.app -name '__pycache__' -exec rmdir {} '+'
			)
		endif()	  
	
		add_custom_command(
			TARGET blender POST_BUILD MAIN_DEPENDENCY blender	
			COMMAND find ${TARGETDIR}/blender.app -name .DS_Store -prune -exec rm -rf {} "\;"
			COMMAND find ${TARGETDIR}/blender.app -name .svn -prune -exec rm -rf {} "\;"
			COMMAND find ${TARGETDIR}/blender.app -name __MACOSX -prune -exec rm -rf {} "\;"
		)
	endif()

endif()

add_dependencies(blender makesdna)

get_property(BLENDER_LINK_LIBS GLOBAL PROPERTY BLENDER_LINK_LIBS)

set(BLENDER_LINK_LIBS bf_nodes ${BLENDER_LINK_LIBS} bf_windowmanager bf_render)

if(WITH_MOD_FLUID)
	list(APPEND BLENDER_LINK_LIBS bf_intern_elbeem)
endif()


#if(UNIX)
	# Sort libraries
	set(BLENDER_SORTED_LIBS
		bf_windowmanager

		bf_editor_space_api
		bf_editor_space_action
		bf_editor_space_buttons
		bf_editor_space_console
		bf_editor_space_file
		bf_editor_space_graph
		bf_editor_space_image
		bf_editor_space_info
		bf_editor_space_logic
		bf_editor_space_nla
		bf_editor_space_node
		bf_editor_space_outliner
		bf_editor_space_script
		bf_editor_space_sequencer
		bf_editor_space_sound
		bf_editor_space_time
		bf_editor_space_userpref
		bf_editor_space_view3d

		bf_editor_text
		bf_editor_transform
		bf_editor_util
		bf_editor_uvedit
		bf_editor_curve
		bf_editor_armature
		bf_editor_gpencil
		bf_editor_interface
		bf_editor_mesh
		bf_editor_metaball
		bf_editor_object
		bf_editor_physics
		bf_editor_render
		bf_editor_screen
		bf_editor_sculpt_paint
		bf_editor_sound
		bf_editor_animation
		bf_editor_datafiles

		bf_render 
		bf_intern_opennl 
		bf_python 
		bf_python_ext 
		bf_ikplugin
		bf_modifiers
		bf_blenkernel
		bf_nodes
		bf_gpu
		bf_blenloader
		bf_blenlib
		bf_intern_ghost
		bf_intern_string
		bf_blenpluginapi 
		bf_imbuf 
		bf_avi 
		bf_imbuf_cineon 
		bf_imbuf_openexr 
		bf_imbuf_dds
		bf_readblenfile
		bf_collada
		bf_intern_bsp 
		bf_intern_bop 
		bf_gen_system 
		bf_intern_decimate 
		bf_intern_elbeem 
		bf_intern_ik 
		bf_intern_memutil 
		bf_intern_guardedalloc 
		bf_intern_ctr 
		ge_blen_routines 
		ge_converter 
		ge_phys_dummy
		ge_phys_bullet 
		bf_intern_smoke
		extern_minilzo
		extern_lzma
		ge_logic_ketsji 
		ge_phys_common
		ge_logic 
		ge_rasterizer 
		ge_oglrasterizer 
		ge_logic_expressions 
		ge_scenegraph 
		ge_logic_network 
		bf_gen_system
		bf_python # duplicate for BPY_driver_exec
		ge_logic_ngnetwork 
		extern_bullet 
		ge_logic_loopbacknetwork 
		bf_intern_moto 
		extern_glew
		extern_openjpeg
		extern_redcode
		ge_videotex
		bf_rna
		bf_dna
		bf_blenfont
		bf_intern_audaspace
		bf_intern_mikktspace
	)

	if(WITH_BINRELOC)
		list(APPEND BLENDER_SORTED_LIBS extern_binreloc)
	endif()

	if(WITH_CXX_GUARDEDALLOC)
		list(APPEND BLENDER_SORTED_LIBS bf_intern_guardedalloc_cpp)
	endif()

	if(WITH_IK_ITASC)
		list(APPEND BLENDER_SORTED_LIBS bf_intern_itasc)
	endif()

	if(WITH_CODEC_QUICKTIME)
		list(APPEND BLENDER_SORTED_LIBS bf_quicktime)
	endif()

	foreach(SORTLIB ${BLENDER_SORTED_LIBS})
		set(REMLIB ${SORTLIB})
		foreach(SEARCHLIB ${BLENDER_LINK_LIBS})
			if(${SEARCHLIB} STREQUAL ${SORTLIB})
				set(REMLIB "")
			endif()
		endforeach()
		if(REMLIB)
			# message(STATUS "Removing library ${REMLIB} from blender linking because: not configured")
			list(APPEND REM_MSG ${REMLIB})
			list(REMOVE_ITEM BLENDER_SORTED_LIBS ${REMLIB})
		endif()
	endforeach()
	if(REM_MSG)
		list(SORT REM_MSG)
		message(STATUS "Blender Skipping: (${REM_MSG})")
	endif()
	target_link_libraries(blender ${BLENDER_SORTED_LIBS})
#else()
#	target_link_libraries(blender ${BLENDER_LINK_LIBS})
#endif()

setup_liblinks(blender)
