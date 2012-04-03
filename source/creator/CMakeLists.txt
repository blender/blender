# -*- mode: cmake; indent-tabs-mode: t; -*-
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

if(WIN32)
	blender_include_dirs(../../intern/utfconv)
endif()

if(WITH_LIBMV)
	blender_include_dirs(../../extern/libmv)
	add_definitions(-DWITH_LIBMV)
endif()

if(WITH_CODEC_FFMPEG)
	add_definitions(-DWITH_FFMPEG)
endif()

if(WITH_PYTHON)
	blender_include_dirs(../blender/python)
	add_definitions(-DWITH_PYTHON)

	if(WITH_PYTHON_SECURITY)
		add_definitions(-DWITH_PYTHON_SECURITY)
	endif()
endif()

if(WITH_HEADLESS)
	add_definitions(-DWITH_HEADLESS)
endif()

if(WITH_GAMEENGINE)
	blender_include_dirs(../gameengine/BlenderRoutines)

	add_definitions(-DWITH_GAMEENGINE)
endif()

if(WITH_SDL)
	add_definitions(-DWITH_SDL)
endif()

if(WITH_BINRELOC)
	add_definitions(-DWITH_BINRELOC)
	blender_include_dirs(${BINRELOC_INCLUDE_DIRS})
endif()

# Setup the exe sources and buildinfo
set(SRC
	creator.c
)

# MSVC 2010 gives linking errors with the manifest
if(WIN32 AND NOT UNIX)
	string(SUBSTRING ${BLENDER_VERSION} 0 1 bver1)
	string(SUBSTRING ${BLENDER_VERSION} 2 1 bver2)
	string(SUBSTRING ${BLENDER_VERSION} 3 1 bver3)
	if(MINGW)
		add_definitions(
			-DWINDRES
			-DBLEN_VER_RC_STR_M=${BLENDER_VERSION}
			-DBLEN_VER_RC_1=${bver1}
			-DBLEN_VER_RC_2=${bver2}
			-DBLEN_VER_RC_3=${bver3}
			-DBLEN_VER_RC_4=0
		)
	else()
		add_definitions(
			-DBLEN_VER_RC_STR=${BLENDER_VERSION}
			-DBLEN_VER_RC_1=${bver1}
			-DBLEN_VER_RC_2=${bver2}
			-DBLEN_VER_RC_3=${bver3}
			-DBLEN_VER_RC_4=0
		)
	endif()


	list(APPEND SRC
		../icons/winblender.rc
	)
endif()

if(WITH_BUILDINFO)
	add_definitions(-DWITH_BUILDINFO)
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
	#
	# note that 'SHARED' works on Linux and Windows,
	# but not OSX which _must_ be 'MODULE'
	add_library(blender MODULE ${SRC})
	set_target_properties(
		blender
		PROPERTIES
			PREFIX ""
			OUTPUT_NAME bpy
			LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
			RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin  # only needed on windows
	)

	if(APPLE)
		set_target_properties(
			blender
			PROPERTIES
				LINK_FLAGS_RELEASE "${PLATFORM_LINKFLAGS}"
				LINK_FLAGS_DEBUG "${PLATFORM_LINKFLAGS_DEBUG}"
		)
	endif()

	if(WIN32)
		# python modules use this
		set_target_properties(
			blender
			PROPERTIES
			SUFFIX ".pyd"
		)
	endif()

else()
	add_executable(blender ${EXETYPE} ${SRC})
endif()

if(WITH_BUILDINFO)
	# explicitly say that the executable depends on the buildinfo
	add_dependencies(blender buildinfo)
endif()

# Post build steps for bundling/packaging.


if(MSVC_IDE)
	# ${CMAKE_CFG_INTDIR} should replace \${BUILD_TYPE} when using add_command
	set(TARGETDIR ${EXECUTABLE_OUTPUT_PATH}/\${BUILD_TYPE})
elseif(APPLE)
	set(TARGETDIR ${EXECUTABLE_OUTPUT_PATH}/\${BUILD_TYPE})
else()
	set(TARGETDIR ${EXECUTABLE_OUTPUT_PATH})
endif()

set(BLENDER_TEXT_FILES
	${CMAKE_SOURCE_DIR}/release/text/GPL-license.txt
	${CMAKE_SOURCE_DIR}/release/text/Python-license.txt
	${CMAKE_SOURCE_DIR}/release/text/copyright.txt
	${CMAKE_SOURCE_DIR}/release/text/readme.html
)


