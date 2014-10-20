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

#-----------------------------------------------------------------------------
# We don't allow in-source builds. This causes no end of troubles because
# all out-of-source builds will use the CMakeCache.txt file there and even
# build the libs and objects in it.

if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
	if(NOT DEFINED WITH_IN_SOURCE_BUILD)
		message(FATAL_ERROR
			"CMake generation for blender is not allowed within the source directory!"
			"\n Remove the CMakeCache.txt file and try again from another folder, e.g.:"
			"\n "
			"\n rm CMakeCache.txt"
			"\n cd .."
			"\n mkdir cmake-make"
			"\n cd cmake-make"
			"\n cmake ../blender"
			"\n "
			"\n Alternately define WITH_IN_SOURCE_BUILD to force this option (not recommended!)"
		)
	endif()
endif()

cmake_minimum_required(VERSION 2.8)

if(NOT (${CMAKE_VERSION} VERSION_LESS 3.0))
	# keep until CMake-3.0 is min requirement
	cmake_policy(SET CMP0043 OLD)
endif()

if(NOT EXECUTABLE_OUTPUT_PATH)
	set(FIRST_RUN "TRUE")
endif()

# this starts out unset
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/build_files/cmake/Modules")

# avoid having empty buildtype
set(CMAKE_BUILD_TYPE_INIT "Release")

# quiet output for Makefiles, 'make -s' helps too
# set_property(GLOBAL PROPERTY RULE_MESSAGES OFF)

# global compile definitions since add_definitions() adds for all.
set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS_DEBUG           DEBUG _DEBUG)
set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS_RELEASE         NDEBUG)
set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS_MINSIZEREL      NDEBUG)
set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS_RELWITHDEBINFO  NDEBUG)


#-----------------------------------------------------------------------------
# Set policy

# see "cmake --help-policy CMP0003"
# So library linking is more sane
cmake_policy(SET CMP0003 NEW)

# So BUILDINFO and BLENDERPATH strings are automatically quoted
cmake_policy(SET CMP0005 NEW)

# So syntax problems are errors
cmake_policy(SET CMP0010 NEW)

# Input directories must have CMakeLists.txt
cmake_policy(SET CMP0014 NEW)

#-----------------------------------------------------------------------------
# Load some macros.
include(build_files/cmake/macros.cmake)


#-----------------------------------------------------------------------------
# Initialize project.

blender_project_hack_pre()

project(Blender)

blender_project_hack_post()

enable_testing()

#-----------------------------------------------------------------------------
# Redirect output files

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin CACHE INTERNAL "" FORCE)
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/lib CACHE INTERNAL "" FORCE)
set(TESTS_OUTPUT_DIR ${EXECUTABLE_OUTPUT_PATH}/tests CACHE INTERNAL "" FORCE)

#-----------------------------------------------------------------------------
# Set default config options

get_blender_version()


# Blender internal features
option(WITH_BLENDER "Build blender (disable to build only the blender player)" ON)
mark_as_advanced(WITH_BLENDER)

option(WITH_INTERNATIONAL "Enable I18N (International fonts and text)" ON)

option(WITH_PYTHON        "Enable Embedded Python API  (only disable for development)" ON)
option(WITH_PYTHON_SECURITY "Disables execution of scripts within blend files by default" ON) 
mark_as_advanced(WITH_PYTHON)  # dont want people disabling this unless they really know what they are doing.
mark_as_advanced(WITH_PYTHON_SECURITY)  # some distributions see this as a security issue, rather than have them patch it, make a build option.
set(WITH_PYTHON_SECURITY ON CACHE BOOL "ON" FORCE) # temp force on. 

option(WITH_PYTHON_SAFETY "Enable internal API error checking to track invalid data to prevent crash on access (at the expense of some effeciency, only enable for development)." OFF)
option(WITH_PYTHON_MODULE "Enable building as a python module which runs without a user interface, like running regular blender in background mode (experimental, only enable for development)" OFF)
if(APPLE)
	option(WITH_PYTHON_FRAMEWORK "Enable building using the Python available in the framework (OSX only)" OFF)
endif()

option(WITH_BUILDINFO     "Include extra build details (only disable for development & faster builds)" ON)
if(${CMAKE_VERSION} VERSION_LESS 2.8.8)
	# add_library OBJECT arg unsupported
	set(WITH_BUILDINFO OFF)
endif()

option(WITH_IK_ITASC      "Enable ITASC IK solver (only disable for development & for incompatible C++ compilers)" ON)
option(WITH_IK_SOLVER     "Enable Legacy IK solver (only disable for development)" ON)
option(WITH_FFTW3         "Enable FFTW3 support (Used for smoke and audio effects)" ON)
option(WITH_BULLET        "Enable Bullet (Physics Engine)" ON)
option(WITH_SYSTEM_BULLET "Use the systems bullet library (currently unsupported due to missing features in upstream!)" )
mark_as_advanced(WITH_SYSTEM_BULLET)
option(WITH_GAMEENGINE    "Enable Game Engine" ON)
option(WITH_PLAYER        "Build Player" OFF)
option(WITH_OPENCOLORIO   "Enable OpenColorIO color management" ON)
option(WITH_COMPOSITOR    "Enable the tile based nodal compositor" ON)

# GHOST Windowing Library Options
option(WITH_GHOST_DEBUG   "Enable debugging output for the GHOST library" OFF)
mark_as_advanced(WITH_GHOST_DEBUG)

option(WITH_GHOST_SDL    "Enable building blender against SDL for windowing rather then the native APIs" OFF)
mark_as_advanced(WITH_GHOST_SDL)

# Misc...
option(WITH_HEADLESS      "Build without graphical support (renderfarm, server mode only)" OFF)
mark_as_advanced(WITH_HEADLESS)

option(WITH_AUDASPACE    "Build with blenders audio library (only disable if you know what you're doing!)" ON)
mark_as_advanced(WITH_AUDASPACE)

option(WITH_OPENMP        "Enable OpenMP (has to be supported by the compiler)" ON)

if(UNIX AND NOT APPLE)
	set(WITH_X11 ON)
endif()

if(WITH_X11)
	option(WITH_X11_XINPUT    "Enable X11 Xinput (tablet support and unicode input)"  ON)
	option(WITH_X11_XF86VMODE "Enable X11 video mode switching"                       ON)
	option(WITH_GHOST_XDND    "Enable drag'n'drop support on X11 using XDND protocol" ON)
endif()

if(UNIX AND NOT APPLE)
	option(WITH_SYSTEM_GLEW "Use GLEW OpenGL wrapper library provided by the operating system" ON)
	option(WITH_SYSTEM_GLES "Use OpenGL ES library provided by the operating system"           ON)
else()
	# not an option for other OS's
	set(WITH_SYSTEM_GLEW OFF)
	set(WITH_SYSTEM_GLES OFF)
endif()


# (unix defaults to System OpenJPEG On)
if(UNIX AND NOT APPLE)
	set(PLATFORM_DEFAULT ON)
else()
	set(PLATFORM_DEFAULT OFF)
endif()
option(WITH_SYSTEM_OPENJPEG "Use the operating systems OpenJPEG library" ${PLATFORM_DEFAULT})
unset(PLATFORM_DEFAULT)


# Modifiers
option(WITH_MOD_FLUID           "Enable Elbeem Modifier (Fluid Simulation)" ON)
option(WITH_MOD_SMOKE           "Enable Smoke Modifier (Smoke Simulation)" ON)
option(WITH_MOD_BOOLEAN         "Enable Boolean Modifier" ON)
option(WITH_MOD_REMESH          "Enable Remesh Modifier" ON)
# option(WITH_MOD_CLOTH_ELTOPO    "Enable Experimental cloth solver" OFF)  # this is now only available in a branch
# mark_as_advanced(WITH_MOD_CLOTH_ELTOPO)
option(WITH_MOD_OCEANSIM        "Enable Ocean Modifier" OFF)

# Image format support
option(WITH_OPENIMAGEIO         "Enable OpenImageIO Support (http://www.openimageio.org)" OFF)
option(WITH_IMAGE_OPENEXR       "Enable OpenEXR Support (http://www.openexr.com)" ON)
option(WITH_IMAGE_OPENJPEG      "Enable OpenJpeg Support (http://www.openjpeg.org)" ON)
option(WITH_IMAGE_TIFF          "Enable LibTIFF Support" ON)
option(WITH_IMAGE_DDS           "Enable DDS Image Support" ON)
option(WITH_IMAGE_CINEON        "Enable CINEON and DPX Image Support" ON)
option(WITH_IMAGE_HDR           "Enable HDR Image Support" ON)
option(WITH_IMAGE_REDCODE       "Enable RedCode Image Support" OFF)
option(WITH_IMAGE_FRAMESERVER   "Enable image FrameServer Support for rendering" ON)

# Audio/Video format support
option(WITH_CODEC_AVI           "Enable Blenders own AVI file support (raw/jpeg)" ON)
option(WITH_CODEC_FFMPEG        "Enable FFMPeg Support (http://ffmpeg.org)" OFF)
option(WITH_CODEC_SNDFILE       "Enable libsndfile Support (http://www.mega-nerd.com/libsndfile)" OFF)

if(APPLE)
	option(WITH_CODEC_FFMPEG        "Enable FFMPeg Support (http://ffmpeg.org)" ON)
	option(WITH_CODEC_QUICKTIME     "Enable Quicktime Support" ON)
else()
	option(WITH_CODEC_FFMPEG        "Enable FFMPeg Support (http://ffmpeg.org)" OFF)
endif()

# 3D format support
# Disable opencollada when we don't have precompiled libs
if(APPLE OR WIN32)
	option(WITH_OPENCOLLADA		"Enable OpenCollada Support (http://www.opencollada.org)" ON)
else()
	option(WITH_OPENCOLLADA		"Enable OpenCollada Support (http://www.opencollada.org)" OFF)
endif()

# Sound output
option(WITH_SDL           "Enable SDL for sound and joystick support" ON)
option(WITH_OPENAL        "Enable OpenAL Support (http://www.openal.org)" ON)
option(WITH_JACK          "Enable Jack Support (http://www.jackaudio.org)" OFF)
if(UNIX AND NOT APPLE)
	option(WITH_JACK_DYNLOAD  "Enable runtime dynamic Jack libraries loading" OFF)
endif()

# Compression
option(WITH_LZO           "Enable fast LZO compression (used for pointcache)" ON)
option(WITH_LZMA          "Enable best LZMA compression, (used for pointcache)" ON)

# Camera/motion tracking
option(WITH_LIBMV         "Enable libmv structure from motion library" ON)
option(WITH_LIBMV_SCHUR_SPECIALIZATIONS "Enable fixed-size schur specializations." ON)

# Freestyle
option(WITH_FREESTYLE     "Enable Freestyle (advanced edges rendering)" ON)

# Misc
option(WITH_INPUT_NDOF "Enable NDOF input devices (SpaceNavigator and friends)" ON)
option(WITH_RAYOPTIMIZATION	"Enable use of SIMD (SSE) optimizations for the raytracer" ON)
option(WITH_OPENNL        "Enable use of Open Numerical Library" ON)
if(UNIX AND NOT APPLE)
	option(WITH_INSTALL_PORTABLE "Install redistributeable runtime, otherwise install into CMAKE_INSTALL_PREFIX" ON)
	option(WITH_STATIC_LIBS "Try to link with static libraries, as much as possible, to make blender more portable across distributions" OFF)
	if(WITH_STATIC_LIBS)
		option(WITH_BOOST_ICU "Boost uses ICU library (required for linking with static Boost built with libicu)." OFF)
		mark_as_advanced(WITH_BOOST_ICU)
	endif()
endif()
option(WITH_PYTHON_INSTALL       "Copy system python into the blender install folder" ON)
option(WITH_PYTHON_INSTALL_NUMPY "Copy system numpy into the blender install folder"  ON)
set(PYTHON_NUMPY_PATH            "" CACHE PATH "Path to python site-packages or dist-packages containing 'numpy' module")
mark_as_advanced(PYTHON_NUMPY_PATH)
option(WITH_CPU_SSE              "Enable SIMD instruction if they're detected on the host machine" ON)
mark_as_advanced(WITH_CPU_SSE)

if(UNIX AND NOT APPLE)
	option(WITH_PYTHON_INSTALL_REQUESTS "Copy system requests into the blender install folder" ON)
	set(PYTHON_REQUESTS_PATH "" CACHE PATH "Path to python site-packages or dist-packages containing 'requests' module")
	mark_as_advanced(PYTHON_REQUESTS_PATH)
endif()

# Cycles
option(WITH_CYCLES					"Enable cycles Render Engine" ON)
option(WITH_CYCLES_STANDALONE		"Build cycles standalone application" OFF)
option(WITH_CYCLES_STANDALONE_GUI	"Build cycles standalone with GUI" OFF)
option(WITH_CYCLES_OSL				"Build Cycles with OSL support" OFF)
option(WITH_CYCLES_CUDA_BINARIES	"Build cycles CUDA binaries" OFF)
set(CYCLES_CUDA_BINARIES_ARCH sm_20 sm_21 sm_30 sm_35 sm_50 CACHE STRING "CUDA architectures to build binaries for")
mark_as_advanced(CYCLES_CUDA_BINARIES_ARCH)
unset(PLATFORM_DEFAULT)
option(WITH_CYCLES_LOGGING	"Build cycles with logging support" OFF)
option(WITH_CYCLES_DEBUG	"Build cycles with with extra debug capabilties" OFF)
mark_as_advanced(WITH_CYCLES_LOGGING)
mark_as_advanced(WITH_CYCLES_DEBUG)

# LLVM
option(WITH_LLVM					"Use LLVM" OFF)
if(APPLE)
	option(LLVM_STATIC					"Link with LLVM static libraries" ON) # we prefer static llvm build on Apple, dyn build possible though
else()
	option(LLVM_STATIC					"Link with LLVM static libraries" OFF)
endif()
mark_as_advanced(LLVM_STATIC)

# disable for now, but plan to support on all platforms eventually
option(WITH_MEM_JEMALLOC   "Enable malloc replacement (http://www.canonware.com/jemalloc)" OFF)
mark_as_advanced(WITH_MEM_JEMALLOC)

# currently only used for BLI_mempool
option(WITH_MEM_VALGRIND "Enable extended valgrind support for better reporting" OFF)
mark_as_advanced(WITH_MEM_VALGRIND)

# Debug
option(WITH_CXX_GUARDEDALLOC "Enable GuardedAlloc for C++ memory allocation tracking (only enable for development)" OFF)
mark_as_advanced(WITH_CXX_GUARDEDALLOC)

option(WITH_ASSERT_ABORT "Call abort() when raising an assertion through BLI_assert()" OFF)
mark_as_advanced(WITH_ASSERT_ABORT)

option(WITH_BOOST					"Enable features depending on boost" ON)

if(CMAKE_COMPILER_IS_GNUCC)
	option(WITH_GCC_MUDFLAP			"Enable mudflap" OFF)
	mark_as_advanced(WITH_GCC_MUDFLAP)
endif()

# Unit testsing
option(WITH_GTESTS "Enable GTest unit testing" OFF)

# OpenGL

option(WITH_GLEW_MX             "Support multiple GLEW contexts (experimental)"                                                                     OFF )
option(WITH_GLEW_ES             "Switches to experimental copy of GLEW that has support for OpenGL ES. (temporary option for development purposes)" OFF)
option(WITH_GL_EGL              "Use the EGL OpenGL system library instead of the platform specific OpenGL system library (CGL, glX, or WGL)"       OFF)
option(WITH_GL_PROFILE_COMPAT   "Support using the OpenGL 'compatibility' profile. (deprecated)"                                                    ON )
option(WITH_GL_PROFILE_CORE     "Support using the OpenGL 3.2+ 'core' profile."                                                                     OFF)
option(WITH_GL_PROFILE_ES20     "Support using OpenGL ES 2.0. (thru either EGL or the AGL/WGL/XGL 'es20' profile)"                                  OFF)

mark_as_advanced(
	WITH_GLEW_MX
	WITH_GLEW_ES
	WITH_GL_EGL
	WITH_GL_PROFILE_COMPAT
	WITH_GL_PROFILE_CORE
	WITH_GL_PROFILE_ES20
)

if (WITH_GL_PROFILE_COMPAT)
	set(WITH_GLU ON)
else()
	set(WITH_GLU OFF)
endif()

if(WIN32)
	option(WITH_GL_ANGLE "Link with the ANGLE library, an OpenGL ES 2.0 implementation based on Direct3D, instead of the system OpenGL library." OFF)
	mark_as_advanced(WITH_GL_ANGLE)
endif()

