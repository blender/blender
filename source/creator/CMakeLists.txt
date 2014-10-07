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
	../../intern/glew-mx
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

add_definitions(${GL_DEFINITIONS})
blender_include_dirs(${GLEW_INCLUDE_PATH})

if(WIN32)
	blender_include_dirs(../../intern/utfconv)
endif()

if(WITH_LIBMV)
	blender_include_dirs(../../extern/libmv)
	add_definitions(-DWITH_LIBMV)
endif()

if(WITH_CYCLES AND WITH_CYCLES_LOGGING)
	blender_include_dirs(../../intern/cycles/blender)
	add_definitions(-DWITH_CYCLES_LOGGING)
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

if(WITH_FREESTYLE)
	add_definitions(-DWITH_FREESTYLE)
	blender_include_dirs(../blender/freestyle)
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
		# -DBUILD_COMMIT_TIMESTAMP="${BUILD_COMMIT_TIMESTAMP}"
		# -DBUILD_COMMIT_TIME="${BUILD_COMMIT_TIME}"
		# -DBUILD_COMMIT_DATE="${BUILD_COMMIT_DATE}"
		# -DBUILD_HASH="${BUILD_HASH}"
		# -DBUILD_BRANCH="${BUILD_BRANCH}"
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

	# make an object library so can load with it in tests
	add_library(buildinfoobj OBJECT buildinfo.c)
	add_dependencies(buildinfoobj buildinfo)
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
	${CMAKE_SOURCE_DIR}/release/datafiles/LICENSE-bfont.ttf.txt
)

if(WITH_INTERNATIONAL)
	list(APPEND BLENDER_TEXT_FILES
		${CMAKE_SOURCE_DIR}/release/datafiles/LICENSE-droidsans.ttf.txt
		${CMAKE_SOURCE_DIR}/release/datafiles/LICENSE-bmonofont-i18n.ttf.txt
	)
endif()


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
		set(TARGETDIR_VER ${TARGETDIR}/blender.app/Contents/Resources/${BLENDER_VERSION})
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

	# do not install freestyle dir if disabled
	if(NOT WITH_FREESTYLE)
		set(FREESTYLE_EXCLUDE_CONDITIONAL "freestyle/*")
	else()
		set(FREESTYLE_EXCLUDE_CONDITIONAL "_freestyle/*")  # dummy, wont do anything
	endif()

	install(
		DIRECTORY ${CMAKE_SOURCE_DIR}/release/scripts
		DESTINATION ${TARGETDIR_VER}
		PATTERN ".git" EXCLUDE
		PATTERN "__pycache__" EXCLUDE
		PATTERN "${ADDON_EXCLUDE_CONDITIONAL}" EXCLUDE
		PATTERN "${FREESTYLE_EXCLUDE_CONDITIONAL}" EXCLUDE
	)
	
	unset(ADDON_EXCLUDE_CONDITIONAL)
	unset(FREESTYLE_EXCLUDE_CONDITIONAL)
endif()

# localization
if(WITH_INTERNATIONAL)
	install(
		DIRECTORY
			${CMAKE_SOURCE_DIR}/release/datafiles/fonts
		DESTINATION ${TARGETDIR_VER}/datafiles
	)

	set(_locale_dir "${CMAKE_SOURCE_DIR}/release/datafiles/locale")
	set(_locale_target_dir ${TARGETDIR_VER}/datafiles/locale)

	file(GLOB _po_files "${_locale_dir}/po/*.po")
	foreach(_po_file ${_po_files})
		msgfmt_simple(${_po_file} _all_mo_files)
	endforeach()

	# Create a custom target which will compile all po to mo
	add_custom_target(
		locales
		DEPENDS ${_all_mo_files})

	add_dependencies(blender locales)

	# Generate INSTALL rules
	install(
		FILES ${_locale_dir}/languages
		DESTINATION ${_locale_target_dir}
	)

	foreach(_mo_file ${_all_mo_files})
		get_filename_component(_locale_name ${_mo_file} NAME_WE)
		install(
			FILES ${_mo_file}
			DESTINATION ${_locale_target_dir}/${_locale_name}/LC_MESSAGES
			RENAME blender.mo
		)
		unset(_locale_name)
	endforeach()

	unset(_all_mo_files)
	unset(_po_files)
	unset(_po_file)
	unset(_mo_file)
	unset(_locale_target_dir)

	unset(_locale_dir)
endif()