# -----------------------------------------------------------------------------
# Platform Specific Var: TARGETDIR_VER

if(UNIX AND NOT APPLE)
	if(WITH_INSTALL_PORTABLE)
		set(TARGETDIR_VER ${TARGETDIR}/${BLENDER_VERSION})
	else()
		if(WITH_PYTHON_MODULE)
			set(TARGETDIR_VER ${PYTHON_LIBPATH}/python${PYTHON_VERSION}/site-packages/${BLENDER_VERSION})
		else()
			set(TARGETDIR_VER ${CMAKE_INSTALL_PREFIX}/share/blender/${BLENDER_VERSION})
		endif()
	endif()

elseif(WIN32)
	set(TARGETDIR_VER ${TARGETDIR}/${BLENDER_VERSION})

elseif(APPLE)
	if(WITH_PYTHON_MODULE)
		set(TARGETDIR_VER ${TARGETDIR}/${BLENDER_VERSION})
	else()
		set(TARGETDIR_VER ${TARGETDIR}/blender.app/Contents/MacOS/${BLENDER_VERSION})
	endif()

endif()


# -----------------------------------------------------------------------------
# Install Targets (Generic, All Platforms)


# important to make a clean  install each time, else old scripts get loaded.
install(
	CODE
	"file(REMOVE_RECURSE ${TARGETDIR_VER})"
)

if(WITH_PYTHON)
	# install(CODE "message(\"copying blender scripts...\")")
	
	# exclude addons_contrib if release
	if("${BLENDER_VERSION_CYCLE}" STREQUAL "release")
		set(ADDON_EXCLUDE_CONDITIONAL "addons_contrib/*")
	else()
		set(ADDON_EXCLUDE_CONDITIONAL "_addons_contrib/*")  # dummy, wont do anything
	endif()

	install(
		DIRECTORY ${CMAKE_SOURCE_DIR}/release/scripts
		DESTINATION ${TARGETDIR_VER}
		PATTERN ".svn" EXCLUDE
		PATTERN "__pycache__" EXCLUDE
		PATTERN "${ADDON_EXCLUDE_CONDITIONAL}" EXCLUDE
	)
	
	unset(ADDON_EXCLUDE_CONDITIONAL)
endif()

# localization
if(WITH_INTERNATIONAL)
	install(
		DIRECTORY
			${CMAKE_SOURCE_DIR}/release/datafiles/locale
			${CMAKE_SOURCE_DIR}/release/datafiles/fonts
		DESTINATION ${TARGETDIR_VER}/datafiles
		PATTERN ".svn" EXCLUDE
	)
endif()

# helpful tip when using make
if("${CMAKE_GENERATOR}" MATCHES ".*Makefiles.*")
	# message after building.
	add_custom_command(
		TARGET blender POST_BUILD MAIN_DEPENDENCY blender
		COMMAND ${CMAKE_COMMAND} -E echo 'now run: \"make install\" to copy runtime files and scripts to ${TARGETDIR_VER}'
	)
endif()


# -----------------------------------------------------------------------------
# Install Targets (Platform Specific)