if(WITH_GLEW_ES AND WITH_SYSTEM_GLEW)
	message(WARNING Ignoring WITH_SYSTEM_GLEW and using WITH_GLEW_ES)
	set(WITH_SYSTEM_GLEW OFF)
endif()

# Apple

if(APPLE)
	if(${CMAKE_VERSION} VERSION_LESS 3.0) # else breaks setting CMP0043 policy
		cmake_minimum_required(VERSION 2.8.8)
		cmake_policy(VERSION 2.8.8)
	endif()

	if(NOT CMAKE_OSX_ARCHITECTURES)
		set(CMAKE_OSX_ARCHITECTURES x86_64 CACHE STRING
		"Choose the architecture you want to build Blender for: i386, x86_64 or ppc"
		FORCE)
	endif()
	
	execute_process(COMMAND uname -r OUTPUT_VARIABLE MAC_SYS) # check for actual system-version
	if(${MAC_SYS} MATCHES 14)
		set(OSX_SYSTEM 10.10)
		# throw an error here, older cmake cannot handle 2 digit subversion!
		cmake_minimum_required(VERSION 3.0.0)
	elseif(${MAC_SYS} MATCHES 13)
		set(OSX_SYSTEM 10.9)
	elseif(${MAC_SYS} MATCHES 12)
		set(OSX_SYSTEM 10.8)
	elseif(${MAC_SYS} MATCHES 11)
		set(OSX_SYSTEM 10.7)
	elseif(${MAC_SYS} MATCHES 10)
		set(OSX_SYSTEM 10.6)
	elseif(${MAC_SYS} MATCHES 9)
		set(OSX_SYSTEM 10.5)
	else()
		set(OSX_SYSTEM unsupported)
	endif()
	message(STATUS "Detected system-version: " ${OSX_SYSTEM})
	
	# workaround for incorrect cmake xcode lookup for developer previews - XCODE_VERSION does not take xcode-select path into accout
	# but would always look into /Applications/Xcode.app while dev versions are named Xcode<version>-DP<preview_number>
	execute_process(COMMAND xcode-select --print-path  OUTPUT_VARIABLE XCODE_CHECK OUTPUT_STRIP_TRAILING_WHITESPACE)
	string(REPLACE "/Contents/Developer" "" XCODE_BUNDLE ${XCODE_CHECK}) # truncate to bundlepath in any case
	message(STATUS "Xcode-bundle : " ${XCODE_BUNDLE})
	
	if(${CMAKE_GENERATOR} MATCHES "Xcode")
	
		if(${XCODE_VERSION} VERSION_GREATER 4.2) # earlier xcode has no bundled developer dir, no sense in getting xcode path from
			string(SUBSTRING "${XCODE_CHECK}" 14 6 DP_NAME) # reduce to XCode name without dp extension
			if(${DP_NAME} MATCHES Xcode5)
				set(XCODE_VERSION 5)
			endif()
		endif()

		##### cmake incompatibility with xcode  4.3 and higher #####
		if(${XCODE_VERSION} MATCHES '') # cmake fails due looking for xcode in the wrong path, thus will be empty var
			message(FATAL_ERROR "Xcode 4.3 and higher must be used with cmake 2.8-8 or higher")
		endif()
		### end cmake incompatibility with xcode 4.3 and higher ###
		
		if(${XCODE_VERSION} VERSION_EQUAL 4 OR ${XCODE_VERSION} VERSION_GREATER 4 AND ${XCODE_VERSION} VERSION_LESS 4.3)
			# Xcode 4 defaults to the Apple LLVM Compiler.
			# Override the default compiler selection because Blender only compiles with gcc up to xcode 4.2
			set(CMAKE_XCODE_ATTRIBUTE_GCC_VERSION "com.apple.compilers.llvmgcc42")
			message(STATUS "Setting compiler to: " ${CMAKE_XCODE_ATTRIBUTE_GCC_VERSION})
		endif()
	else() # unix makefile generator does not fill XCODE_VERSION var, so we get it with a command
		execute_process(COMMAND xcodebuild -version OUTPUT_VARIABLE XCODE_VERS_BUILD_NR)
		string(SUBSTRING "${XCODE_VERS_BUILD_NR}" 6 3 XCODE_VERSION) # truncate away build-nr
		unset(XCODE_VERS_BUILD_NR)
	endif()
	
	message(STATUS "Detected Xcode-version: " ${XCODE_VERSION})

	if(${XCODE_VERSION} VERSION_LESS 4.3)
		set(CMAKE_OSX_SYSROOT /Developer/SDKs/MacOSX${OSX_SYSTEM}.sdk CACHE PATH "" FORCE)  # use guaranteed existing sdk
	else()
		# note: xcode-select path could be ambigous, cause /Applications/Xcode.app/Contents/Developer or /Applications/Xcode.app would be allowed
		# so i use a selfcomposed bundlepath here  
		set(OSX_SYSROOT_PREFIX ${XCODE_BUNDLE}/Contents/Developer/Platforms/MacOSX.platform)
		message(STATUS "OSX_SYSROOT_PREFIX: " ${OSX_SYSROOT_PREFIX})
		set(OSX_DEVELOPER_PREFIX /Developer/SDKs/MacOSX${OSX_SYSTEM}.sdk) # use guaranteed existing sdk
		set(CMAKE_OSX_SYSROOT ${OSX_SYSROOT_PREFIX}/${OSX_DEVELOPER_PREFIX} CACHE PATH "" FORCE)
		if(${CMAKE_GENERATOR} MATCHES "Xcode")
			set(CMAKE_XCODE_ATTRIBUTE_SDKROOT macosx${OSX_SYSTEM}) # to silence sdk not found warning, just overrides CMAKE_OSX_SYSROOT
		endif()
	endif()

	if(OSX_SYSTEM MATCHES 10.9)
		set(CMAKE_FIND_ROOT_PATH ${CMAKE_OSX_SYSROOT}) # make sure syslibs and headers are looked up in sdk ( expecially for 10.9 openGL atm. )
	endif()

	if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
		set(CMAKE_OSX_DEPLOYMENT_TARGET "10.6" CACHE STRING "" FORCE) # 10.6 is our min. target, if you use higher sdk, weak linking happens
	endif()
	
	if(NOT ${CMAKE_GENERATOR} MATCHES "Xcode")
		# force CMAKE_OSX_DEPLOYMENT_TARGET for makefiles, will not work else ( cmake bug ? )
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}")
		add_definitions("-DMACOSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}")
	endif()

	option(WITH_LIBS10.5  "Use 10.5 libs (needed for 64bit builds)" OFF)
endif()


#-----------------------------------------------------------------------------
# Check for conflicting/unsupported configurations

if(NOT WITH_BLENDER AND NOT WITH_PLAYER AND NOT WITH_CYCLES_STANDALONE)
	message(FATAL_ERROR "At least one of WITH_BLENDER or WITH_PLAYER or WITH_CYCLES_STANDALONE must be enabled, nothing to do!")
endif()

if(NOT WITH_GAMEENGINE AND WITH_PLAYER)
	message(FATAL_ERROR "WITH_PLAYER requires WITH_GAMEENGINE")
endif()

if(NOT WITH_AUDASPACE)
	if(WITH_OPENAL)
		message(FATAL_ERROR "WITH_OPENAL requires WITH_AUDASPACE")
	endif()
	if(WITH_JACK)
		message(FATAL_ERROR "WITH_JACK requires WITH_AUDASPACE")
	endif()
	if(WITH_GAMEENGINE)
		message(FATAL_ERROR "WITH_GAMEENGINE requires WITH_AUDASPACE")
	endif()
endif()

if(NOT WITH_SDL AND WITH_GHOST_SDL)
	message(FATAL_ERROR "WITH_GHOST_SDL requires WITH_SDL")
endif()

if(WITH_IMAGE_REDCODE AND ((NOT WITH_IMAGE_OPENJPEG) OR (NOT WITH_CODEC_FFMPEG)))
	message(FATAL_ERROR "WITH_IMAGE_REDCODE requires WITH_IMAGE_OPENJPEG and WITH_CODEC_FFMPEG")
endif()

# python module, needs some different options
if(WITH_PYTHON_MODULE AND WITH_PLAYER)
	message(FATAL_ERROR "WITH_PYTHON_MODULE requires WITH_PLAYER to be OFF")
endif()

if(WITH_PYTHON_MODULE AND WITH_PYTHON_INSTALL)
	message(FATAL_ERROR "WITH_PYTHON_MODULE requires WITH_PYTHON_INSTALL to be OFF")
endif()


# may as well build python module without a UI
if(WITH_PYTHON_MODULE)
	set(WITH_HEADLESS ON)
endif()

if(NOT WITH_PYTHON)
	set(WITH_CYCLES OFF)
endif()

# enable boost for cycles, booleans, audaspace or i18n
# otherwise if the user disabled
if(NOT WITH_BOOST)
	# Explicitly disabled. so disable all deps.
	macro(set_and_warn
		_setting _val)
		if(${${_setting}})
			message(STATUS "'WITH_BOOST' is disabled: forceing 'set(${_setting} ${_val})'")
		endif()
		set(${_setting} ${_val})
	endmacro()

	set_and_warn(WITH_CYCLES         OFF)
	set_and_warn(WITH_MOD_BOOLEAN    OFF)
	set_and_warn(WITH_AUDASPACE      OFF)
	set_and_warn(WITH_INTERNATIONAL  OFF)

	set_and_warn(WITH_OPENAL         OFF)  # depends on AUDASPACE
	set_and_warn(WITH_GAMEENGINE     OFF)  # depends on AUDASPACE
elseif(WITH_CYCLES OR WITH_OPENIMAGEIO OR WITH_MOD_BOOLEAN OR WITH_AUDASPACE OR WITH_INTERNATIONAL)
	# Keep enabled
else()
	# Enabled but we don't need it
	set(WITH_BOOST OFF)
endif()

# auto enable openimageio for cycles
if(WITH_CYCLES)
	set(WITH_OPENIMAGEIO ON)
endif()

# auto enable openimageio linking dependencies
if(WITH_OPENIMAGEIO)
	set(WITH_IMAGE_OPENEXR ON)
	set(WITH_IMAGE_TIFF ON)
endif()

# auto enable llvm for cycles_osl
if(WITH_CYCLES_OSL)
	set(WITH_LLVM ON CACHE BOOL "ON" FORCE)
endif()

# don't store paths to libs for portable distribution
if(WITH_INSTALL_PORTABLE)
	set(CMAKE_SKIP_BUILD_RPATH TRUE)
endif()

if(WITH_GHOST_SDL OR WITH_HEADLESS)
	set(WITH_X11           OFF)
	set(WITH_X11_XINPUT    OFF)
	set(WITH_X11_XF86VMODE OFF)
	set(WITH_GHOST_XDND    OFF)
endif()

if(WITH_CPU_SSE)
	TEST_SSE_SUPPORT(COMPILER_SSE_FLAG COMPILER_SSE2_FLAG)
else()
	message(STATUS "SSE and SSE2 optimizations are DISABLED!")
	set(COMPILER_SSE_FLAG)
	set(COMPILER_SSE2_FLAG)
endif()

TEST_STDBOOL_SUPPORT()
if(HAVE_STDBOOL_H)
	add_definitions(-DHAVE_STDBOOL_H)
endif()

#-----------------------------------------------------------------------------
# Check for valid directories
# ... a partial checkout may cause this.
#
# note: we need to check for a known subdir in both cases.
#       since uninitialized git submodules will give blank dirs

if(WITH_INTERNATIONAL)
	if(NOT EXISTS "${CMAKE_SOURCE_DIR}/release/datafiles/locale/languages")
		message(WARNING "Translation path '${CMAKE_SOURCE_DIR}/release/datafiles/locale' is missing, "
						"This is a 'git submodule', which are known not to work with bridges to other version "
						"control systems, disabling 'WITH_INTERNATIONAL'.")
		set(WITH_INTERNATIONAL OFF)
	endif()
endif()

if(WITH_PYTHON)
	if(NOT EXISTS "${CMAKE_SOURCE_DIR}/release/scripts/addons/modules")
		message(WARNING "Addons path '${CMAKE_SOURCE_DIR}/release/scripts/addons' is missing, "
						"This is a 'git submodule', which are known not to work with bridges to other version "
						"control systems: * CONTINUING WITHOUT ADDONS *")
	endif()
endif()

#-----------------------------------------------------------------------------
# Initialize un-cached vars, avoid unused warning

# linux only, not cached
set(WITH_BINRELOC OFF)

# MAXOSX only, set to avoid uninitialized
set(EXETYPE "")

# C/C++ flags
set(PLATFORM_CFLAGS)

# these are added to later on.
set(C_WARNINGS)
set(CXX_WARNINGS)

# for gcc -Wno-blah-blah
set(CC_REMOVE_STRICT_FLAGS)

# libraries to link the binary with passed to target_link_libraries()
# known as LLIBS to scons
set(PLATFORM_LINKLIBS "")

# Added to linker flags in setup_liblinks
# - CMAKE_EXE_LINKER_FLAGS
# - CMAKE_EXE_LINKER_FLAGS_DEBUG
set(PLATFORM_LINKFLAGS "")
set(PLATFORM_LINKFLAGS_DEBUG "")


# For alternate Python locations the commandline can be used to override detected/default cache settings, e.g:
# On Unix:
#   cmake ../blender \
#         -D PYTHON_VERSION=3.4 \
#         -D PYTHON_INCLUDE_DIR=/opt/py34/include/python3.4d \
#         -D PYTHON_LIBRARY=/opt/py34/lib/libpython3.4d.so
#
# On Macs:
#   cmake ../blender \
#         -D PYTHON_INCLUDE_DIR=/System/Library/Frameworks/Python.framework/Versions/3.4/include/python3.4 \
#         -D PYTHON_LIBPATH=/System/Library/Frameworks/Python.framework/Versions/3.4/lib/python3.4/config \
#         -G Xcode
#
# When changing any of this remember to update the notes in doc/build_systems/cmake.txt

#-----------------------------------------------------------------------------
#Platform specifics

if(WITH_X11)
	find_package(X11 REQUIRED)

	find_path(X11_XF86keysym_INCLUDE_PATH X11/XF86keysym.h ${X11_INC_SEARCH_PATH})
	mark_as_advanced(X11_XF86keysym_INCLUDE_PATH)

	list(APPEND PLATFORM_LINKLIBS ${X11_X11_LIB})

	if(WITH_X11_XINPUT)
		if(X11_Xinput_LIB)
			list(APPEND PLATFORM_LINKLIBS ${X11_Xinput_LIB})
		else()
			set(WITH_X11_XINPUT OFF)
		endif()
	endif()

	if(WITH_X11_XF86VMODE)
		# XXX, why dont cmake make this available?
		find_library(X11_Xxf86vmode_LIB Xxf86vm   ${X11_LIB_SEARCH_PATH})
		mark_as_advanced(X11_Xxf86vmode_LIB)
		if(X11_Xxf86vmode_LIB)
			list(APPEND PLATFORM_LINKLIBS ${X11_Xxf86vmode_LIB})
		else()
			set(WITH_X11_XF86VMODE OFF)
		endif()
	endif()

endif()

if(UNIX AND NOT APPLE)
	macro(find_package_wrapper)
		string(TOUPPER ${ARGV0} _NAME_UPPER)
		if(WITH_STATIC_LIBS)
			set(_cmake_find_library_suffixes_back ${CMAKE_FIND_LIBRARY_SUFFIXES})
			set(CMAKE_FIND_LIBRARY_SUFFIXES .a ${CMAKE_FIND_LIBRARY_SUFFIXES})
		endif()
		find_package(${ARGV})
		if(WITH_STATIC_LIBS)
			set(CMAKE_FIND_LIBRARY_SUFFIXES ${_cmake_find_library_suffixes_back})
			unset(_cmake_find_library_suffixes_back)
		endif()
	endmacro()

	find_package_wrapper(JPEG REQUIRED)
	find_package_wrapper(PNG REQUIRED)
	find_package_wrapper(ZLIB REQUIRED)
	find_package_wrapper(Freetype REQUIRED)

	if(WITH_PYTHON)
		# No way to set py34. remove for now.
		# find_package(PythonLibs)

		# Use our own instead, since wothout py is such a rare case,
		# require this package
		# XXX Linking errors with debian static python :/
#		find_package_wrapper(PythonLibsUnix REQUIRED)
		find_package(PythonLibsUnix REQUIRED)
	endif()

	if(WITH_IMAGE_OPENEXR)
		find_package_wrapper(OpenEXR)  # our own module
		if(NOT OPENEXR_FOUND)
			set(WITH_IMAGE_OPENEXR OFF)
		endif()
	endif()

	if(WITH_IMAGE_OPENJPEG)
		find_package_wrapper(OpenJPEG)
		if(NOT OPENJPEG_FOUND)
			set(WITH_IMAGE_OPENJPEG OFF)
		endif()
	endif()

	if(WITH_IMAGE_TIFF)
		# XXX Linking errors with debian static tiff :/