# color management
if(WITH_OPENCOLORIO)
	install(
		DIRECTORY ${CMAKE_SOURCE_DIR}/release/datafiles/colormanagement
		DESTINATION ${TARGETDIR_VER}/datafiles
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

	if(NOT WITH_PYTHON_MODULE)
		install(
			CODE
			"
			execute_process(COMMAND
							${CMAKE_SOURCE_DIR}/doc/manpage/blender.1.py
							${TARGETDIR}/blender
							${TARGETDIR}/blender.1)
			"
		)
	endif()

	# there are a few differences between portable and system install
	if(WITH_INSTALL_PORTABLE)
		install(
			FILES ${TARGETDIR}/blender.1
			DESTINATION ${TARGETDIR}
		)

		install(
			FILES
				${CMAKE_SOURCE_DIR}/release/freedesktop/blender.desktop
				${CMAKE_SOURCE_DIR}/release/freedesktop/icons/scalable/apps/blender.svg
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
			# manpage only with 'blender' binary
			install(
				FILES ${TARGETDIR}/blender.1
				DESTINATION ${CMAKE_INSTALL_PREFIX}/share/man/man1
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
			FILES ${BLENDER_TEXT_FILES}
			DESTINATION ${CMAKE_INSTALL_PREFIX}/share/doc/blender
		)
	endif()

	if(WITH_PYTHON)
		if(WITH_PYTHON_INSTALL)
			# on some platforms (like openSUSE) Python is linked
			# to be used from lib64 folder.
			# determine this from Python's libraries path
			#
			# ugh, its possible 'lib64' is just a symlink to 'lib' which causes incorrect use of 'lib64'
			get_filename_component(_pypath_real ${PYTHON_LIBPATH} REALPATH)
			if(${_pypath_real} MATCHES "lib64$")
				set(_target_LIB "lib64")
			else()
				set(_target_LIB "lib")
			endif()
			unset(_pypath_real)

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
				PATTERN "turtle.py" EXCLUDE                 # ./turtle.py
			)

			# # doesnt work, todo
			# install(CODE "execute_process(COMMAND find ${TARGETDIR}/${BLENDER_VERSION}/python/lib/ -name '*.so' -exec strip -s {} '\;')")

			if(WITH_PYTHON_INSTALL_NUMPY)
				# Install to the same directory as the source, so debian-like
				# distros are happy with their policy.
				set(_suffix "site-packages")
				if(${PYTHON_NUMPY_PATH} MATCHES "dist-packages")
					set(_suffix "dist-packages")
				endif()
				install(
					DIRECTORY ${PYTHON_NUMPY_PATH}/numpy
					DESTINATION ${TARGETDIR_VER}/python/${_target_LIB}/python${PYTHON_VERSION}/${_suffix}
					PATTERN ".svn" EXCLUDE
					PATTERN "__pycache__" EXCLUDE           # * any cache *
					PATTERN "*.pyc" EXCLUDE                 # * any cache *
					PATTERN "*.pyo" EXCLUDE                 # * any cache *
					PATTERN "distutils" EXCLUDE             # ./distutils
					PATTERN "oldnumeric" EXCLUDE            # ./oldnumeric
					PATTERN "doc" EXCLUDE                   # ./doc
					PATTERN "tests" EXCLUDE                 # ./tests
					PATTERN "f2py" EXCLUDE                  # ./f2py - fortran/python interface code, not fun for blender devs.
					PATTERN "include" EXCLUDE               # include dirs all over, we wont use NumPy/CAPI
					PATTERN "*.h" EXCLUDE                   # some includes are not in include dirs
					PATTERN "*.a" EXCLUDE                   # ./core/lib/libnpymath.a - for linking, we dont need.
				)
				unset(_suffix)
			endif()

			# Copy requests, we need to generalize site-packages
			if(WITH_PYTHON_INSTALL_REQUESTS)
				set(_suffix "site-packages")
				if(${PYTHON_REQUESTS_PATH} MATCHES "dist-packages")
					set(_suffix "dist-packages")
				endif()
				install(
					DIRECTORY ${PYTHON_REQUESTS_PATH}/requests
					DESTINATION ${TARGETDIR_VER}/python/${_target_LIB}/python${PYTHON_VERSION}/${_suffix}
					PATTERN ".svn" EXCLUDE
					PATTERN "__pycache__" EXCLUDE           # * any cache *
					PATTERN "*.pyc" EXCLUDE                 # * any cache *
					PATTERN "*.pyo" EXCLUDE                 # * any cache *
					PATTERN "cacert.pem" EXCLUDE            # for now we don't deal with security
				)
				# On some platforms requests does have extra dependencies.
				set(_requests_deps "chardet" "urllib3")
				foreach(_requests_dep ${_requests_deps})
					if(EXISTS ${PYTHON_REQUESTS_PATH}/${_requests_dep})
						install(
							DIRECTORY ${PYTHON_REQUESTS_PATH}/${_requests_dep}
							DESTINATION ${TARGETDIR_VER}/python/${_target_LIB}/python${PYTHON_VERSION}/${_suffix}
							PATTERN ".svn" EXCLUDE
							PATTERN "__pycache__" EXCLUDE           # * any cache *
							PATTERN "*.pyc" EXCLUDE                 # * any cache *
							PATTERN "*.pyo" EXCLUDE                 # * any cache *
						)
					endif()
				endforeach()
				if(EXISTS ${PYTHON_REQUESTS_PATH}/six.py)
					install(
						FILES ${PYTHON_REQUESTS_PATH}/six.py
						DESTINATION ${TARGETDIR_VER}/python/${_target_LIB}/python${PYTHON_VERSION}/${_suffix}
					)
					endif()
				unset(_requests_dep)
				unset(_requests_deps)
				unset(_suffix)
			endif()
			unset(_target_LIB)
			
		endif()
	endif()
elseif(WIN32)

	install(
		FILES ${BLENDER_TEXT_FILES}
		DESTINATION ${TARGETDIR}
	)

	if(WITH_PYTHON)
		string(REPLACE "." "" _PYTHON_VERSION_NO_DOTS ${PYTHON_VERSION})

		install(
			FILES ${LIBDIR}/python/lib/python${_PYTHON_VERSION_NO_DOTS}.dll ${LIBDIR}/python/lib/sqlite3.dll
			DESTINATION ${TARGETDIR}
			CONFIGURATIONS Release;RelWithDebInfo;MinSizeRel
		)

		install(
			FILES ${LIBDIR}/python/lib/python${_PYTHON_VERSION_NO_DOTS}_d.dll ${LIBDIR}/python/lib/sqlite3_d.dll
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
						\"${CMAKE_COMMAND}\" -E tar xzfv \"${LIBDIR}/release/python${_PYTHON_VERSION_NO_DOTS}_d.tar.gz\")
				else()
					execute_process(COMMAND \"${CMAKE_COMMAND}\" -E chdir \"${TARGETDIR_VER}/python/lib\"
						\"${CMAKE_COMMAND}\" -E tar xzfv \"${LIBDIR}/release/python${_PYTHON_VERSION_NO_DOTS}.tar.gz\")
				endif()
				"
			)

			if(WITH_PYTHON_INSTALL_NUMPY)
				install(
					CODE
					"
					execute_process(COMMAND \"${CMAKE_COMMAND}\" -E chdir \"${TARGETDIR_VER}/python/lib/site-packages\"
						\"${CMAKE_COMMAND}\" -E tar xzfv \"${LIBDIR}/release/python${_PYTHON_VERSION_NO_DOTS}_numpy_1.8.tar.gz\")
					"
				)
			endif()

			# release/site-packages
			install(
				CODE
				"
				execute_process(COMMAND \"${CMAKE_COMMAND}\" -E copy_directory
					${LIBDIR}/release/site-packages
					\"${TARGETDIR_VER}/python/lib/site-packages\")
				"
			)


			# doesnt work, todo
			# install(CODE "execute_process(COMMAND find ${TARGETDIR}/${BLENDER_VERSION}/python/lib/ -name '*.so' -exec strip -s {} '\;')")
		endif()

		unset(_PYTHON_VERSION_NO_DOTS)
	endif()

	# EGL Runtime Components
	if(WITH_GL_EGL)
		if(WIN32)
			install(FILES "${OPENGLES_DLL}"     DESTINATION ${TARGETDIR})
			install(FILES "${OPENGLES_EGL_DLL}" DESTINATION ${TARGETDIR})

			if(WITH_GL_ANGLE)
				install(FILES "${D3DCOMPILER_DLL}" DESTINATION ${TARGETDIR})
			endif()
		endif()
	endif()

	if(MSVC)
		install(
			FILES ${LIBDIR}/pthreads/lib/pthreadVC2.dll
			DESTINATION ${TARGETDIR}
		)
	else()
		#MinGW64 comes with own version. For portable builds it will probaly have to be copied to work
		if(NOT WITH_MINGW64)
			install(
				FILES ${LIBDIR}/pthreads/lib/pthreadGC2.dll
				DESTINATION ${TARGETDIR}
			)
		elseif(WITH_MINGW64)
			install(
				FILES 
					${LIBDIR}/binaries/libgcc_s_sjlj-1.dll
					${LIBDIR}/binaries/libwinpthread-1.dll
					${LIBDIR}/binaries/libstdc++-6.dll
				DESTINATION ${TARGETDIR}
			)
			
			if(WITH_OPENMP)
				install(
					FILES 
					${LIBDIR}/binaries/libgomp-1.dll
					DESTINATION ${TARGETDIR}
				)
			endif()
		endif()
	endif()

	if(WITH_CODEC_FFMPEG)
		if(WITH_MINGW64)
			install(
				FILES
					${LIBDIR}/ffmpeg/lib/avcodec-53.dll
					${LIBDIR}/ffmpeg/lib/avformat-53.dll
					${LIBDIR}/ffmpeg/lib/avdevice-53.dll
					${LIBDIR}/ffmpeg/lib/avutil-51.dll
					${LIBDIR}/ffmpeg/lib/swscale-2.dll
					${LIBDIR}/ffmpeg/lib/swresample-0.dll
					${LIBDIR}/ffmpeg/lib/xvidcore.dll
				DESTINATION ${TARGETDIR}
			)
		else()
			install(
				FILES
					${LIBDIR}/ffmpeg/lib/avcodec-55.dll
					${LIBDIR}/ffmpeg/lib/avformat-55.dll
					${LIBDIR}/ffmpeg/lib/avdevice-55.dll
					${LIBDIR}/ffmpeg/lib/avutil-52.dll
					${LIBDIR}/ffmpeg/lib/swscale-2.dll
				DESTINATION ${TARGETDIR}
			)
		endif()
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
		install(
			FILES ${LIBDIR}/sdl/lib/SDL.dll
			DESTINATION ${TARGETDIR}
		)
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

	if(WITH_OPENCOLORIO)
		set(OCIOBIN ${LIBDIR}/opencolorio/bin)
		if(NOT MINGW)
			install(
				FILES
					${OCIOBIN}/OpenColorIO.dll
				DESTINATION ${TARGETDIR}
			)
		else()
			install(
				FILES
					${OCIOBIN}/libOpenColorIO.dll
				DESTINATION ${TARGETDIR}
			)
		endif()
	endif()

elseif(APPLE)

	# handy install macro to exclude files, we use \$ escape for the "to"
	# argument when calling so ${BUILD_TYPE} does not get expanded
	macro(install_dir from to)
		install(
			DIRECTORY ${from}
			DESTINATION ${to}
			PATTERN ".git" EXCLUDE
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

	set(OSX_APP_SOURCEDIR ${CMAKE_SOURCE_DIR}/release/darwin/blender.app)

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

	if(WITH_OPENMP AND CMAKE_C_COMPILER_ID MATCHES "Clang" AND NOT ${CMAKE_C_COMPILER_VERSION} VERSION_LESS '3.4')
		install(
			FILES ${LIBDIR}/openmp/lib/libiomp5.dylib
			DESTINATION ${TARGETDIR}/blender.app/Contents/MacOS
		)
	endif()

	if(WITH_LLVM AND NOT LLVM_STATIC)
		install(
			FILES ${LIBDIR}/llvm/lib/libLLVM-3.4.dylib
			DESTINATION ${TARGETDIR}/blender.app/Contents/MacOS
		)
	endif()

	# python
	if(WITH_PYTHON AND NOT WITH_PYTHON_MODULE AND NOT WITH_PYTHON_FRAMEWORK)
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
		# copy site-packages files
		install_dir(
			${LIBDIR}/release/site-packages
			${CMAKE_CURRENT_BINARY_DIR}/python/lib/python${PYTHON_VERSION}/site-packages
		)

	endif()
	
	# install blenderplayer bundle - copy of blender.app above. re-using macros et al
	# note we are using OSX Bundle as base and copying Blender dummy bundle on top of it
	if(WITH_GAMEENGINE AND WITH_PLAYER)
		set(OSX_APP_PLAYER_SOURCEDIR ${CMAKE_SOURCE_DIR}/release/darwin/blenderplayer.app)
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
		if(WITH_PYTHON AND NOT WITH_PYTHON_FRAMEWORK)
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

setup_blender_sorted_libs()

target_link_libraries(blender ${BLENDER_SORTED_LIBS})

setup_liblinks(blender)

# -----------------------------------------------------------------------------
# Setup launcher

if(WIN32 AND NOT WITH_PYTHON_MODULE)
	set(LAUNCHER_SRC
		creator_launch_win.c
		../icons/winblender.rc
	)
	add_executable(blender-launcher ${LAUNCHER_SRC})
	target_link_libraries(blender-launcher bf_intern_utfconv ${PLATFORM_LINKLIBS})

	set_target_properties(blender PROPERTIES OUTPUT_NAME blender-app)
	set_target_properties(blender-launcher PROPERTIES OUTPUT_NAME blender)
endif()