if(UNIX AND NOT APPLE)

	# there are a few differences between portable and system install
	if(WITH_INSTALL_PORTABLE)
		install(
			FILES
				${CMAKE_SOURCE_DIR}/release/freedesktop/blender.desktop
				${CMAKE_SOURCE_DIR}/release/freedesktop/icons/scalable/apps/blender.svg
				${CMAKE_SOURCE_DIR}/doc/manpage/blender.1
			DESTINATION ${TARGETDIR}
		)

		install(
			PROGRAMS
			${CMAKE_SOURCE_DIR}/release/bin/blender-thumbnailer.py
			DESTINATION ${TARGETDIR}
		)

		install(
			FILES ${BLENDER_TEXT_FILES}
			DESTINATION ${TARGETDIR}
		)

	else()
		# main blender binary
		if(WITH_PYTHON_MODULE)
			install(
				TARGETS blender
				LIBRARY
				DESTINATION ${PYTHON_LIBPATH}/python${PYTHON_VERSION}/site-packages
			)
		else()
			install(
				PROGRAMS ${TARGETDIR}/blender
				DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
			)
		endif()


		if(WITH_GAMEENGINE AND WITH_PLAYER)
			install(
				PROGRAMS ${TARGETDIR}/blenderplayer
				DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
			)
		endif()

		# misc files
		install(
			FILES ${CMAKE_SOURCE_DIR}/release/freedesktop/blender.desktop
			DESTINATION ${CMAKE_INSTALL_PREFIX}/share/applications
		)
		install(
			DIRECTORY
				${CMAKE_SOURCE_DIR}/release/freedesktop/icons/16x16
				${CMAKE_SOURCE_DIR}/release/freedesktop/icons/22x22
				${CMAKE_SOURCE_DIR}/release/freedesktop/icons/24x24
				${CMAKE_SOURCE_DIR}/release/freedesktop/icons/32x32
				${CMAKE_SOURCE_DIR}/release/freedesktop/icons/48x48
				${CMAKE_SOURCE_DIR}/release/freedesktop/icons/256x256
			DESTINATION ${CMAKE_INSTALL_PREFIX}/share/icons/hicolor
			PATTERN ".svn" EXCLUDE
			PATTERN "*.svg" EXCLUDE
		)
		install(
			FILES ${CMAKE_SOURCE_DIR}/release/freedesktop/icons/scalable/apps/blender.svg
			DESTINATION ${CMAKE_INSTALL_PREFIX}/share/icons/hicolor/scalable/apps
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
			FILES ${BLENDER_TEXT_FILES}
			DESTINATION ${CMAKE_INSTALL_PREFIX}/share/doc/blender
		)
	endif()

		# plugins in blender 2.5 don't work at the moment.
		#
		# install(
		# 	DIRECTORY ${CMAKE_SOURCE_DIR}/release/plugins
		# 	DESTINATION ${TARGETDIR_VER}
		# 	PATTERN ".svn" EXCLUDE
		# )

	if(WITH_PYTHON)
		if(WITH_PYTHON_INSTALL)
			# on some platforms (like openSUSE) Python is linked
			# to be used from lib64 folder.
			# determine this from Python's libraries path
			if(${PYTHON_LIBPATH} MATCHES "lib64$")
				set(_target_LIB "lib64")
			else()
				set(_target_LIB "lib")
			endif()

			# Copy the systems python into the install directory
			# Scons copy in tools/Blender.py
			# install(CODE "message(\"copying a subset of the systems python...\")")
			install(
				DIRECTORY ${PYTHON_LIBPATH}/python${PYTHON_VERSION}
				DESTINATION ${TARGETDIR_VER}/python/${_target_LIB}
				PATTERN ".svn" EXCLUDE
				PATTERN "__pycache__" EXCLUDE               # * any cache *
				PATTERN "distutils" EXCLUDE                 # ./distutils
				PATTERN "lib2to3" EXCLUDE                   # ./lib2to3
				PATTERN "config" EXCLUDE                    # ./config
				PATTERN "config-*" EXCLUDE                  # ./config-*
				PATTERN "site-packages/*" EXCLUDE           # ./site-packages/*
				PATTERN "tkinter" EXCLUDE                   # ./tkinter
				PATTERN "lib-dynload/_tkinter.*" EXCLUDE    # ./lib-dynload/_tkinter.co
				PATTERN "idlelib" EXCLUDE                   # ./idlelib
				PATTERN "test" EXCLUDE                      # ./test
				PATTERN "turtledemo" EXCLUDE                # ./turtledemo
				PATTERN "turtle.py" EXCLUDE					# ./turtle.py
			)

			# # doesnt work, todo
			# install(CODE "execute_process(COMMAND find ${TARGETDIR}/${BLENDER_VERSION}/python/lib/ -name '*.so' -exec strip -s {} '\;')")
			unset(_target_LIB)
		endif()
	endif()