#		find_package_wrapper(TIFF)
		find_package(TIFF)
		if(NOT TIFF_FOUND)
			set(WITH_IMAGE_TIFF OFF)
		endif()
	endif()

	# Audio IO
	if(WITH_OPENAL)
		find_package_wrapper(OpenAL)
		if(NOT OPENAL_FOUND)
			set(WITH_OPENAL OFF)
		endif()
	endif()

	if(WITH_SDL)
		find_package_wrapper(SDL)
		mark_as_advanced(
			SDLMAIN_LIBRARY
			SDL_INCLUDE_DIR
			SDL_LIBRARY
			SDL_LIBRARY_TEMP
		)
		# unset(SDLMAIN_LIBRARY CACHE)
		if(NOT SDL_FOUND)
			set(WITH_SDL OFF)
		endif()
	endif()

	if(WITH_JACK)
		find_package_wrapper(Jack)
		if(NOT JACK_FOUND)
			set(WITH_JACK OFF)
		endif()
	endif()

	# Codecs
	if(WITH_CODEC_SNDFILE)
		find_package_wrapper(SndFile)
		if(NOT SNDFILE_FOUND)
			set(WITH_CODEC_SNDFILE OFF)
		endif()
	endif()

	if(WITH_CODEC_FFMPEG)
		set(FFMPEG /usr CACHE PATH "FFMPEG Directory")
		set(FFMPEG_LIBRARIES avformat avcodec avutil avdevice swscale CACHE STRING "FFMPEG Libraries")

		mark_as_advanced(FFMPEG)

		# lame, but until we have proper find module for ffmpeg
		set(FFMPEG_INCLUDE_DIRS ${FFMPEG}/include)
		if(EXISTS "${FFMPEG}/include/ffmpeg/")
			list(APPEND FFMPEG_INCLUDE_DIRS "${FFMPEG}/include/ffmpeg")
		endif()
		# end lameness

		mark_as_advanced(FFMPEG_LIBRARIES)
		set(FFMPEG_LIBPATH ${FFMPEG}/lib)
	endif()

	if(WITH_FFTW3)
		find_package_wrapper(Fftw3)
		if(NOT FFTW3_FOUND)
			set(WITH_FFTW3 OFF)
		endif()
	endif()

	if(WITH_OPENCOLLADA)
		find_package_wrapper(OpenCOLLADA)
		if(OPENCOLLADA_FOUND)
			find_package_wrapper(XML2)
			find_package_wrapper(PCRE)
		else()
			set(WITH_OPENCOLLADA OFF)
		endif()
	endif()

	if(WITH_MEM_JEMALLOC)
		find_package_wrapper(JeMalloc)
		if(NOT JEMALLOC_FOUND)
			set(WITH_MEM_JEMALLOC OFF)
		endif()
	endif()

	if(WITH_INPUT_NDOF)
		find_package_wrapper(Spacenav)
		if(NOT SPACENAV_FOUND)
			set(WITH_INPUT_NDOF OFF)
		endif()

		# use generic names within blenders buildsystem.
		if(SPACENAV_FOUND)
			set(NDOF_INCLUDE_DIRS ${SPACENAV_INCLUDE_DIRS})
			set(NDOF_LIBRARIES ${SPACENAV_LIBRARIES})
		endif()
	endif()

	if(WITH_BOOST)
		# uses in build instructions to override include and library variables
		if(NOT BOOST_CUSTOM)
			if(WITH_STATIC_LIBS)
				set(Boost_USE_STATIC_LIBS ON)
			endif()
			set(Boost_USE_MULTITHREADED ON)
			set(__boost_packages filesystem regex system thread date_time)
			if(WITH_INTERNATIONAL)
				list(APPEND __boost_packages locale)
			endif()
			if(WITH_CYCLES_NETWORK)
				list(APPEND __boost_packages serialization)
			endif()
			find_package(Boost 1.48 COMPONENTS ${__boost_packages})
			if(NOT Boost_FOUND)
				# try to find non-multithreaded if -mt not found, this flag
				# doesn't matter for us, it has nothing to do with thread
				# safety, but keep it to not disturb build setups
				set(Boost_USE_MULTITHREADED OFF)
				find_package(Boost 1.48 COMPONENTS ${__boost_packages})
			endif()
			unset(__boost_packages)
			if(Boost_USE_STATIC_LIBS AND WITH_BOOST_ICU)
				find_package(IcuLinux)
			endif()
			mark_as_advanced(Boost_DIR)  # why doesnt boost do this?
		endif()

		set(BOOST_INCLUDE_DIR ${Boost_INCLUDE_DIRS})
		set(BOOST_LIBRARIES ${Boost_LIBRARIES})
		set(BOOST_LIBPATH ${Boost_LIBRARY_DIRS})
		set(BOOST_DEFINITIONS "-DBOOST_ALL_NO_LIB")
	endif()

	if(WITH_OPENIMAGEIO)
		find_package_wrapper(OpenImageIO)

		set(OPENIMAGEIO_LIBRARIES ${OPENIMAGEIO_LIBRARIES} ${PNG_LIBRARIES} ${JPEG_LIBRARIES} ${ZLIB_LIBRARIES} ${BOOST_LIBRARIES})
		set(OPENIMAGEIO_LIBPATH)  # TODO, remove and reference the absolute path everywhere
		set(OPENIMAGEIO_DEFINITIONS "")

		if(WITH_IMAGE_TIFF)
			list(APPEND OPENIMAGEIO_LIBRARIES "${TIFF_LIBRARY}")
		endif()
		if(WITH_IMAGE_OPENEXR)
			list(APPEND OPENIMAGEIO_LIBRARIES "${OPENEXR_LIBRARIES}")
		endif()

		if(NOT OPENIMAGEIO_FOUND)
			set(WITH_OPENIMAGEIO OFF)
			message(STATUS "OpenImageIO not found, disabling WITH_CYCLES")
		endif()
	endif()

	if(WITH_OPENCOLORIO)
		find_package_wrapper(OpenColorIO)

		set(OPENCOLORIO_LIBRARIES ${OPENCOLORIO_LIBRARIES})
		set(OPENCOLORIO_LIBPATH)  # TODO, remove and reference the absolute path everywhere
		set(OPENCOLORIO_DEFINITIONS)

		if(NOT OPENCOLORIO_FOUND)
			set(WITH_OPENCOLORIO OFF)
			message(STATUS "OpenColorIO not found")
		endif()
	endif()

	# XXX Maybe most of this section should go into an llvm module?
	if(WITH_LLVM)
		# Set llvm version if not specified
		if(NOT LLVM_VERSION)
			set(LLVM_VERSION "3.0")
		endif()

		if(LLVM_DIRECTORY)
			find_program(LLVM_CONFIG llvm-config-${LLVM_VERSION} HINTS ${LLVM_DIRECTORY}/bin NO_CMAKE_PATH)
			if(NOT LLVM_CONFIG)
				find_program(LLVM_CONFIG llvm-config HINTS ${LLVM_DIRECTORY}/bin NO_CMAKE_PATH)
			endif()
		else()
			find_program(LLVM_CONFIG llvm-config-${LLVM_VERSION})
			if(NOT LLVM_CONFIG)
				find_program(LLVM_CONFIG llvm-config)
			endif()
		endif()

		if(NOT DEFINED LLVM_VERSION)
			execute_process(COMMAND ${LLVM_CONFIG} --version
							OUTPUT_VARIABLE LLVM_VERSION
							OUTPUT_STRIP_TRAILING_WHITESPACE)
			set(LLVM_VERSION ${LLVM_VERSION} CACHE STRING "Version of LLVM to use")
		endif()
		if(NOT DEFINED LLVM_DIRECTORY)
			execute_process(COMMAND ${LLVM_CONFIG} --prefix
							OUTPUT_VARIABLE LLVM_DIRECTORY
							OUTPUT_STRIP_TRAILING_WHITESPACE)
			set(LLVM_DIRECTORY ${LLVM_DIRECTORY} CACHE PATH "Path to the LLVM installation")
		endif()
		if(NOT DEFINED LLVM_LIBPATH)
			execute_process(COMMAND ${LLVM_CONFIG} --libdir
							OUTPUT_VARIABLE LLVM_LIBPATH
							OUTPUT_STRIP_TRAILING_WHITESPACE)
			set(LLVM_LIBPATH ${LLVM_LIBPATH} CACHE PATH "Path to the LLVM library path")
			mark_as_advanced(LLVM_LIBPATH)
		endif()

		if(LLVM_STATIC)
			find_library(LLVM_LIBRARY
			             NAMES LLVMAnalysis # first of a whole bunch of libs to get
			             PATHS ${LLVM_LIBPATH})
		else()
			find_library(LLVM_LIBRARY
			             NAMES LLVM-${LLVM_VERSION}
			             PATHS ${LLVM_LIBPATH})
		endif()


		if(LLVM_LIBRARY AND LLVM_DIRECTORY AND LLVM_LIBPATH)
			if(LLVM_STATIC)
				# if static LLVM libraries were requested, use llvm-config to generate
				# the list of what libraries we need, and substitute that in the right
				# way for LLVM_LIBRARY.
				execute_process(COMMAND ${LLVM_CONFIG} --libfiles
				                OUTPUT_VARIABLE LLVM_LIBRARY
				                OUTPUT_STRIP_TRAILING_WHITESPACE)
				string(REPLACE " " ";" LLVM_LIBRARY "${LLVM_LIBRARY}")
			endif()
		else()
			message(FATAL_ERROR "LLVM not found.")
		endif()

		# Fix for conflict with Mesa llvmpipe
		set(PLATFORM_LINKFLAGS "${PLATFORM_LINKFLAGS} -Wl,--version-script=${CMAKE_SOURCE_DIR}/source/creator/blender.map")
	endif()

	if(WITH_CYCLES_OSL)
		set(CYCLES_OSL ${LIBDIR}/osl CACHE PATH "Path to OpenShadingLanguage installation")
	
		message(STATUS "CYCLES_OSL = ${CYCLES_OSL}")
	
		find_library(OSL_LIB_EXEC NAMES oslexec PATHS ${CYCLES_OSL}/lib)
		find_library(OSL_LIB_COMP NAMES oslcomp PATHS ${CYCLES_OSL}/lib)
		find_library(OSL_LIB_QUERY NAMES oslquery PATHS ${CYCLES_OSL}/lib)
		# Note: --whole-archive is needed to force loading of all symbols in liboslexec,
		# otherwise LLVM is missing the osl_allocate_closure_component function
		list(APPEND OSL_LIBRARIES ${OSL_LIB_COMP} -Wl,--whole-archive ${OSL_LIB_EXEC} -Wl,--no-whole-archive ${OSL_LIB_QUERY})
		find_path(OSL_INCLUDES OSL/oslclosure.h PATHS ${CYCLES_OSL}/include)
		find_program(OSL_COMPILER NAMES oslc PATHS ${CYCLES_OSL}/bin)
	
		if(OSL_INCLUDES AND OSL_LIBRARIES AND OSL_COMPILER)
			set(OSL_FOUND TRUE)
			message(STATUS "OSL includes = ${OSL_INCLUDES}")
			message(STATUS "OSL library = ${OSL_LIBRARIES}")
			message(STATUS "OSL compiler = ${OSL_COMPILER}")
		else()
			message(STATUS "OSL not found")
		endif()
	endif()

	# OpenSuse needs lutil, ArchLinux not, for now keep, can avoid by using --as-needed
	list(APPEND PLATFORM_LINKLIBS -lutil -lc -lm -lpthread)

	if(CMAKE_SYSTEM_NAME MATCHES "Linux")
		if(NOT WITH_PYTHON_MODULE)
			# binreloc is linux only
			set(BINRELOC_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/extern/binreloc/include)
			set(WITH_BINRELOC ON)
		endif()
	endif()

	set(PLATFORM_LINKFLAGS "${PLATFORM_LINKFLAGS} -pthread")
	if(CMAKE_DL_LIBS)
		list(APPEND PLATFORM_LINKLIBS ${CMAKE_DL_LIBS})
	endif()

	# lfs on glibc, all compilers should use
	add_definitions(-D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE)

	# GNU Compiler
	if(CMAKE_COMPILER_IS_GNUCC)
		set(PLATFORM_CFLAGS "-pipe -fPIC -funsigned-char -fno-strict-aliasing")
	# CLang is the same as GCC for now.
	elseif(CMAKE_C_COMPILER_ID MATCHES "Clang")
		set(PLATFORM_CFLAGS "-pipe -fPIC -funsigned-char -fno-strict-aliasing")
	# Solaris CC
	elseif(CMAKE_C_COMPILER_ID MATCHES "SunPro")
		set(PLATFORM_CFLAGS "-pipe -features=extensions -fPIC -D__FUNCTION__=__func__")
		
	# Intel C++ Compiler
	elseif(CMAKE_C_COMPILER_ID MATCHES "Intel")
		# think these next two are broken
		find_program(XIAR xiar)
		if(XIAR)
			set(CMAKE_AR "${XIAR}")
		endif()
		mark_as_advanced(XIAR)

		find_program(XILD xild)
		if(XILD)
			set(CMAKE_LINKER "${XILD}")
		endif()
		mark_as_advanced(XILD)

		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fp-model precise -prec_div -parallel")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fp-model precise -prec_div -parallel")

		# set(PLATFORM_CFLAGS "${PLATFORM_CFLAGS} -diag-enable sc3")
		set(PLATFORM_CFLAGS "-pipe -fPIC -funsigned-char -fno-strict-aliasing")
		set(PLATFORM_LINKFLAGS "${PLATFORM_LINKFLAGS} -static-intel")
	endif()