elseif(WIN32)

	install(
		FILES ${BLENDER_TEXT_FILES}
		DESTINATION ${TARGETDIR}
	)

	if(WITH_INTERNATIONAL)
		install(
			FILES ${LIBDIR}/gettext/lib/gnu_gettext.dll
			DESTINATION ${TARGETDIR}
		)

		if(NOT CMAKE_CL_64)
			install(
				FILES ${LIBDIR}/iconv/lib/iconv.dll
				DESTINATION ${TARGETDIR}
			)
		endif()
	endif()

	# plugins in blender 2.5 don't work at the moment.
	#
	# install(
	# 	DIRECTORY ${CMAKE_SOURCE_DIR}/release/plugins
	# 	DESTINATION ${TARGETDIR_VER}
	# 	PATTERN ".svn" EXCLUDE
	# )

	if(WITH_PYTHON)
		set_lib_path(PYLIB "python/lib")
		install(
			FILES ${PYLIB}/python32.dll
			DESTINATION ${TARGETDIR}
			CONFIGURATIONS Release;RelWithDebInfo;MinSizeRel
		)

		install(
			FILES ${PYLIB}/python32_d.dll
			DESTINATION ${TARGETDIR}
			CONFIGURATIONS Debug
		)

		if(WITH_PYTHON_INSTALL)
			# note, as far as python is concerned 'RelWithDebInfo' is not debug since its without debug flags.

			# create the directory in multiple steps, so it actually gets created when it doesn't exist yet
			install(
				CODE
				"
				message(\"creating ${TARGETDIR_VER}/python/lib\")
				file(MAKE_DIRECTORY \"${TARGETDIR_VER}/python\")
				file(MAKE_DIRECTORY \"${TARGETDIR_VER}/python/lib\")
				message(\"done creating dir\")
				"
			)

			install(
				CODE
				"
				if(\"\${CMAKE_INSTALL_CONFIG_NAME}\" STREQUAL \"Debug\")
					execute_process(COMMAND \"${CMAKE_COMMAND}\" -E chdir \"${TARGETDIR_VER}/python/lib\"
						\"${CMAKE_COMMAND}\" -E tar xzfv \"${LIBDIR}/release/python32_d.tar.gz\")
				else()
					execute_process(COMMAND \"${CMAKE_COMMAND}\" -E chdir \"${TARGETDIR_VER}/python/lib\"
						\"${CMAKE_COMMAND}\" -E tar xzfv \"${LIBDIR}/release/python32.tar.gz\")
				endif()
				"
			)

			# doesnt work, todo
			# install(CODE "execute_process(COMMAND find ${TARGETDIR}/${BLENDER_VERSION}/python/lib/ -name '*.so' -exec strip -s {} '\;')")
		endif()
	endif()

	if(CMAKE_CL_64)
		# gettext and png are statically linked on win64
		install(
			FILES ${LIBDIR}/zlib/lib/zlib.dll
			DESTINATION ${TARGETDIR}
		)
	else()
		install(
			FILES
				${LIBDIR}/zlib/lib/zlib.dll
			DESTINATION ${TARGETDIR}
		)
	endif()

	if(MSVC)
		install(
			FILES ${LIBDIR}/pthreads/lib/pthreadVC2.dll
			DESTINATION ${TARGETDIR}
		)
	else()
		install(
			FILES ${LIBDIR}/pthreads/lib/pthreadGC2.dll
			DESTINATION ${TARGETDIR}
		)
	endif()

	if(WITH_CODEC_FFMPEG)
		install(
			FILES
				${LIBDIR}/ffmpeg/lib/avcodec-53.dll
				${LIBDIR}/ffmpeg/lib/avformat-53.dll
				${LIBDIR}/ffmpeg/lib/avdevice-53.dll
				${LIBDIR}/ffmpeg/lib/avutil-51.dll
				${LIBDIR}/ffmpeg/lib/swscale-2.dll
			DESTINATION ${TARGETDIR}
		)

	endif()

	if(WITH_CODEC_SNDFILE)
		install(
			FILES ${LIBDIR}/sndfile/lib/libsndfile-1.dll
			DESTINATION ${TARGETDIR}
		)
	endif()

	if(WITH_OPENAL)
		install(
			FILES
				${LIBDIR}/openal/lib/OpenAL32.dll
				${LIBDIR}/openal/lib/wrap_oal.dll
			DESTINATION ${TARGETDIR}
		)
	endif()

	if(WITH_SDL)
		if(NOT CMAKE_CL_64)
			install(
				FILES ${LIBDIR}/sdl/lib/SDL.dll
				DESTINATION ${TARGETDIR}
			)
		endif()
	endif()

	if(NOT CMAKE_CL_64)
		install(
			FILES ${LIBDIR}/thumbhandler/lib/BlendThumb.dll
			DESTINATION ${TARGETDIR}
		)
	endif()
		
	install( # x86 builds can run on x64 Windows, so this is required at all times
		FILES ${LIBDIR}/thumbhandler/lib/BlendThumb64.dll
		DESTINATION ${TARGETDIR}
	)	

	if(WITH_OPENIMAGEIO)
		set_lib_path(OIIOBIN "openimageio/bin")
		install(
			FILES
				${OIIOBIN}/OpenImageIO.dll
			DESTINATION ${TARGETDIR}
		)
	endif()

elseif(APPLE)

	# handy install macro to exclude files, we use \$ escape for the "to"
	# argument when calling so ${BUILD_TYPE} does not get expanded
	macro(install_dir from to)
		install(
			DIRECTORY ${from}
			DESTINATION ${to}
			PATTERN ".svn" EXCLUDE
			PATTERN "*.pyc" EXCLUDE
			PATTERN "*.pyo" EXCLUDE
			PATTERN "*.orig" EXCLUDE
			PATTERN "*.rej" EXCLUDE
			PATTERN "__pycache__" EXCLUDE
			PATTERN "__MACOSX" EXCLUDE
			PATTERN ".DS_Store" EXCLUDE
		)
	endmacro()

	set(OSX_APP_SOURCEDIR ${CMAKE_SOURCE_DIR}/source/darwin/blender.app)

	# setup Info.plist
	execute_process(COMMAND date "+%Y-%m-%d" OUTPUT_VARIABLE BLENDER_DATE OUTPUT_STRIP_TRAILING_WHITESPACE)

	set_target_properties(blender PROPERTIES
		MACOSX_BUNDLE_INFO_PLIST ${OSX_APP_SOURCEDIR}/Contents/Info.plist
		MACOSX_BUNDLE_SHORT_VERSION_STRING ${BLENDER_VERSION}
		MACOSX_BUNDLE_LONG_VERSION_STRING "${BLENDER_VERSION} ${BLENDER_DATE}")


	# install release and app files
	install(
		FILES ${BLENDER_TEXT_FILES}
		DESTINATION ${TARGETDIR}
	)

	install(
		FILES ${OSX_APP_SOURCEDIR}/Contents/PkgInfo
		DESTINATION ${TARGETDIR}/blender.app/Contents
	)

	install_dir(
		${OSX_APP_SOURCEDIR}/Contents/Resources
		\${TARGETDIR}/blender.app/Contents/
	)

	# python
	if(WITH_PYTHON AND NOT WITH_PYTHON_MODULE)
		# the python zip is first extract as part of the build process,
		# and then later installed as part of make install. this is much
		# quicker, and means we can easily exclude files on copy
		# Not needed for PYTHON_MODULE or WEB_PLUGIN due uses  Pyhon framework
		add_custom_target(
			extractpyzip
			DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/python)

		set(PYTHON_ZIP "python_${CMAKE_OSX_ARCHITECTURES}.zip")

		add_custom_command(
			OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/python
			COMMAND rm -rf ${CMAKE_CURRENT_BINARY_DIR}/python/
			COMMAND mkdir ${CMAKE_CURRENT_BINARY_DIR}/python/
			COMMAND unzip -q ${LIBDIR}/release/${PYTHON_ZIP} -d ${CMAKE_CURRENT_BINARY_DIR}/python/
			DEPENDS ${LIBDIR}/release/${PYTHON_ZIP})

		add_dependencies(blender extractpyzip)

		# copy extracted python files
		install_dir(
			${CMAKE_CURRENT_BINARY_DIR}/python
			\${TARGETDIR_VER}
		)

	endif()

	# install blenderplayer bundle - copy of blender.app above. re-using macros et al
	# note we are using OSX Bundle as base and copying Blender dummy bundle on top of it
	if(WITH_GAMEENGINE AND WITH_PLAYER)
		set(OSX_APP_PLAYER_SOURCEDIR ${CMAKE_SOURCE_DIR}/source/darwin/blenderplayer.app)
		set(PLAYER_SOURCEINFO ${OSX_APP_PLAYER_SOURCEDIR}/Contents/Info.plist)
		set(PLAYER_TARGETDIR_VER ${TARGETDIR}/blenderplayer.app/Contents/MacOS/${BLENDER_VERSION})


		# important to make a clean  install each time else old scripts get loaded.
		install(
			CODE
			"file(REMOVE_RECURSE ${PLAYER_TARGETDIR_VER})"
		)

		install(
			FILES ${OSX_APP_PLAYER_SOURCEDIR}/Contents/PkgInfo
			DESTINATION ${TARGETDIR}/blenderplayer.app/Contents
		)

		install_dir(
			${OSX_APP_PLAYER_SOURCEDIR}/Contents/Resources
			\${TARGETDIR}/blenderplayer.app/Contents/
		)

		# python
		if(WITH_PYTHON)
			add_custom_command(
				OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/python
				COMMAND rm -rf ${CMAKE_CURRENT_BINARY_DIR}/python/
				COMMAND mkdir ${CMAKE_CURRENT_BINARY_DIR}/python/
				COMMAND unzip -q ${LIBDIR}/release/${PYTHON_ZIP} -d ${CMAKE_CURRENT_BINARY_DIR}/python/
				DEPENDS ${LIBDIR}/release/${PYTHON_ZIP})

			# copy extracted python files
			install_dir(
				${CMAKE_CURRENT_BINARY_DIR}/python
				\${PLAYER_TARGETDIR_VER}
			)
		endif()

	endif()
endif()

# -----------------------------------------------------------------------------
# Generic Install, for all targets



# install more files specified elsewhere
delayed_do_install(${TARGETDIR_VER})

unset(BLENDER_TEXT_FILES)


# -----------------------------------------------------------------------------
# Setup link libs

add_dependencies(blender makesdna)

get_property(BLENDER_LINK_LIBS GLOBAL PROPERTY BLENDER_LINK_LIBS)

set(BLENDER_LINK_LIBS
	${BLENDER_LINK_LIBS}
	bf_windowmanager
	bf_render
)

if(WITH_MOD_FLUID)
	list(APPEND BLENDER_LINK_LIBS bf_intern_elbeem)
endif()

if(WITH_CYCLES)
	list(APPEND BLENDER_LINK_LIBS
		cycles_render
		cycles_bvh
		cycles_device
		cycles_kernel
		cycles_util
		cycles_subd)
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
		bf_editor_space_time
		bf_editor_space_userpref
		bf_editor_space_view3d
		bf_editor_space_clip

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
		bf_python_mathutils
		bf_python_bmesh
		bf_ikplugin
		bf_modifiers
		bf_bmesh
		bf_blenkernel
		bf_nodes
		bf_gpu
		bf_blenloader
		bf_imbuf
		bf_blenlib
		bf_intern_ghost
		bf_intern_string
		bf_blenpluginapi
		bf_avi
		bf_imbuf_cineon
		bf_imbuf_openexr
		bf_imbuf_dds
		bf_collada
		bf_intern_bsp
		bf_intern_bop
		bf_intern_decimate
		bf_intern_elbeem
		bf_intern_ik
		bf_intern_memutil
		bf_intern_guardedalloc
		bf_intern_ctr
		bf_intern_utfconv
		ge_blen_routines
		ge_converter
		ge_phys_dummy
		ge_phys_bullet
		bf_intern_smoke
		extern_minilzo
		extern_lzma
		extern_colamd
		ge_logic_ketsji
		extern_recastnavigation
		ge_phys_common
		ge_logic
		ge_rasterizer
		ge_oglrasterizer
		ge_logic_expressions
		ge_scenegraph
		ge_logic_network
		ge_logic_ngnetwork
		extern_bullet
		ge_logic_loopbacknetwork
		bf_intern_moto
		extern_openjpeg
		extern_redcode
		ge_videotex
		bf_rna
		bf_dna
		bf_blenfont
		bf_intern_audaspace
		bf_intern_mikktspace
		bf_intern_dualcon
		bf_intern_cycles
		cycles_render
		cycles_bvh
		cycles_device
		cycles_kernel
		cycles_util
		cycles_subd
	)

	if(WITH_LIBMV)
		list(APPEND BLENDER_SORTED_LIBS extern_libmv)
	endif()

	if(WITH_MOD_CLOTH_ELTOPO)
		list(APPEND BLENDER_SORTED_LIBS extern_eltopo)
	endif()

	if(WITH_BUILTIN_GLEW)
		list(APPEND BLENDER_SORTED_LIBS extern_glew)
	endif()

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

	if(WITH_CARVE)
		list(APPEND BLENDER_SORTED_LIBS extern_carve)
	endif()

	if(WITH_GHOST_XDND)
		list(APPEND BLENDER_SORTED_LIBS extern_xdnd)
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

	unset(SEARCHLIB)
	unset(SORTLIB)
	unset(REMLIB)
	unset(REM_MSG)

#else()
#	target_link_libraries(blender ${BLENDER_LINK_LIBS})
#endif()

setup_liblinks(blender)