elseif(WIN32)

	add_definitions(-DWIN32)

	if(MSVC)

		# needed for some MSVC installations
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SAFESEH:NO")
		set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /SAFESEH:NO")
		set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} /SAFESEH:NO")

		list(APPEND PLATFORM_LINKLIBS ws2_32 vfw32 winmm kernel32 user32 gdi32 comdlg32 advapi32 shfolder shell32 ole32 oleaut32 uuid psapi)

		add_definitions(
			-D_CRT_NONSTDC_NO_DEPRECATE
			-D_CRT_SECURE_NO_DEPRECATE
			-D_SCL_SECURE_NO_DEPRECATE
			-D_CONSOLE
			-D_LIB
		)

		# MSVC11 needs _ALLOW_KEYWORD_MACROS to build
		if(NOT MSVC_VERSION VERSION_LESS 1700)
			add_definitions(-D_ALLOW_KEYWORD_MACROS)
		endif()

		if(CMAKE_CL_64)
			# We want to support Vista level ABI for x64
			if(NOT MSVC_VERSION VERSION_LESS 1700)
				add_definitions(-D_WIN32_WINNT=0x600)
			endif()
		endif()

		add_definitions(-DOIIO_STATIC_BUILD)

		set(CMAKE_CXX_FLAGS "/nologo /J /Gd /EHsc /MP" CACHE STRING "MSVC MT C++ flags " FORCE)
		set(CMAKE_C_FLAGS   "/nologo /J /Gd /MP"       CACHE STRING "MSVC MT C++ flags " FORCE)

		if(CMAKE_CL_64)
			set(CMAKE_CXX_FLAGS_DEBUG "/Od /RTC1 /MTd /Zi /MP" CACHE STRING "MSVC MT flags " FORCE)
		else()
			set(CMAKE_CXX_FLAGS_DEBUG "/Od /RTC1 /MTd /ZI /MP" CACHE STRING "MSVC MT flags " FORCE)
		endif()
		set(CMAKE_CXX_FLAGS_RELEASE "/O2 /Ob2 /MT /MP" CACHE STRING "MSVC MT flags " FORCE)
		set(CMAKE_CXX_FLAGS_MINSIZEREL "/O1 /Ob1 /MT /MP" CACHE STRING "MSVC MT flags " FORCE)
		set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "/O2 /Ob1 /MT /Zi /MP" CACHE STRING "MSVC MT flags " FORCE)
		if(CMAKE_CL_64)
			set(CMAKE_C_FLAGS_DEBUG "/Od /RTC1 /MTd /Zi /MP" CACHE STRING "MSVC MT flags " FORCE)
		else()
			set(CMAKE_C_FLAGS_DEBUG "/Od /RTC1 /MTd /ZI /MP" CACHE STRING "MSVC MT flags " FORCE)
		endif()
		set(CMAKE_C_FLAGS_RELEASE "/O2 /Ob2 /MT /MP" CACHE STRING "MSVC MT flags " FORCE)
		set(CMAKE_C_FLAGS_MINSIZEREL "/O1 /Ob1 /MT /MP" CACHE STRING "MSVC MT flags " FORCE)
		set(CMAKE_C_FLAGS_RELWITHDEBINFO "/O2 /Ob1 /MT /Zi /MP" CACHE STRING "MSVC MT flags " FORCE)

		# most msvc warnings are C & C++
		set(_WARNINGS
			# warning level:
			"/W3"
			"/w34062"  # switch statement contains 'default' but no 'case' labels
			# disable:
			"/wd4018"  # signed/unsigned mismatch
			"/wd4065"  # switch statement contains 'default' but no 'case' labels
			"/wd4127"  # conditional expression is constant
			"/wd4181"  # qualifier applied to reference type; ignored
			"/wd4200"  # zero-sized array in struct/union
			"/wd4244"  # conversion from 'type1' to 'type2', possible loss of data
			"/wd4267"  # conversion from 'size_t' to 'type', possible loss of data
			"/wd4305"  # truncation from 'type1' to 'type2'
			"/wd4800"  # forcing value to bool 'true' or 'false'
			# errors:
			"/we4013"  # 'function' undefined; assuming extern returning int
			"/we4431"  # missing type specifier - int assumed
		)

		string(REPLACE ";" " " _WARNINGS "${_WARNINGS}")
		set(C_WARNINGS "${_WARNINGS}")
		set(CXX_WARNINGS "${_WARNINGS}")
		unset(_WARNINGS)

		set(PLATFORM_LINKFLAGS "/SUBSYSTEM:CONSOLE /STACK:2097152 /INCREMENTAL:NO /NODEFAULTLIB:msvcrt.lib /NODEFAULTLIB:msvcmrt.lib /NODEFAULTLIB:msvcurt.lib /NODEFAULTLIB:msvcrtd.lib")

		# MSVC only, Mingw doesnt need
		if(CMAKE_CL_64)
			set(PLATFORM_LINKFLAGS "/MACHINE:X64 /OPT:NOREF ${PLATFORM_LINKFLAGS}")
		else()
			set(PLATFORM_LINKFLAGS "/MACHINE:IX86 /LARGEADDRESSAWARE ${PLATFORM_LINKFLAGS}")
		endif()

		set(PLATFORM_LINKFLAGS_DEBUG "/IGNORE:4099 /NODEFAULTLIB:libcmt.lib /NODEFAULTLIB:libc.lib")

		if(NOT DEFINED LIBDIR)
			# Setup 64bit and 64bit windows systems
			if(CMAKE_CL_64)
				message(STATUS "64 bit compiler detected.")
				set(LIBDIR_BASE ${CMAKE_SOURCE_DIR}/../lib/win64)
			else()
				message(STATUS "32 bit compiler detected.")
				set(LIBDIR_BASE ${CMAKE_SOURCE_DIR}/../lib/windows)
			endif()
			if(MSVC12)
				message(STATUS "Visual C++ 2013 detected.")
				set(LIBDIR ${LIBDIR_BASE}_vc12)
			else()
				set(LIBDIR ${LIBDIR_BASE})
			endif()
		else()
			message(STATUS using LIBDIR  ${LIBDIR})
		endif()

		# Add each of our libraries to our cmake_prefix_path so find_package() could work
		FILE(GLOB children RELATIVE ${LIBDIR} ${LIBDIR}/*)
		FOREACH(child ${children})
		IF(IS_DIRECTORY ${LIBDIR}/${child})
			LIST(APPEND CMAKE_PREFIX_PATH  ${LIBDIR}/${child})
		ENDIF()
		ENDFOREACH()

		set(ZLIB_INCLUDE_DIRS ${LIBDIR}/zlib/include)
		set(ZLIB_LIBRARIES ${LIBDIR}/zlib/lib/libz_st.lib)
		set(ZLIB_INCLUDE_DIR ${LIBDIR}/zlib/include)
		set(ZLIB_LIBRARY ${LIBDIR}/zlib/lib/libz_st.lib)
		set(ZLIB_DIR ${LIBDIR}/zlib)
		#find_package(zlib) # we want to find before finding things that depend on it like png


		find_package(png)
		if(NOT PNG_FOUND)
			set(PNG_PNG_INCLUDE_DIR ${LIBDIR}/png/include)
			message(WARNING "Using HARDCODED libpng locations")
			set(PNG_LIBRARIES libpng)
			set(PNG "${LIBDIR}/png")
			set(PNG_INCLUDE_DIRS "${PNG}/include")
			set(PNG_LIBPATH ${PNG}/lib) # not cmake defined
		endif()

		if(MSVC)
			set(JPEG_NAMES ${JPEG_NAMES} libjpeg)
		endif()
		find_package(jpeg REQUIRED)

		set(PTHREADS_INCLUDE_DIRS ${LIBDIR}/pthreads/include)
		set(PTHREADS_LIBRARIES ${LIBDIR}/pthreads/lib/pthreadVC2.lib)

		set(FREETYPE ${LIBDIR}/freetype)
		set(FREETYPE_INCLUDE_DIRS
			${LIBDIR}/freetype/include
			${LIBDIR}/freetype/include/freetype2
		)
		set(FREETYPE_LIBRARY ${LIBDIR}/freetype/lib/freetype2ST.lib)
		find_package(freetype REQUIRED)

		if(WITH_FFTW3)
			set(FFTW3 ${LIBDIR}/fftw3)
			set(FFTW3_LIBRARIES libfftw)
			set(FFTW3_INCLUDE_DIRS ${FFTW3}/include)
			set(FFTW3_LIBPATH ${FFTW3}/lib)
		endif()

		if(WITH_OPENCOLLADA)
			set(OPENCOLLADA ${LIBDIR}/opencollada)

			set(OPENCOLLADA_INCLUDE_DIRS
				${OPENCOLLADA}/include/opencollada/COLLADAStreamWriter
				${OPENCOLLADA}/include/opencollada/COLLADABaseUtils
				${OPENCOLLADA}/include/opencollada/COLLADAFramework
				${OPENCOLLADA}/include/opencollada/COLLADASaxFrameworkLoader
				${OPENCOLLADA}/include/opencollada/GeneratedSaxParser
			)

			set(OPENCOLLADA_LIBRARIES
				${OPENCOLLADA}/lib/opencollada/OpenCOLLADASaxFrameworkLoader.lib
				${OPENCOLLADA}/lib/opencollada/OpenCOLLADAFramework.lib
				${OPENCOLLADA}/lib/opencollada/OpenCOLLADABaseUtils.lib
				${OPENCOLLADA}/lib/opencollada/OpenCOLLADAStreamWriter.lib
				${OPENCOLLADA}/lib/opencollada/MathMLSolver.lib
				${OPENCOLLADA}/lib/opencollada/GeneratedSaxParser.lib
				${OPENCOLLADA}/lib/opencollada/xml.lib
				${OPENCOLLADA}/lib/opencollada/buffer.lib
				${OPENCOLLADA}/lib/opencollada/ftoa.lib
			)

			if(NOT WITH_LLVM)
				list(APPEND OPENCOLLADA_LIBRARIES ${OPENCOLLADA}/lib/opencollada/UTF.lib)
			endif()

			set(PCRE_LIBRARIES
				${OPENCOLLADA}/lib/opencollada/pcre.lib
			)
		endif()

		if(WITH_CODEC_FFMPEG)
			set(FFMPEG_INCLUDE_DIRS
				${LIBDIR}/ffmpeg/include
				${LIBDIR}/ffmpeg/include/msvc
			)
			find_package(FFMPEG)
			if(NOT FFMPEG_FOUND)
				message(WARNING "Using HARDCODED ffmpeg locations")
				set(FFMPEG_LIBRARY_VERSION 55)
				set(FFMPEG_LIBRARY_VERSION_AVU 52)
				set(FFMPEG_LIBRARIES
					${LIBDIR}/ffmpeg/lib/avcodec-${FFMPEG_LIBRARY_VERSION}.lib
					${LIBDIR}/ffmpeg/lib/avformat-${FFMPEG_LIBRARY_VERSION}.lib
					${LIBDIR}/ffmpeg/lib/avdevice-${FFMPEG_LIBRARY_VERSION}.lib
					${LIBDIR}/ffmpeg/lib/avutil-${FFMPEG_LIBRARY_VERSION_AVU}.lib
					${LIBDIR}/ffmpeg/lib/swscale-2.lib
					)
			endif()
		endif()

		if(WITH_IMAGE_OPENEXR)
			set(OPENEXR_ROOT_DIR ${LIBDIR}/openexr)
			set(OPENEXR_VERSION "2.1")
			find_package(OPENEXR REQUIRED)
			if(NOT OPENEXR_FOUND)
			message(WARNING "Using HARDCODED OpenEXR locations")
			set(OPENEXR ${LIBDIR}/openexr)
			set(OPENEXR_INCLUDE_DIR ${OPENEXR}/include)
			set(OPENEXR_INCLUDE_DIRS ${OPENEXR}/include/OpenEXR)
			set(OPENEXR_LIBPATH ${OPENEXR}/lib)
			if(MSVC12)
				set(OPENEXR_LIBRARIES
					${OPENEXR_LIBPATH}/Iex-2_1.lib
					${OPENEXR_LIBPATH}/Half.lib
					${OPENEXR_LIBPATH}/IlmImf-2_1.lib
					${OPENEXR_LIBPATH}/Imath-2_1.lib
					${OPENEXR_LIBPATH}/IlmThread-2_1.lib
				)
			else()
				set(OPENEXR_LIBRARIES
					${OPENEXR_LIBPATH}/Iex.lib
					${OPENEXR_LIBPATH}/Half.lib
					${OPENEXR_LIBPATH}/IlmImf.lib
					${OPENEXR_LIBPATH}/Imath.lib
					${OPENEXR_LIBPATH}/IlmThread.lib
				)
			endif()
			endif(NOT OPENEXR_FOUND)
		endif()

		if(WITH_IMAGE_TIFF)
		# Try to find tiff first then complain and set static and maybe wrong paths
		find_package(TIFF)
		if(NOT TIFF_FOUND)
			message(WARNING "Using HARDCODED libtiff locations")
			set(TIFF_LIBRARY ${LIBDIR}/tiff/lib/libtiff.lib)
			set(TIFF_INCLUDE_DIR ${LIBDIR}/tiff/include)
		endif()
		endif()

		if(WITH_JACK)
			set(JACK_INCLUDE_DIRS
				${LIBDIR}/jack/include/jack
				${LIBDIR}/jack/include
			)
			set(JACK_LIBRARIES optimized ${LIBDIR}/jack/lib/libjack.lib debug ${LIBDIR}/jack/lib/libjack_d.lib)
		endif()

		if(WITH_PYTHON)
			set(PYTHON_VERSION 3.4) # CACHE STRING)

			string(REPLACE "." "" _PYTHON_VERSION_NO_DOTS ${PYTHON_VERSION})
			# Use shared libs for vc2008 and vc2010 until we actually have vc2010 libs
			set(PYTHON_LIBRARY ${LIBDIR}/python/lib/python${_PYTHON_VERSION_NO_DOTS}.lib)
			unset(_PYTHON_VERSION_NO_DOTS)

			# Shared includes for both vc2008 and vc2010
			set(PYTHON_INCLUDE_DIR ${LIBDIR}/python/include/python${PYTHON_VERSION})

			# uncached vars
			set(PYTHON_INCLUDE_DIRS "${PYTHON_INCLUDE_DIR}")
			set(PYTHON_LIBRARIES  "${PYTHON_LIBRARY}")
		endif()

		if(WITH_BOOST)
			if(WITH_CYCLES_OSL)
				set(boost_extra_libs wave)
			endif(WITH_CYCLES_OSL)
			if(WITH_INTERNATIONAL)
				list(APPEND boost_extra_libs locale)
			endif(WITH_INTERNATIONAL)
			set(Boost_USE_STATIC_RUNTIME ON) # prefix lib
			set(Boost_USE_MULTITHREADED ON) # suffix -mt
			set(Boost_USE_STATIC_LIBS ON) # suffix -s
			find_package(Boost COMPONENTS date_time filesystem thread regex system ${boost_extra_libs})
			IF(NOT Boost_FOUND)
				message(WARNING "USING HARDCODED boost locations")
				set(BOOST ${LIBDIR}/boost)
				set(BOOST_INCLUDE_DIR ${BOOST}/include)
				if(MSVC12)
					set(BOOST_LIBPATH ${BOOST}/lib)
					set(BOOST_POSTFIX "vc120-mt-s-1_55.lib")
					set(BOOST_DEBUG_POSTFIX "vc120-mt-sgd-1_55.lib")
				else()
					set(BOOST_LIBPATH ${BOOST}/lib)
					set(BOOST_POSTFIX "vc90-mt-s-1_49.lib")
					set(BOOST_DEBUG_POSTFIX "vc90-mt-sgd-1_49.lib")
				endif()
				set(BOOST_LIBRARIES
					optimized libboost_date_time-${BOOST_POSTFIX} optimized libboost_filesystem-${BOOST_POSTFIX}
					optimized libboost_regex-${BOOST_POSTFIX}
					optimized libboost_system-${BOOST_POSTFIX} optimized libboost_thread-${BOOST_POSTFIX}
					debug libboost_date_time-${BOOST_DEBUG_POSTFIX} debug libboost_filesystem-${BOOST_DEBUG_POSTFIX}
					debug libboost_regex-${BOOST_DEBUG_POSTFIX}
					debug libboost_system-${BOOST_DEBUG_POSTFIX} debug libboost_thread-${BOOST_DEBUG_POSTFIX})
				if(WITH_CYCLES_OSL)
					set(BOOST_LIBRARIES ${BOOST_LIBRARIES}
						optimized libboost_wave-${BOOST_POSTFIX}
						debug libboost_wave-${BOOST_DEBUG_POSTFIX})
				endif()
				if(WITH_INTERNATIONAL)
					set(BOOST_LIBRARIES ${BOOST_LIBRARIES}
						optimized libboost_locale-${BOOST_POSTFIX}
						debug libboost_locale-${BOOST_DEBUG_POSTFIX})
				endif()
			else() # we found boost using find_package
				set(BOOST_INCLUDE_DIR ${Boost_INCLUDE_DIRS})
				set(BOOST_LIBRARIES ${Boost_LIBRARIES})
				set(BOOST_LIBPATH ${Boost_LIBRARY_DIRS})
			endif()
			set(BOOST_DEFINITIONS "-DBOOST_ALL_NO_LIB")
		endif()
			
		if(WITH_OPENIMAGEIO)
			find_package(OpenImageIO)
			set(OPENIMAGEIO ${LIBDIR}/openimageio)
			set(OPENIMAGEIO_INCLUDE_DIRS ${OPENIMAGEIO}/include)
			set(OIIO_OPTIMIZED optimized OpenImageIO optimized OpenImageIO_Util)
			set(OIIO_DEBUG debug OpenImageIO_d debug OpenImageIO_Util_d)
			set(OPENIMAGEIO_LIBRARIES ${OIIO_OPTIMIZED} ${OIIO_DEBUG})
			set(OPENIMAGEIO_LIBPATH ${OPENIMAGEIO}/lib)
			set(OPENIMAGEIO_DEFINITIONS "-DUSE_TBB=0")
		endif()

		if(WITH_LLVM)
			set(LLVM_DIRECTORY ${LIBDIR}/llvm CACHE PATH	"Path to the LLVM installation")
			file(GLOB LLVM_LIBRARY_OPTIMIZED ${LLVM_DIRECTORY}/lib/*.lib)

			if(EXISTS ${LLVM_DIRECTORY}/debug/lib)
				foreach(LLVM_OPTIMIZED_LIB ${LLVM_LIBRARY_OPTIMIZED})
					get_filename_component(LIBNAME ${LLVM_OPTIMIZED_LIB} ABSOLUTE)
					list(APPEND LLVM_LIBS optimized ${LIBNAME})
				endforeach(LLVM_OPTIMIZED_LIB)
			
				file(GLOB LLVM_LIBRARY_DEBUG ${LLVM_DIRECTORY}/debug/lib/*.lib)

				foreach(LLVM_DEBUG_LIB ${LLVM_LIBRARY_DEBUG})
					get_filename_component(LIBNAME ${LLVM_DEBUG_LIB} ABSOLUTE)
					list(APPEND LLVM_LIBS debug ${LIBNAME})
				endforeach(LLVM_DEBUG_LIB)

				set(LLVM_LIBRARY ${LLVM_LIBS})
			else()
				message(WARNING "LLVM debug libs not present on this system. Using release libs for debug builds.")
				set(LLVM_LIBRARY ${LLVM_LIBRARY_OPTIMIZED})
			endif()
			
		endif()
	
		if(WITH_OPENCOLORIO)
			set(OPENCOLORIO ${LIBDIR}/opencolorio)
			set(OPENCOLORIO_INCLUDE_DIRS ${OPENCOLORIO}/include)
			set(OPENCOLORIO_LIBRARIES OpenColorIO)
			set(OPENCOLORIO_LIBPATH ${LIBDIR}/opencolorio/lib)
			set(OPENCOLORIO_DEFINITIONS)
		endif()
		

		if(WITH_MOD_CLOTH_ELTOPO)
			set(LAPACK ${LIBDIR}/lapack)
			# set(LAPACK_INCLUDE_DIR ${LAPACK}/include)
			set(LAPACK_LIBPATH ${LAPACK}/lib)
			set(LAPACK_LIBRARIES
				${LIBDIR}/lapack/lib/libf2c.lib
				${LIBDIR}/lapack/lib/clapack_nowrap.lib
				${LIBDIR}/lapack/lib/BLAS_nowrap.lib
			)
		endif()

		# used in many places so include globally, like OpenGL
		blender_include_dirs_sys("${PTHREADS_INCLUDE_DIRS}")

	elseif(CMAKE_COMPILER_IS_GNUCC)
		# keep GCC specific stuff here
		include(CheckCSourceCompiles)
		# Setup 64bit and 64bit windows systems
		CHECK_C_SOURCE_COMPILES("
			#ifndef __MINGW64__
			#error
			#endif
			int main(void) { return 0; }
			" 
			WITH_MINGW64)
		
		if(WITH_MINGW64)
			message(STATUS "Compiling for 64 bit with MinGW-w64.")
			set(LIBDIR ${CMAKE_SOURCE_DIR}/../lib/mingw64)
		else()
			message(STATUS "Compiling for 32 bit with MinGW-w32.")
			set(LIBDIR ${CMAKE_SOURCE_DIR}/../lib/mingw32)
			
			if(WITH_RAYOPTIMIZATION)
				message(WARNING "MinGW-w32 is known to be unstable with 'WITH_RAYOPTIMIZATION' option enabled.")
			endif()
		endif()
		
		list(APPEND PLATFORM_LINKLIBS -lshell32 -lshfolder -lgdi32 -lmsvcrt -lwinmm -lmingw32 -lm -lws2_32 -lz -lstdc++ -lole32 -luuid -lwsock32 -lpsapi)
		set(PLATFORM_CFLAGS "-pipe -funsigned-char -fno-strict-aliasing")

		if(WITH_MINGW64)
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fpermissive")
			list(APPEND PLATFORM_LINKLIBS -lpthread)
			
			add_definitions(-DFREE_WINDOWS64 -DMS_WIN64)
		endif()

		add_definitions(-D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE)

		add_definitions(-DFREE_WINDOWS)

		set(PNG "${LIBDIR}/png")
		set(PNG_INCLUDE_DIRS "${PNG}/include")
		set(PNG_LIBPATH ${PNG}/lib) # not cmake defined

		if(WITH_MINGW64)
			set(JPEG_LIBRARIES jpeg)
		else()
			set(JPEG_LIBRARIES libjpeg)
		endif()
		set(PNG_LIBRARIES png)

		set(ZLIB ${LIBDIR}/zlib)
		set(ZLIB_INCLUDE_DIRS ${ZLIB}/include)
		set(ZLIB_LIBPATH ${ZLIB}/lib)
		set(ZLIB_LIBRARIES z)

		set(JPEG "${LIBDIR}/jpeg")
		set(JPEG_INCLUDE_DIR "${JPEG}/include")
		set(JPEG_LIBPATH ${JPEG}/lib) # not cmake defined
		
		# comes with own pthread library
		if(NOT WITH_MINGW64)
			set(PTHREADS ${LIBDIR}/pthreads)
			#set(PTHREADS_INCLUDE_DIRS ${PTHREADS}/include)
			set(PTHREADS_LIBPATH ${PTHREADS}/lib)
			set(PTHREADS_LIBRARIES pthreadGC2)
		endif()
		
		set(FREETYPE ${LIBDIR}/freetype)
		set(FREETYPE_INCLUDE_DIRS ${FREETYPE}/include ${FREETYPE}/include/freetype2)
		set(FREETYPE_LIBPATH ${FREETYPE}/lib)
		set(FREETYPE_LIBRARY freetype)

		if(WITH_FFTW3)
			set(FFTW3 ${LIBDIR}/fftw3)
			set(FFTW3_LIBRARIES fftw3)
			set(FFTW3_INCLUDE_DIRS ${FFTW3}/include)
			set(FFTW3_LIBPATH ${FFTW3}/lib)
		endif()

		if(WITH_OPENCOLLADA)
			set(OPENCOLLADA ${LIBDIR}/opencollada)
			set(OPENCOLLADA_INCLUDE_DIRS
				${OPENCOLLADA}/include/opencollada/COLLADAStreamWriter
				${OPENCOLLADA}/include/opencollada/COLLADABaseUtils
				${OPENCOLLADA}/include/opencollada/COLLADAFramework
				${OPENCOLLADA}/include/opencollada/COLLADASaxFrameworkLoader
				${OPENCOLLADA}/include/opencollada/GeneratedSaxParser
			)
			set(OPENCOLLADA_LIBPATH ${OPENCOLLADA}/lib/opencollada)
			set(OPENCOLLADA_LIBRARIES OpenCOLLADAStreamWriter OpenCOLLADASaxFrameworkLoader OpenCOLLADAFramework OpenCOLLADABaseUtils GeneratedSaxParser UTF MathMLSolver buffer ftoa xml)
			set(PCRE_LIBRARIES pcre)
		endif()

		if(WITH_CODEC_FFMPEG)
			set(FFMPEG ${LIBDIR}/ffmpeg)
			set(FFMPEG_INCLUDE_DIRS ${FFMPEG}/include)
			if(WITH_MINGW64)
				set(FFMPEG_LIBRARIES avcodec.dll avformat.dll avdevice.dll avutil.dll swscale.dll swresample.dll)
			else()
				set(FFMPEG_LIBRARIES avcodec-55 avformat-55 avdevice-55 avutil-52 swscale-2)
			endif()
			set(FFMPEG_LIBPATH ${FFMPEG}/lib)
		endif()

		if(WITH_IMAGE_OPENEXR)
			set(OPENEXR ${LIBDIR}/openexr)
			set(OPENEXR_INCLUDE_DIR ${OPENEXR}/include)
			set(OPENEXR_INCLUDE_DIRS ${OPENEXR}/include/OpenEXR)
			set(OPENEXR_LIBRARIES Half IlmImf Imath IlmThread Iex)
			set(OPENEXR_LIBPATH ${OPENEXR}/lib)
		endif()

		if(WITH_IMAGE_TIFF)
			set(TIFF ${LIBDIR}/tiff)
			set(TIFF_LIBRARY tiff)
			set(TIFF_INCLUDE_DIR ${TIFF}/include)
			set(TIFF_LIBPATH ${TIFF}/lib)
		endif()

		if(WITH_JACK)
			set(JACK ${LIBDIR}/jack)
			set(JACK_INCLUDE_DIRS ${JACK}/include/jack ${JACK}/include)
			set(JACK_LIBRARIES jack)
			set(JACK_LIBPATH ${JACK}/lib)

			# TODO, gives linking errors, force off
			set(WITH_JACK OFF)
		endif()

		if(WITH_PYTHON)
			# normally cached but not since we include them with blender
			set(PYTHON_VERSION 3.4) #  CACHE STRING)
			string(REPLACE "." "" _PYTHON_VERSION_NO_DOTS ${PYTHON_VERSION})
			set(PYTHON_INCLUDE_DIR "${LIBDIR}/python/include/python${PYTHON_VERSION}")  # CACHE PATH)
			set(PYTHON_LIBRARY "${LIBDIR}/python/lib/python${_PYTHON_VERSION_NO_DOTS}mw.lib")  # CACHE FILEPATH)
			unset(_PYTHON_VERSION_NO_DOTS)

			# uncached vars
			set(PYTHON_INCLUDE_DIRS "${PYTHON_INCLUDE_DIR}")
			set(PYTHON_LIBRARIES  "${PYTHON_LIBRARY}")
		endif()

		if(WITH_BOOST)
			set(BOOST ${LIBDIR}/boost)
			set(BOOST_INCLUDE_DIR ${BOOST}/include)
			if(WITH_MINGW64)
				set(BOOST_POSTFIX "mgw47-mt-s-1_49")
				set(BOOST_DEBUG_POSTFIX "mgw47-mt-sd-1_49")
			else()
				set(BOOST_POSTFIX "mgw46-mt-s-1_49")
				set(BOOST_DEBUG_POSTFIX "mgw46-mt-sd-1_49")
			endif()
			set(BOOST_LIBRARIES
				optimized boost_date_time-${BOOST_POSTFIX} boost_filesystem-${BOOST_POSTFIX}
				boost_regex-${BOOST_POSTFIX}
				boost_system-${BOOST_POSTFIX} boost_thread-${BOOST_POSTFIX}
				debug boost_date_time-${BOOST_DEBUG_POSTFIX} boost_filesystem-${BOOST_DEBUG_POSTFIX}
				boost_regex-${BOOST_DEBUG_POSTFIX}
				boost_system-${BOOST_DEBUG_POSTFIX} boost_thread-${BOOST_DEBUG_POSTFIX})
			if(WITH_INTERNATIONAL)
				set(BOOST_LIBRARIES ${BOOST_LIBRARIES}
					optimized boost_locale-${BOOST_POSTFIX}
					debug boost_locale-${BOOST_DEBUG_POSTFIX}) 
			endif()
			if(WITH_CYCLES_OSL)
				set(BOOST_LIBRARIES ${BOOST_LIBRARIES}
					optimized boost_wave-${BOOST_POSTFIX}
					debug boost_wave-${BOOST_DEBUG_POSTFIX}) 
			endif()
			set(BOOST_LIBPATH ${BOOST}/lib)
			set(BOOST_DEFINITIONS "-DBOOST_ALL_NO_LIB -DBOOST_THREAD_USE_LIB ")
		endif()
			
		if(WITH_OPENIMAGEIO)
			set(OPENIMAGEIO ${LIBDIR}/openimageio)
			set(OPENIMAGEIO_INCLUDE_DIRS ${OPENIMAGEIO}/include)
			set(OPENIMAGEIO_LIBRARIES OpenImageIO)
			set(OPENIMAGEIO_LIBPATH ${OPENIMAGEIO}/lib)
			set(OPENIMAGEIO_DEFINITIONS "")
		endif()
		
		if(WITH_LLVM)
			set(LLVM_DIRECTORY ${LIBDIR}/llvm CACHE PATH	"Path to the LLVM installation")
			set(LLVM_LIBPATH ${LLVM_DIRECTORY}/lib)
			# Explicitly set llvm lib order.
			#---- WARNING ON GCC ORDER OF LIBS IS IMPORTANT, DO NOT CHANGE! ---------
			set(LLVM_LIBRARY LLVMSelectionDAG LLVMCodeGen LLVMScalarOpts LLVMAnalysis LLVMArchive
				LLVMAsmParser LLVMAsmPrinter
				LLVMBitReader LLVMBitWriter
				LLVMDebugInfo LLVMExecutionEngine
				LLVMInstCombine LLVMInstrumentation
				LLVMInterpreter LLVMJIT
				LLVMLinker LLVMMC
				LLVMMCDisassembler LLVMMCJIT
				LLVMMCParser LLVMObject
				LLVMRuntimeDyld 
				LLVMSupport
				LLVMTableGen LLVMTarget
				LLVMTransformUtils LLVMVectorize
				LLVMX86AsmParser LLVMX86AsmPrinter
				LLVMX86CodeGen LLVMX86Desc
				LLVMX86Disassembler LLVMX86Info
				LLVMX86Utils LLVMipa
				LLVMipo LLVMCore)
			# imagehelp is needed by LLVM 3.1 on MinGW, check lib\Support\Windows\Signals.inc
			list(APPEND PLATFORM_LINKLIBS -limagehlp)
		endif()
		
		if(WITH_OPENCOLORIO)
			set(OPENCOLORIO ${LIBDIR}/opencolorio)
			set(OPENCOLORIO_INCLUDE_DIRS ${OPENCOLORIO}/include)
			set(OPENCOLORIO_LIBRARIES OpenColorIO)
			set(OPENCOLORIO_LIBPATH ${OPENCOLORIO}/lib)
			set(OPENCOLORIO_DEFINITIONS)
		endif()

		set(PLATFORM_LINKFLAGS "-Xlinker --stack=2097152")

		## DISABLE - causes linking errors 
		## for re-distribution, so users dont need mingw installed
		# set(PLATFORM_LINKFLAGS "${PLATFORM_LINKFLAGS} -static-libgcc -static-libstdc++")

	endif()
	
	# Things common to both mingw and MSVC  should go here

	set(WINTAB_INC ${LIBDIR}/wintab/include)

	if(WITH_OPENAL)
		set(OPENAL ${LIBDIR}/openal)
		set(OPENALDIR ${LIBDIR}/openal)
		set(OPENAL_INCLUDE_DIR ${OPENAL}/include)
		if(MSVC12)
			set(OPENAL_LIBRARY openal32)
		else()
			set(OPENAL_LIBRARY wrap_oal)
		endif()
		set(OPENAL_LIBPATH ${OPENAL}/lib)
	endif()

	if(WITH_CODEC_SNDFILE)
		set(SNDFILE ${LIBDIR}/sndfile)
		set(SNDFILE_INCLUDE_DIRS ${SNDFILE}/include)
		set(SNDFILE_LIBRARIES libsndfile-1)
		set(SNDFILE_LIBPATH ${SNDFILE}/lib) # TODO, deprecate
	endif()

	if(WITH_SDL)
		set(SDL ${LIBDIR}/sdl)
		set(SDL_INCLUDE_DIR ${SDL}/include)
		set(SDL_LIBRARY SDL)
		set(SDL_LIBPATH ${SDL}/lib)
	endif()

	if(WITH_RAYOPTIMIZATION AND SUPPORT_SSE_BUILD)
		add_definitions(-D__SSE__ -D__MMX__)
	endif()

	if(WITH_CYCLES_OSL)
		set(CYCLES_OSL ${LIBDIR}/osl CACHE PATH "Path to OpenShadingLanguage installation")
	
		find_library(OSL_LIB_EXEC NAMES oslexec PATHS ${CYCLES_OSL}/lib)
		find_library(OSL_LIB_COMP NAMES oslcomp PATHS ${CYCLES_OSL}/lib)
		find_library(OSL_LIB_QUERY NAMES oslquery PATHS ${CYCLES_OSL}/lib)
		find_library(OSL_LIB_EXEC_DEBUG NAMES oslexec_d PATHS ${CYCLES_OSL}/lib)
		find_library(OSL_LIB_COMP_DEBUG NAMES oslcomp_d PATHS ${CYCLES_OSL}/lib)
		find_library(OSL_LIB_QUERY_DEBUG NAMES oslquery_d PATHS ${CYCLES_OSL}/lib)
		list(APPEND OSL_LIBRARIES optimized ${OSL_LIB_COMP} optimized ${OSL_LIB_EXEC} optimized ${OSL_LIB_QUERY} debug ${OSL_LIB_EXEC_DEBUG} debug ${OSL_LIB_COMP_DEBUG} debug ${OSL_LIB_QUERY_DEBUG})
		find_path(OSL_INCLUDES OSL/oslclosure.h PATHS ${CYCLES_OSL}/include)
		find_program(OSL_COMPILER NAMES oslc PATHS ${CYCLES_OSL}/bin)
	
		if(OSL_INCLUDES AND OSL_LIBRARIES AND OSL_COMPILER)
			set(OSL_FOUND TRUE)
		else()
			message(STATUS "OSL not found")
		endif()
	endif()

elseif(APPLE)

	if(${CMAKE_OSX_DEPLOYMENT_TARGET} STREQUAL "10.5" OR ${CMAKE_OSX_DEPLOYMENT_TARGET} STRGREATER "10.5")
		set(WITH_LIBS10.5 ON CACHE BOOL "Use 10.5 libs" FORCE) # valid also for 10.6/7/8/9
	endif()

	if(WITH_LIBS10.5)
		set(LIBDIR ${CMAKE_SOURCE_DIR}/../lib/darwin-9.x.universal)
	else()
		if(CMAKE_OSX_ARCHITECTURES MATCHES i386)
			set(LIBDIR ${CMAKE_SOURCE_DIR}/../lib/darwin-8.x.i386)
		else()
			set(LIBDIR ${CMAKE_SOURCE_DIR}/../lib/darwin-8.0.0-powerpc)
		endif()
	endif()


	if(WITH_OPENAL)
		find_package(OpenAL)
		if(OPENAL_FOUND)
			set(WITH_OPENAL ON)
			set(OPENAL_INCLUDE_DIR "${LIBDIR}/openal/include")
		else()
			set(WITH_OPENAL OFF)
		endif()
	endif()

	if(WITH_JACK)
		find_library(JACK_FRAMEWORK
			NAMES jackmp
		)
		set(JACK_INCLUDE_DIRS ${JACK_FRAMEWORK}/headers)
		if(NOT JACK_FRAMEWORK)
			set(WITH_JACK OFF)
		endif()
	endif()

	if(WITH_CODEC_SNDFILE)
		set(SNDFILE ${LIBDIR}/sndfile)
		set(SNDFILE_INCLUDE_DIRS ${SNDFILE}/include)
		set(SNDFILE_LIBRARIES sndfile FLAC ogg vorbis vorbisenc)
		set(SNDFILE_LIBPATH ${SNDFILE}/lib ${FFMPEG}/lib)  # TODO, deprecate
	endif()

	if(WITH_PYTHON)
		# we use precompiled libraries for py 3.4 and up by default
		set(PYTHON_VERSION 3.4)
		if(NOT WITH_PYTHON_MODULE AND NOT WITH_PYTHON_FRAMEWORK)
			# normally cached but not since we include them with blender
			set(PYTHON_INCLUDE_DIR "${LIBDIR}/python/include/python${PYTHON_VERSION}m")
			# set(PYTHON_BINARY "${LIBDIR}/python/bin/python${PYTHON_VERSION}") # not used yet
			set(PYTHON_LIBRARY python${PYTHON_VERSION}m)
			set(PYTHON_LIBPATH "${LIBDIR}/python/lib/python${PYTHON_VERSION}")
			# set(PYTHON_LINKFLAGS "-u _PyMac_Error")  # won't  build with this enabled
		else()
			# module must be compiled against Python framework
			set(PYTHON_INCLUDE_DIR "/Library/Frameworks/Python.framework/Versions/${PYTHON_VERSION}/include/python${PYTHON_VERSION}m")
			set(PYTHON_BINARY "/Library/Frameworks/Python.framework/Versions/${PYTHON_VERSION}/bin/python${PYTHON_VERSION}")
			#set(PYTHON_LIBRARY python${PYTHON_VERSION})
			set(PYTHON_LIBPATH "/Library/Frameworks/Python.framework/Versions/${PYTHON_VERSION}/lib/python${PYTHON_VERSION}/config-${PYTHON_VERSION}m")
			#set(PYTHON_LINKFLAGS "-u _PyMac_Error -framework Python")  # won't  build with this enabled
		endif()
		
		# uncached vars
		set(PYTHON_INCLUDE_DIRS "${PYTHON_INCLUDE_DIR}")
		set(PYTHON_LIBRARIES  "${PYTHON_LIBRARY}")
	endif()

	if(WITH_FFTW3)
		set(FFTW3 ${LIBDIR}/fftw3)
		set(FFTW3_INCLUDE_DIRS ${FFTW3}/include)
		set(FFTW3_LIBRARIES fftw3)
		set(FFTW3_LIBPATH ${FFTW3}/lib)
	endif()

	set(PNG_LIBRARIES png)
	set(JPEG_LIBRARIES jpeg)

	set(ZLIB /usr)
	set(ZLIB_INCLUDE_DIRS "${ZLIB}/include")
	set(ZLIB_LIBRARIES z bz2)

	set(FREETYPE ${LIBDIR}/freetype)
	set(FREETYPE_INCLUDE_DIRS ${FREETYPE}/include ${FREETYPE}/include/freetype2)
	set(FREETYPE_LIBPATH ${FREETYPE}/lib)
	set(FREETYPE_LIBRARY freetype)

	if(WITH_IMAGE_OPENEXR)
		set(OPENEXR ${LIBDIR}/openexr)
		set(OPENEXR_INCLUDE_DIR ${OPENEXR}/include)
		set(OPENEXR_INCLUDE_DIRS ${OPENEXR}/include/OpenEXR)
		set(OPENEXR_LIBRARIES Iex Half IlmImf Imath IlmThread)
		set(OPENEXR_LIBPATH ${OPENEXR}/lib)
	endif()

	if(WITH_CODEC_FFMPEG)
		set(FFMPEG ${LIBDIR}/ffmpeg)
		set(FFMPEG_INCLUDE_DIRS ${FFMPEG}/include)
		set(FFMPEG_LIBRARIES avcodec avdevice avformat avutil mp3lame swscale x264 xvidcore theora theoradec theoraenc vorbis vorbisenc vorbisfile ogg)
		set(FFMPEG_LIBPATH ${FFMPEG}/lib)
	endif()

	find_library(SYSTEMSTUBS_LIBRARY
		NAMES
		SystemStubs
		PATHS
	)
	mark_as_advanced(SYSTEMSTUBS_LIBRARY)
	if(SYSTEMSTUBS_LIBRARY)
		list(APPEND PLATFORM_LINKLIBS stdc++ SystemStubs)
	else()
		list(APPEND PLATFORM_LINKLIBS stdc++)
	endif()

	set(PLATFORM_CFLAGS "-pipe -funsigned-char")
	set(PLATFORM_LINKFLAGS "-fexceptions -framework CoreServices -framework Foundation -framework IOKit -framework AppKit -framework Cocoa -framework Carbon -framework AudioUnit -framework AudioToolbox -framework CoreAudio")
	if(WITH_CODEC_QUICKTIME)
		set(PLATFORM_LINKFLAGS "${PLATFORM_LINKFLAGS} -framework QTKit")
		if(CMAKE_OSX_ARCHITECTURES MATCHES i386)
			set(PLATFORM_LINKFLAGS "${PLATFORM_LINKFLAGS} -framework QuickTime")
			# libSDL still needs 32bit carbon quicktime
		endif()
	endif()

	# XXX - SOME MAC DEV PLEASE TEST WITH THE SDK INSTALLED!
	# ALSO SHOULD BE MOVED INTO OWN MODULE WHEN FUNCTIONAL
	if(WITH_INPUT_NDOF)
		# This thread it *should* work and check the framework - campbell
		# http://www.cmake.org/pipermail/cmake/2005-December/007740.html
		find_library(3DCONNEXION_CLIENT_FRAMEWORK
			NAMES 3DconnexionClient
		)
		if(NOT 3DCONNEXION_CLIENT_FRAMEWORK)
			set(WITH_INPUT_NDOF OFF)
		endif()

		if(WITH_INPUT_NDOF)
			set(PLATFORM_LINKFLAGS "${PLATFORM_LINKFLAGS} -F/Library/Frameworks -weak_framework 3DconnexionClient")
			set(NDOF_INCLUDE_DIRS /Library/Frameworks/3DconnexionClient.framework/Headers )
		endif()
	endif()

	if(WITH_JACK)
		set(PLATFORM_LINKFLAGS "${PLATFORM_LINKFLAGS} -F/Library/Frameworks -weak_framework jackmp")
	endif()
	
	if(WITH_PYTHON_MODULE OR WITH_PYTHON_FRAMEWORK)
		set(PLATFORM_LINKFLAGS "${PLATFORM_LINKFLAGS} /Library/Frameworks/Python.framework/Versions/${PYTHON_VERSION}/Python")# force cmake to link right framework
	endif()
	
	if(WITH_OPENCOLLADA)
		set(OPENCOLLADA ${LIBDIR}/opencollada)

		set(OPENCOLLADA_INCLUDE_DIRS
			${LIBDIR}/opencollada/include/COLLADAStreamWriter
			${LIBDIR}/opencollada/include/COLLADABaseUtils
			${LIBDIR}/opencollada/include/COLLADAFramework
			${LIBDIR}/opencollada/include/COLLADASaxFrameworkLoader
			${LIBDIR}/opencollada/include/GeneratedSaxParser
		)

		set(OPENCOLLADA_LIBPATH ${OPENCOLLADA}/lib)
		set(OPENCOLLADA_LIBRARIES "OpenCOLLADASaxFrameworkLoader -lOpenCOLLADAFramework -lOpenCOLLADABaseUtils -lOpenCOLLADAStreamWriter -lMathMLSolver -lGeneratedSaxParser -lxml2 -lbuffer -lftoa")
		# Use UTF functions from collada if LLVM is not enabled
		if(NOT WITH_LLVM)
			set(OPENCOLLADA_LIBRARIES "${OPENCOLLADA_LIBRARIES} -lUTF")
		endif()
		# pcre is bundled with openCollada
		#set(PCRE ${LIBDIR}/pcre)
		#set(PCRE_LIBPATH ${PCRE}/lib)
		set(PCRE_LIBRARIES pcre)
		#libxml2 is used
		#set(EXPAT ${LIBDIR}/expat)
		#set(EXPAT_LIBPATH ${EXPAT}/lib)
		set(EXPAT_LIB)
	endif()

	if(WITH_SDL)
		set(SDL ${LIBDIR}/sdl)
		set(SDL_INCLUDE_DIR ${SDL}/include)
		set(SDL_LIBRARY SDL)
		set(SDL_LIBPATH ${SDL}/lib)
	endif()

	set(PNG "${LIBDIR}/png")
	set(PNG_INCLUDE_DIRS "${PNG}/include")
	set(PNG_LIBPATH ${PNG}/lib)

	set(JPEG "${LIBDIR}/jpeg")
	set(JPEG_INCLUDE_DIR "${JPEG}/include")
	set(JPEG_LIBPATH ${JPEG}/lib)

	if(WITH_IMAGE_TIFF)
		set(TIFF ${LIBDIR}/tiff)
		set(TIFF_INCLUDE_DIR ${TIFF}/include)
		set(TIFF_LIBRARY tiff)
		set(TIFF_LIBPATH ${TIFF}/lib)
	endif()

	if(WITH_INPUT_NDOF)
		# linker needs "-weak_framework 3DconnexionClient"
	endif()

	if(WITH_BOOST)
		set(BOOST ${LIBDIR}/boost)
		set(BOOST_INCLUDE_DIR ${BOOST}/include)
		set(BOOST_LIBRARIES boost_date_time-mt boost_filesystem-mt boost_regex-mt boost_system-mt boost_thread-mt boost_wave-mt)
		if(WITH_INTERNATIONAL)
			list(APPEND BOOST_LIBRARIES boost_locale-mt)
		endif()
		if(WITH_CYCLES_NETWORK)
			list(APPEND BOOST_LIBRARIES boost_serialization-mt)
		endif()
		set(BOOST_LIBPATH ${BOOST}/lib)
		set(BOOST_DEFINITIONS)
	endif()
	
	if(WITH_INTERNATIONAL OR WITH_CODEC_FFMPEG)
		set(PLATFORM_LINKFLAGS "${PLATFORM_LINKFLAGS} -liconv") # boost_locale and ffmpeg needs it !
	endif()

	if(WITH_OPENIMAGEIO)
		set(OPENIMAGEIO ${LIBDIR}/openimageio)
		set(OPENIMAGEIO_INCLUDE_DIRS ${OPENIMAGEIO}/include)
		set(OPENIMAGEIO_LIBRARIES ${OPENIMAGEIO}/lib/libOpenImageIO.a ${PNG_LIBRARIES} ${JPEG_LIBRARIES} ${TIFF_LIBRARY} ${OPENEXR_LIBRARIES} ${ZLIB_LIBRARIES})
		set(OPENIMAGEIO_LIBPATH ${OPENIMAGEIO}/lib ${JPEG_LIBPATH} ${PNG_LIBPATH} ${TIFF_LIBPATH} ${OPENEXR_LIBPATH} ${ZLIB_LIBPATH})
		set(OPENIMAGEIO_DEFINITIONS "-DOIIO_STATIC_BUILD")
	endif()

	if(WITH_OPENCOLORIO)
		set(OPENCOLORIO ${LIBDIR}/opencolorio)
		set(OPENCOLORIO_INCLUDE_DIRS ${OPENCOLORIO}/include)
		set(OPENCOLORIO_LIBRARIES OpenColorIO tinyxml yaml-cpp)
		set(OPENCOLORIO_LIBPATH ${OPENCOLORIO}/lib)
		set(OPENCOLORIO_DEFINITIONS "-DOCIO_STATIC_BUILD")
	endif()

	if(WITH_LLVM)
		set(LLVM_DIRECTORY ${LIBDIR}/llvm CACHE PATH	"Path to the LLVM installation")
		set(LLVM_VERSION "3.4" CACHE STRING	"Version of LLVM to use")
		if(EXISTS "${LLVM_DIRECTORY}/bin/llvm-config")
			set(LLVM_CONFIG "${LLVM_DIRECTORY}/bin/llvm-config")
		else()
			set(LLVM_CONFIG llvm-config)
		endif()
		execute_process(COMMAND ${LLVM_CONFIG} --version
		                OUTPUT_VARIABLE LLVM_VERSION
		                OUTPUT_STRIP_TRAILING_WHITESPACE)
		execute_process(COMMAND ${LLVM_CONFIG} --prefix
		                OUTPUT_VARIABLE LLVM_DIRECTORY
		                OUTPUT_STRIP_TRAILING_WHITESPACE)
		execute_process(COMMAND ${LLVM_CONFIG} --libdir
		                OUTPUT_VARIABLE LLVM_LIBPATH
		                OUTPUT_STRIP_TRAILING_WHITESPACE)
		find_library(LLVM_LIBRARY
		             NAMES LLVMAnalysis # first of a whole bunch of libs to get
		             PATHS ${LLVM_LIBPATH})

		if(LLVM_LIBRARY AND LLVM_DIRECTORY AND LLVM_LIBPATH)
			if(LLVM_STATIC)
				# if static LLVM libraries were requested, use llvm-config to generate
				# the list of what libraries we need, and substitute that in the right
				# way for LLVM_LIBRARY.
				execute_process(COMMAND ${LLVM_CONFIG} --libfiles
				                OUTPUT_VARIABLE LLVM_LIBRARY
				                OUTPUT_STRIP_TRAILING_WHITESPACE)
				string(REPLACE " " ";" LLVM_LIBRARY ${LLVM_LIBRARY})
			else()
				set(PLATFORM_LINKFLAGS "${PLATFORM_LINKFLAGS} -lLLVM-3.4")
			endif()
		else()
			message(FATAL_ERROR "LLVM not found.")
		endif()
	endif()

	if(WITH_CYCLES_OSL)
		set(CYCLES_OSL ${LIBDIR}/osl CACHE PATH "Path to OpenShadingLanguage installation")
	
		find_library(OSL_LIB_EXEC NAMES oslexec PATHS ${CYCLES_OSL}/lib)
		find_library(OSL_LIB_COMP NAMES oslcomp PATHS ${CYCLES_OSL}/lib)
		find_library(OSL_LIB_QUERY NAMES oslquery PATHS ${CYCLES_OSL}/lib)
		# WARNING! depends on correct order of OSL libs linking
		list(APPEND OSL_LIBRARIES ${OSL_LIB_COMP} -force_load ${OSL_LIB_EXEC} ${OSL_LIB_QUERY})
		find_path(OSL_INCLUDES OSL/oslclosure.h PATHS ${CYCLES_OSL}/include)
		find_program(OSL_COMPILER NAMES oslc PATHS ${CYCLES_OSL}/bin)
	
		if(OSL_INCLUDES AND OSL_LIBRARIES AND OSL_COMPILER)
			set(OSL_FOUND TRUE)
		else()
			message(STATUS "OSL not found")
		endif()
	endif()
	
	if(WITH_OPENMP)
		execute_process(COMMAND ${CMAKE_C_COMPILER} --version OUTPUT_VARIABLE COMPILER_VENDOR)
		string(SUBSTRING "${COMPILER_VENDOR}" 0 5 VENDOR_NAME) # truncate output
		if(${VENDOR_NAME} MATCHES "Apple") # Apple does not support OpenMP reliable with gcc and not with clang
			set(WITH_OPENMP OFF)
		else() # vanilla gcc or clang_omp support OpenMP
			message(STATUS "Using special OpenMP enabled compiler !") # letting find_package(OpenMP) module work for gcc
			if(CMAKE_C_COMPILER_ID MATCHES "Clang") # clang-omp in darwin libs
				set(OPENMP_FOUND ON)
				set(OpenMP_C_FLAGS "-fopenmp" CACHE STRING "C compiler flags for OpenMP parallization" FORCE)
				set(OpenMP_CXX_FLAGS "-fopenmp" CACHE STRING "C++ compiler flags for OpenMP parallization" FORCE)
				include_directories(${LIBDIR}/openmp/include)
				link_directories(${LIBDIR}/openmp/lib)
				# This is a workaround for our helperbinaries ( datatoc, masgfmt, ... ),
				# They are linked also to omp lib, so we need it in builddir for runtime exexcution, TODO: remove all unneeded dependencies from these
				execute_process(COMMAND ditto -arch ${CMAKE_OSX_ARCHITECTURES} ${LIBDIR}/openmp/lib/libiomp5.dylib ${CMAKE_BINARY_DIR}/bin/libiomp5.dylib) # for intermediate binaries, lib id @loader_path
			endif()
		endif()
	endif()

	set(EXETYPE MACOSX_BUNDLE)

	set(CMAKE_C_FLAGS_DEBUG "-fno-strict-aliasing -g")
	set(CMAKE_CXX_FLAGS_DEBUG "-fno-strict-aliasing -g")
	if(CMAKE_OSX_ARCHITECTURES MATCHES "x86_64" OR CMAKE_OSX_ARCHITECTURES MATCHES "i386")
		set(CMAKE_CXX_FLAGS_RELEASE "-O2 -mdynamic-no-pic -msse -msse2 -msse3 -mssse3")
		set(CMAKE_C_FLAGS_RELEASE "-O2 -mdynamic-no-pic  -msse -msse2 -msse3 -mssse3")
		if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
			set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -ftree-vectorize  -fvariable-expansion-in-unroller")
			set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -ftree-vectorize  -fvariable-expansion-in-unroller")
		endif()
	else()
		set(CMAKE_C_FLAGS_RELEASE "-mdynamic-no-pic -fno-strict-aliasing")
		set(CMAKE_CXX_FLAGS_RELEASE "-mdynamic-no-pic -fno-strict-aliasing")
	endif()

	if(${XCODE_VERSION} VERSION_EQUAL 5 OR ${XCODE_VERSION} VERSION_GREATER 5)
		# Xcode 5 is always using CLANG, which has too low template depth of 128 for libmv
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ftemplate-depth=1024")
	endif()
	# Get rid of eventually clashes, we export some symbols explicite as local
	set(PLATFORM_LINKFLAGS "${PLATFORM_LINKFLAGS} -Xlinker -unexported_symbols_list -Xlinker ${CMAKE_SOURCE_DIR}/source/creator/osx_locals.map")
endif()

#-----------------------------------------------------------------------------
# Common.

if(APPLE OR WIN32)
	if(NOT EXISTS "${LIBDIR}/")
		message(FATAL_ERROR "Apple and Windows require pre-compiled libs at: '${LIBDIR}'")
	endif()
endif()

if(NOT WITH_FFTW3 AND WITH_MOD_OCEANSIM)
	message(FATAL_ERROR "WITH_MOD_OCEANSIM requires WITH_FFTW3 to be ON")
endif()

if(WITH_CYCLES)
	if(NOT WITH_OPENIMAGEIO)
		message(FATAL_ERROR "Cycles requires WITH_OPENIMAGEIO, the library may not have been found. Configure OIIO or disable WITH_CYCLES")
	endif()
	if(NOT WITH_BOOST)
		message(FATAL_ERROR "Cycles requires WITH_BOOST, the library may not have been found. Configure BOOST or disable WITH_CYCLES")
	endif()

	if(WITH_CYCLES_OSL)
		if(NOT WITH_LLVM)
			message(FATAL_ERROR "Cycles OSL requires WITH_LLVM, the library may not have been found. Configure LLVM or disable WITH_CYCLES_OSL")
		endif()
	endif()
endif()

if(WITH_INTERNATIONAL)
	if(NOT WITH_BOOST)
		message(FATAL_ERROR "Internationalization requires WITH_BOOST, the library may not have been found. Configure BOOST or disable WITH_INTERNATIONAL")
	endif()
endif()

# See TEST_SSE_SUPPORT() for how this is defined.

if(WITH_RAYOPTIMIZATION)
	if(SUPPORT_SSE_BUILD)
		set(PLATFORM_CFLAGS " ${COMPILER_SSE_FLAG} ${PLATFORM_CFLAGS}")
		add_definitions(-D__SSE__ -D__MMX__)
	endif()
	if(SUPPORT_SSE2_BUILD)
		set(PLATFORM_CFLAGS " ${COMPILER_SSE2_FLAG} ${PLATFORM_CFLAGS}")
		add_definitions(-D__SSE2__)
		if(NOT SUPPORT_SSE_BUILD) # dont double up
			add_definitions(-D__MMX__)
		endif()
	endif()
endif()


# set the endian define
if(MSVC)
	# for some reason this fails on msvc
	add_definitions(-D__LITTLE_ENDIAN__)
	
# OSX-Note: as we do crosscompiling with specific set architecture,
# endianess-detection and autosetting is counterproductive
# so we just set endianess according CMAKE_OSX_ARCHITECTURES

elseif(CMAKE_OSX_ARCHITECTURES MATCHES i386 OR CMAKE_OSX_ARCHITECTURES MATCHES x86_64)
	add_definitions(-D__LITTLE_ENDIAN__)
elseif(CMAKE_OSX_ARCHITECTURES MATCHES ppc OR CMAKE_OSX_ARCHITECTURES MATCHES ppc64)		
	add_definitions(-D__BIG_ENDIAN__)
	
else()
	include(TestBigEndian)
	test_big_endian(_SYSTEM_BIG_ENDIAN)
	if(_SYSTEM_BIG_ENDIAN)
		add_definitions(-D__BIG_ENDIAN__)
	else()
		add_definitions(-D__LITTLE_ENDIAN__)
	endif()
	unset(_SYSTEM_BIG_ENDIAN)
endif()


if(WITH_IMAGE_OPENJPEG)
	if(WITH_SYSTEM_OPENJPEG)
		# dealt with above
	else()
		set(OPENJPEG_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/extern/libopenjpeg")
		add_definitions(-DOPJ_STATIC)
	endif()
endif()

if(WITH_IMAGE_REDCODE)
	set(REDCODE ${CMAKE_SOURCE_DIR}/extern)
	set(REDCODE_INC ${REDCODE})
endif()


#-----------------------------------------------------------------------------
# Configure OpenGL.

find_package(OpenGL)
blender_include_dirs_sys("${OPENGL_INCLUDE_DIR}")

if(WITH_GLU)
	list(APPEND BLENDER_GL_LIBRARIES "${OPENGL_glu_LIBRARY}")
	list(APPEND GL_DEFINITIONS -DWITH_GLU)
endif()

if(WITH_SYSTEM_GLES)
	find_package_wrapper(OpenGLES)
endif()

if(WITH_GL_PROFILE_COMPAT OR WITH_GL_PROFILE_CORE)
	list(APPEND BLENDER_GL_LIBRARIES "${OPENGL_gl_LIBRARY}")

elseif(WITH_GL_PROFILE_ES20)
	if(WITH_SYSTEM_GLES)
		if(NOT OPENGLES_LIBRARY)
			message(FATAL_ERROR "Unable to find OpenGL ES libraries.  Install them or disable WITH_SYSTEM_GLES.")
		endif()

		list(APPEND BLENDER_GL_LIBRARIES OPENGLES_LIBRARY)

	else()
		set(OPENGLES_LIBRARY "" CACHE FILEPATH "OpenGL ES 2.0 library file")
		mark_as_advanced(OPENGLES_LIBRARY)

		list(APPEND BLENDER_GL_LIBRARIES "${OPENGLES_LIBRARY}")

		if (NOT OPENGLES_LIBRARY)
			message(FATAL_ERROR "To compile WITH_GL_EGL you need to set OPENGLES_LIBRARY to the file path of an OpenGL ES 2.0 library.")
		endif()

	endif()

	if(WIN32)
		# Setup paths to files needed to install and redistribute Windows Blender with OpenGL ES

		set(OPENGLES_DLL "" CACHE FILEPATH "OpenGL ES 2.0 redistributable DLL file")
		mark_as_advanced(OPENGLES_DLL)

		if(NOT OPENGLES_DLL)
			message(FATAL_ERROR "To compile WITH_GL_PROFILE_ES20 you need to set OPENGLES_DLL to the file path of an OpenGL ES 2.0 runtime dynamic link library (DLL).")
		endif()

		if(WITH_GL_ANGLE)
			list(APPEND GL_DEFINITIONS -DWITH_ANGLE)

			set(D3DCOMPILER_DLL "" CACHE FILEPATH "Direct3D Compiler redistributable DLL file (needed by ANGLE)")

			get_filename_component(D3DCOMPILER_FILENAME "${D3DCOMPILER_DLL}" NAME)
			list(APPEND GL_DEFINITIONS "-DD3DCOMPILER=\"\\\"${D3DCOMPILER_FILENAME}\\\"\"")

			mark_as_advanced(D3DCOMPILER_DLL)

			if (D3DCOMPILER_DLL STREQUAL "")
				message(FATAL_ERROR "To compile WITH_GL_ANGLE you need to set D3DCOMPILER_DLL to the file path of a copy of the DirectX redistributable DLL file: D3DCompiler_46.dll")
			endif()

		endif()

	endif()

endif()

if(WITH_GL_EGL)
	list(APPEND GL_DEFINITIONS -DWITH_GL_EGL)

	if(WITH_SYSTEM_GLES)
		if(NOT OPENGLES_EGL_LIBRARY)
			message(FATAL_ERROR "Unable to find OpenGL ES libraries.  Install them or disable WITH_SYSTEM_GLES.")
		endif()

		list(APPEND BLENDER_GL_LIBRARIES OPENGLES_EGL_LIBRARY)

	else()
		set(OPENGLES_EGL_LIBRARY "" CACHE FILEPATH "EGL library file")
		mark_as_advanced(OPENGLES_EGL_LIBRARY)

		list(APPEND BLENDER_GL_LIBRARIES "${OPENGLES_LIBRARY}" "${OPENGLES_EGL_LIBRARY}")

		if (NOT OPENGLES_EGL_LIBRARY)
			message(FATAL_ERROR "To compile WITH_GL_EGL you need to set OPENGLES_EGL_LIBRARY to the file path of an EGL library.")
		endif()

	endif()

	if(WIN32)
		# Setup paths to files needed to install and redistribute Windows Blender with OpenGL ES

		set(OPENGLES_EGL_DLL "" CACHE FILEPATH "EGL redistributable DLL file")
		mark_as_advanced(OPENGLES_EGL_DLL)

		if(NOT OPENGLES_EGL_DLL)
			message(FATAL_ERROR "To compile WITH_GL_EGL you need to set OPENGLES_EGL_DLL to the file path of an EGL runtime dynamic link library (DLL).")
		endif()

	endif()

endif()

if(WITH_GL_PROFILE_COMPAT)
	list(APPEND GL_DEFINITIONS -DWITH_GL_PROFILE_COMPAT)
endif()

if(WITH_GL_PROFILE_CORE)
	list(APPEND GL_DEFINITIONS -DWITH_GL_PROFILE_CORE)
endif()

if(WITH_GL_PROFILE_ES20)
	list(APPEND GL_DEFINITIONS -DWITH_GL_PROFILE_ES20)
endif()

if(WITH_GL_EGL)
	list(APPEND GL_DEFINITIONS -DWITH_EGL)
endif()

#-----------------------------------------------------------------------------
# Configure OpenMP.
if(WITH_OPENMP)
	find_package(OpenMP)
	if(OPENMP_FOUND)
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
	else()
		set(WITH_OPENMP OFF)
	endif()

	mark_as_advanced(
		OpenMP_C_FLAGS
		OpenMP_CXX_FLAGS
	)
endif()

#-----------------------------------------------------------------------------
# Configure GLEW

if(WITH_GLEW_MX)
	list(APPEND GL_DEFINITIONS -DWITH_GLEW_MX)
endif()

if(WITH_SYSTEM_GLEW)
	find_package(GLEW)

	# Note: There is an assumption here that the system GLEW is not a static library.

	if(NOT GLEW_FOUND)
		message(FATAL_ERROR "GLEW is required to build Blender. Install it or disable WITH_SYSTEM_GLEW.")
	endif()

	if(WITH_GLEW_MX)
		set(BLENDER_GLEW_LIBRARIES ${GLEW_MX_LIBRARY})
	else()
		set(BLENDER_GLEW_LIBRARIES ${GLEW_LIBRARY})
	endif()
else()
	if(WITH_GLEW_ES)
		set(GLEW_INCLUDE_PATH "${CMAKE_SOURCE_DIR}/extern/glew-es/include")

		list(APPEND GL_DEFINITIONS -DGLEW_STATIC -DWITH_GLEW_ES)

		# These definitions remove APIs from glew.h, making GLEW smaller, and catching unguarded API usage
		if(NOT WITH_GL_PROFILE_ES20)
			# No ES functions are needed
			list(APPEND GL_DEFINITIONS -DGLEW_NO_ES)
		elseif(NOT (WITH_GL_PROFILE_CORE OR WITH_GL_PROFILE_COMPAT))
			# ES is enabled, but the other functions are all disabled
			list(APPEND GL_DEFINITIONS -DGLEW_ES_ONLY)
		endif()

		if(WITH_GL_PROFILE_ES20)
			if(WITH_GL_EGL)
				list(APPEND GL_DEFINITIONS -DGLEW_USE_LIB_ES20)
			endif()

			# ToDo: This is an experiment to eliminate ES 1 symbols,
			# GLEW doesn't really properly provide this level of control
			# (for example, without modification it eliminates too many symbols)
			# so there are lots of modifications to GLEW to make this work,
			# and no attempt to make it work beyond Blender at this point.
			list(APPEND GL_DEFINITIONS -DGL_ES_VERSION_1_0=0 -DGL_ES_VERSION_CL_1_1=0 -DGL_ES_VERSION_CM_1_1=0)
		endif()

		if(WITH_GL_EGL)
			list(APPEND GL_DEFINITIONS -DGLEW_INC_EGL)
		endif()

		set(BLENDER_GLEW_LIBRARIES extern_glew_es bf_intern_glew_mx)

	else()
		set(GLEW_INCLUDE_PATH "${CMAKE_SOURCE_DIR}/extern/glew/include")

		list(APPEND GL_DEFINITIONS -DGLEW_STATIC)

		# This won't affect the non-experimental glew library, 
		# but is used for conditional compilation elsewhere.
		list(APPEND GL_DEFINITIONS -DGLEW_NO_ES)

		set(BLENDER_GLEW_LIBRARIES extern_glew)

	endif()

endif()

if(NOT WITH_GLU)
	list(APPEND GL_DEFINITIONS -DGLEW_NO_GLU)
endif()

#-----------------------------------------------------------------------------
# Configure Bullet

if(WITH_BULLET AND WITH_SYSTEM_BULLET)
	find_package(Bullet)
	if(NOT BULLET_FOUND)
		set(WITH_BULLET OFF)
	endif()
else()
	set(BULLET_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/extern/bullet2/src")
	# set(BULLET_LIBRARIES "")
endif()

#-----------------------------------------------------------------------------
# Configure Python.

if(WITH_PYTHON_MODULE)
	add_definitions(-DPy_ENABLE_SHARED)
endif()

#-----------------------------------------------------------------------------
# Extra compile flags

if(CMAKE_COMPILER_IS_GNUCC)

	ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_ALL -Wall)
	ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_CAST_ALIGN -Wcast-align)
	ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_ERROR_DECLARATION_AFTER_STATEMENT -Werror=declaration-after-statement)
	ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_ERROR_IMPLICIT_FUNCTION_DECLARATION -Werror=implicit-function-declaration)
	ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_ERROR_RETURN_TYPE  -Werror=return-type)
	# system headers sometimes do this, disable for now, was: -Werror=strict-prototypes
	ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_STRICT_PROTOTYPES  -Wstrict-prototypes)
	ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_MISSING_PROTOTYPES -Wmissing-prototypes)
	ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_NO_CHAR_SUBSCRIPTS -Wno-char-subscripts)
	ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_NO_UNKNOWN_PRAGMAS -Wno-unknown-pragmas)
	ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_POINTER_ARITH -Wpointer-arith)
	ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_UNUSED_PARAMETER -Wunused-parameter)
	ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_WRITE_STRINGS -Wwrite-strings)
	ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_LOGICAL_OP -Wlogical-op)
	ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_UNDEF -Wundef)
	ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_INIT_SELF -Winit-self)  # needs -Wuninitialized
	ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_NO_NULL -Wnonnull)  # C only
	ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_MISSING_INCLUDE_DIRS -Wmissing-include-dirs)
	ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_NO_DIV_BY_ZERO -Wno-div-by-zero)
	ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_TYPE_LIMITS -Wtype-limits)

	# gcc 4.2 gives annoying warnings on every file with this
	if(NOT "${CMAKE_C_COMPILER_VERSION}" VERSION_LESS "4.3")
		ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_UNINITIALIZED -Wuninitialized)
	endif()

	# versions before gcc4.6 give many BLI_math warnings
	if(NOT "${CMAKE_C_COMPILER_VERSION}" VERSION_LESS "4.6")
		ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_REDUNDANT_DECLS       -Wredundant-decls)
		ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_REDUNDANT_DECLS -Wredundant-decls)
	endif()

	# disable because it gives warnings for printf() & friends.
	# ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_DOUBLE_PROMOTION -Wdouble-promotion -Wno-error=double-promotion)

	if(NOT APPLE)
		ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_NO_ERROR_UNUSED_BUT_SET_VARIABLE -Wno-error=unused-but-set-variable)
	endif()

	ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_ALL -Wall)
	ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_NO_INVALID_OFFSETOF -Wno-invalid-offsetof)
	ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_NO_SIGN_COMPARE -Wno-sign-compare)
	ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_LOGICAL_OP -Wlogical-op)
	ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_INIT_SELF -Winit-self)  # needs -Wuninitialized
	ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_MISSING_INCLUDE_DIRS -Wmissing-include-dirs)
	ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_NO_DIV_BY_ZERO -Wno-div-by-zero)
	ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_TYPE_LIMITS -Wtype-limits)

	# gcc 4.2 gives annoying warnings on every file with this
	if(NOT "${CMAKE_C_COMPILER_VERSION}" VERSION_LESS "4.3")
		ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_UNINITIALIZED -Wuninitialized)
	endif()

	# causes too many warnings
	if(NOT APPLE)
		ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_UNDEF -Wundef)
		ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_MISSING_DECLARATIONS -Wmissing-declarations)
	endif()

	# flags to undo strict flags
	ADD_CHECK_C_COMPILER_FLAG(CC_REMOVE_STRICT_FLAGS C_WARN_NO_DEPRECATED_DECLARATIONS -Wno-deprecated-declarations)
	ADD_CHECK_C_COMPILER_FLAG(CC_REMOVE_STRICT_FLAGS C_WARN_NO_UNUSED_PARAMETER        -Wno-unused-parameter)

	if(NOT APPLE)
		ADD_CHECK_C_COMPILER_FLAG(CC_REMOVE_STRICT_FLAGS C_WARN_NO_ERROR_UNUSED_BUT_SET_VARIABLE -Wno-error=unused-but-set-variable)
	endif()

elseif(CMAKE_C_COMPILER_ID MATCHES "Clang")

	if(APPLE AND WITH_OPENMP) # we need the Intel omp lib linked here to not fail all tests due presence of -fopenmp !
		set(CMAKE_REQUIRED_FLAGS "-L${LIBDIR}/openmp/lib -liomp5") # these are only used for the checks
	endif()

	# strange, clang complains these are not supported, but then yses them.
	ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_ALL -Wall)
	ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_ERROR_DECLARATION_AFTER_STATEMENT -Werror=declaration-after-statement)
	ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_ERROR_IMPLICIT_FUNCTION_DECLARATION -Werror=implicit-function-declaration)
	ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_ERROR_RETURN_TYPE  -Werror=return-type)
	ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_NO_AUTOLOGICAL_COMPARE -Wno-tautological-compare)
	ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_NO_UNKNOWN_PRAGMAS -Wno-unknown-pragmas)
	ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_NO_CHAR_SUBSCRIPTS -Wno-char-subscripts)
	ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_STRICT_PROTOTYPES  -Wstrict-prototypes)
	ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_MISSING_PROTOTYPES -Wmissing-prototypes)
	ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_UNUSED_PARAMETER -Wunused-parameter)

	ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_ALL -Wall)
	ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_NO_AUTOLOGICAL_COMPARE -Wno-tautological-compare)
	ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_NO_UNKNOWN_PRAGMAS     -Wno-unknown-pragmas)
	ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_NO_CHAR_SUBSCRIPTS     -Wno-char-subscripts)
	ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_NO_OVERLOADED_VIRTUAL  -Wno-overloaded-virtual)  # we get a lot of these, if its a problem a dev needs to look into it.
	ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_NO_SIGN_COMPARE        -Wno-sign-compare)
	ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_NO_INVALID_OFFSETOF    -Wno-invalid-offsetof)

	# gives too many unfixable warnings
	# ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_UNUSED_MACROS      -Wunused-macros)
	# ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_UNUSED_MACROS          -Wunused-macros)

	# flags to undo strict flags
	ADD_CHECK_C_COMPILER_FLAG(CC_REMOVE_STRICT_FLAGS C_WARN_NO_UNUSED_PARAMETER -Wno-unused-parameter)
	ADD_CHECK_C_COMPILER_FLAG(CC_REMOVE_STRICT_FLAGS C_WARN_NO_UNUSED_MACROS    -Wno-unused-macros)

	ADD_CHECK_C_COMPILER_FLAG(CC_REMOVE_STRICT_FLAGS C_WARN_NO_MISSING_VARIABLE_DECLARATIONS -Wno-missing-variable-declarations)
	ADD_CHECK_C_COMPILER_FLAG(CC_REMOVE_STRICT_FLAGS C_WARN_NO_INCOMPAT_PTR_DISCARD_QUAL -Wno-incompatible-pointer-types-discards-qualifiers)
	ADD_CHECK_C_COMPILER_FLAG(CC_REMOVE_STRICT_FLAGS C_WARN_NO_UNUSED_FUNCTION -Wno-unused-function)
	ADD_CHECK_C_COMPILER_FLAG(CC_REMOVE_STRICT_FLAGS C_WARN_NO_INT_TO_VOID_POINTER_CAST -Wno-int-to-void-pointer-cast)
	ADD_CHECK_C_COMPILER_FLAG(CC_REMOVE_STRICT_FLAGS C_WARN_NO_MISSING_PROTOTYPES -Wno-missing-prototypes)
	ADD_CHECK_C_COMPILER_FLAG(CC_REMOVE_STRICT_FLAGS C_WARN_NO_DUPLICATE_ENUM -Wno-duplicate-enum)
	ADD_CHECK_C_COMPILER_FLAG(CC_REMOVE_STRICT_FLAGS C_WARN_NO_UNDEF -Wno-undef)
	ADD_CHECK_C_COMPILER_FLAG(CC_REMOVE_STRICT_FLAGS C_WARN_NO_MISSING_NORETURN -Wno-missing-noreturn)

	ADD_CHECK_CXX_COMPILER_FLAG(CC_REMOVE_STRICT_FLAGS CXX_WARN_NO_UNUSED_PRIVATE_FIELD -Wno-unused-private-field)
	ADD_CHECK_CXX_COMPILER_FLAG(CC_REMOVE_STRICT_FLAGS CXX_WARN_NO_CXX11_NARROWING -Wno-c++11-narrowing)
	ADD_CHECK_CXX_COMPILER_FLAG(CC_REMOVE_STRICT_FLAGS CXX_WARN_NO_NON_VIRTUAL_DTOR -Wno-non-virtual-dtor)
	ADD_CHECK_CXX_COMPILER_FLAG(CC_REMOVE_STRICT_FLAGS CXX_WARN_NO_UNUSED_MACROS -Wno-unused-macros)
	ADD_CHECK_CXX_COMPILER_FLAG(CC_REMOVE_STRICT_FLAGS CXX_WARN_NO_REORDER -Wno-reorder)

elseif(CMAKE_C_COMPILER_ID MATCHES "Intel")

	ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_ALL -Wall)
	ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_POINTER_ARITH -Wpointer-arith)
	ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_NO_UNKNOWN_PRAGMAS -Wno-unknown-pragmas)

	ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_ALL -Wall)
	ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_NO_INVALID_OFFSETOF -Wno-invalid-offsetof)
	ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_NO_SIGN_COMPARE -Wno-sign-compare)

	# disable numbered, false positives
	set(C_WARNINGS "${C_WARNINGS} -wd188,186,144,913,556")
	set(CXX_WARNINGS "${CXX_WARNINGS} -wd188,186,144,913,556")

endif()

# ensure python header is found since detection can fail, this could happen
# with _any_ library but since we used a fixed python version this tends to
# be most problematic.
if(WITH_PYTHON)
	if(NOT EXISTS "${PYTHON_INCLUDE_DIR}/Python.h")
		message(FATAL_ERROR "Missing: \"${PYTHON_INCLUDE_DIR}/Python.h\",\n"
							"Set the cache entry 'PYTHON_INCLUDE_DIR' to point "
							"to a valid python include path. Containing "
							"Python.h for python version \"${PYTHON_VERSION}\"")
	endif()

	if(WIN32)
		# pass, we have this in an archive to extract
	elseif(WITH_PYTHON_INSTALL AND WITH_PYTHON_INSTALL_NUMPY)
		find_python_package(numpy)
	endif()

	if(WIN32 OR APPLE)
		# pass, we have this in lib/python/site-packages
	elseif(WITH_PYTHON_INSTALL_REQUESTS)
		find_python_package(requests)
	endif()
endif()

if(WITH_GCC_MUDFLAP)
	set(PLATFORM_CFLAGS "${PLATFORM_CFLAGS} -fmudflapth -funwind-tables")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lpthread -lmudflapth -rdynamic")
	ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS     C_WARN_NO_ERROR_MUDFLAP   -Wno-error=mudflap)
	ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_NO_ERROR_MUDFLAP -Wno-error=mudflap)
endif()

# Include warnings first, so its possible to disable them with user defined flags
# eg: -Wno-uninitialized
set(CMAKE_C_FLAGS "${C_WARNINGS} ${CMAKE_C_FLAGS} ${PLATFORM_CFLAGS}")
set(CMAKE_CXX_FLAGS "${CXX_WARNINGS} ${CMAKE_CXX_FLAGS} ${PLATFORM_CFLAGS}")

# defined above, platform spesific but shared names
mark_as_advanced(
	CYCLES_OSL
	OSL_LIB_EXEC
	OSL_COMPILER
	OSL_LIB_COMP
	OSL_LIB_QUERY
	OSL_INCLUDES
)

mark_as_advanced(
	LLVM_CONFIG
	LLVM_DIRECTORY
	LLVM_LIBRARY
	LLVM_VERSION
)

#-------------------------------------------------------------------------------
# Global Defines

# better not set includes here but this debugging option is off by default.
if(WITH_CXX_GUARDEDALLOC)
	include_directories(${CMAKE_SOURCE_DIR}/intern/guardedalloc)
	add_definitions(-DWITH_CXX_GUARDEDALLOC)
endif()

if(WITH_ASSERT_ABORT)
	add_definitions(-DWITH_ASSERT_ABORT)
endif()

# message(STATUS "Using CFLAGS: ${CMAKE_C_FLAGS}")
# message(STATUS "Using CXXFLAGS: ${CMAKE_CXX_FLAGS}")

#-----------------------------------------------------------------------------
# Libraries

if(WITH_GTESTS)
	include(GTestTesting)
endif()

if(WITH_BLENDER OR WITH_PLAYER)
	add_subdirectory(intern)
	add_subdirectory(extern)

	# source after intern and extern to gather all
	# internal and external library information first, for test linking
	add_subdirectory(source)
elseif(WITH_CYCLES_STANDALONE)
	add_subdirectory(intern/cycles)
	add_subdirectory(extern/clew)
	add_subdirectory(extern/cuew)
	if(NOT WITH_SYSTEM_GLEW)
		add_subdirectory(extern/glew)
	endif()
endif()

#-----------------------------------------------------------------------------
# Blender Application
if(WITH_BLENDER)
	add_subdirectory(source/creator)
endif()


#-----------------------------------------------------------------------------
# Blender Player
if(WITH_PLAYER)
	add_subdirectory(source/blenderplayer)
endif()


#-----------------------------------------------------------------------------
# Testing
add_subdirectory(tests)


#-----------------------------------------------------------------------------
# CPack for generating packages
include(build_files/cmake/packaging.cmake)


#-----------------------------------------------------------------------------
# Print Final Configuration

if(FIRST_RUN)
	set(_config_msg "\n* Blender Configuration *")
	macro(info_cfg_option
		_setting)
		set(_msg "   * ${_setting}")
		string(LENGTH "${_msg}" _len)
		while("32" GREATER "${_len}")
			set(_msg "${_msg} ")
			 math(EXPR _len "${_len} + 1")
		endwhile()

		set(_config_msg "${_config_msg}\n${_msg}${${_setting}}")
	endmacro()

	macro(info_cfg_text
		_text)
		set(_config_msg "${_config_msg}\n\n  ${_text}")


	endmacro()

	message(STATUS "C Compiler:   \"${CMAKE_C_COMPILER_ID}\"")
	message(STATUS "C++ Compiler: \"${CMAKE_CXX_COMPILER_ID}\"")

	info_cfg_text("Build Options:")
	info_cfg_option(WITH_GAMEENGINE)
	info_cfg_option(WITH_PLAYER)
	info_cfg_option(WITH_BULLET)
	info_cfg_option(WITH_IK_SOLVER)
	info_cfg_option(WITH_IK_ITASC)
	info_cfg_option(WITH_OPENCOLLADA)
	info_cfg_option(WITH_FFTW3)
	info_cfg_option(WITH_INTERNATIONAL)
	info_cfg_option(WITH_INPUT_NDOF)
	info_cfg_option(WITH_CYCLES)
	info_cfg_option(WITH_FREESTYLE)
	info_cfg_option(WITH_OPENCOLORIO)

	info_cfg_text("Compiler Options:")
	info_cfg_option(WITH_BUILDINFO)
	info_cfg_option(WITH_OPENMP)
	info_cfg_option(WITH_RAYOPTIMIZATION)

	info_cfg_text("System Options:")
	info_cfg_option(WITH_INSTALL_PORTABLE)
	info_cfg_option(WITH_X11_XF86VMODE)
	info_cfg_option(WITH_X11_XINPUT)
	info_cfg_option(WITH_MEM_JEMALLOC)
	info_cfg_option(WITH_MEM_VALGRIND)
	info_cfg_option(WITH_SYSTEM_GLEW)
	info_cfg_option(WITH_SYSTEM_OPENJPEG)

	info_cfg_text("Image Formats:")
	info_cfg_option(WITH_OPENIMAGEIO)
	info_cfg_option(WITH_IMAGE_CINEON)
	info_cfg_option(WITH_IMAGE_DDS)
	info_cfg_option(WITH_IMAGE_HDR)
	info_cfg_option(WITH_IMAGE_OPENEXR)
	info_cfg_option(WITH_IMAGE_OPENJPEG)
	info_cfg_option(WITH_IMAGE_REDCODE)
	info_cfg_option(WITH_IMAGE_TIFF)

	info_cfg_text("Audio:")
	info_cfg_option(WITH_OPENAL)
	info_cfg_option(WITH_SDL)
	info_cfg_option(WITH_JACK)
	info_cfg_option(WITH_JACK_DYNLOAD)
	info_cfg_option(WITH_CODEC_AVI)
	info_cfg_option(WITH_CODEC_FFMPEG)
	info_cfg_option(WITH_CODEC_SNDFILE)

	info_cfg_text("Compression:")
	info_cfg_option(WITH_LZMA)
	info_cfg_option(WITH_LZO)

	info_cfg_text("Python:")
	info_cfg_option(WITH_PYTHON_INSTALL)
	info_cfg_option(WITH_PYTHON_INSTALL_NUMPY)
	info_cfg_option(WITH_PYTHON_MODULE)
	info_cfg_option(WITH_PYTHON_SAFETY)
	if(APPLE)
		info_cfg_option(WITH_PYTHON_FRAMEWORK)
	endif()

	info_cfg_text("Modifiers:")
	info_cfg_option(WITH_MOD_BOOLEAN)
	info_cfg_option(WITH_MOD_REMESH)
	info_cfg_option(WITH_MOD_FLUID)
	info_cfg_option(WITH_MOD_OCEANSIM)

	info_cfg_text("OpenGL:")
	info_cfg_option(WITH_GLEW_ES)
	info_cfg_option(WITH_GLU)
	info_cfg_option(WITH_GL_EGL)
	info_cfg_option(WITH_GL_PROFILE_COMPAT)
	info_cfg_option(WITH_GL_PROFILE_CORE)
	info_cfg_option(WITH_GL_PROFILE_ES20)
	if(WIN32)
		info_cfg_option(WITH_GL_ANGLE)
	endif()

	info_cfg_text("Other:")
	info_cfg_option(WITH_OPENNL)

	# debug
	message(STATUS "HAVE_STDBOOL_H = ${HAVE_STDBOOL_H}")

	info_cfg_text("")

	message("${_config_msg}")
endif()
