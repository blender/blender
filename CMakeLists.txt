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

if(NOT EXECUTABLE_OUTPUT_PATH)
	set(FIRST_RUN TRUE)
else()
	set(FIRST_RUN FALSE)
endif()

# this starts out unset
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/build_files/cmake/Modules")

# avoid having empty buildtype
set(CMAKE_BUILD_TYPE_INIT "Release")

# quiet output for Makefiles, 'make -s' helps too
# set_property(GLOBAL PROPERTY RULE_MESSAGES OFF)

# global compile definitions since add_definitions() adds for all.

if(NOT (${CMAKE_VERSION} VERSION_LESS 3.0))
	set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS
		$<$<CONFIG:Debug>:DEBUG;_DEBUG>
		$<$<CONFIG:Release>:NDEBUG>
		$<$<CONFIG:MinSizeRel>:NDEBUG>
		$<$<CONFIG:RelWithDebInfo>:NDEBUG>
	)
else()
	# keep until CMake-3.0 is min requirement
	set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS_DEBUG           DEBUG _DEBUG)
	set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS_RELEASE         NDEBUG)
	set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS_MINSIZEREL      NDEBUG)
	set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS_RELWITHDEBINFO  NDEBUG)
endif()

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


#-----------------------------------------------------------------------------
# Platform Specific Defaults

# list of var-names
set(_init_vars)

# initialize to ON
macro(option_defaults_init)
	foreach(_var ${ARGV})
		set(${_var} ON)
		list(APPEND _init_vars "${_var}")
	endforeach()
	unset(_var)
endmacro()

# remove from namespace
macro(option_defaults_clear)
	foreach(_var ${_init_vars})
		unset(${_var})
	endforeach()
	unset(_var)
	unset(_init_vars)
endmacro()


# values to initialize WITH_****
option_defaults_init(
	_init_BUILDINFO
	_init_CODEC_FFMPEG
	_init_CYCLES_OSL
	_init_IMAGE_OPENEXR
	_init_INPUT_NDOF
	_init_JACK
	_init_OPENCOLLADA
	_init_OPENCOLORIO
	_init_SDL
	_init_FFTW3
	_init_GAMEENGINE
	_init_OPENSUBDIV
)

# customize...
if(UNIX AND NOT APPLE)
	# some of these libraries are problematic on Linux
	# disable less important dependencies by default
	set(_init_CODEC_FFMPEG                   OFF)
	set(_init_CYCLES_OSL                     OFF)
	set(_init_IMAGE_OPENEXR                  OFF)
	set(_init_JACK                           OFF)
	set(_init_OPENCOLLADA                    OFF)
	set(_init_OPENCOLORIO                    OFF)
	set(_init_SDL                            OFF)
	set(_init_FFTW3                          OFF)
	set(_init_OPENSUBDIV                     OFF)
elseif(WIN32)
	set(_init_JACK                           OFF)
elseif(APPLE)
	set(_init_JACK                           OFF)
	set(_init_OPENSUBDIV                     OFF)
endif()


#-----------------------------------------------------------------------------
# Options

# First platform spesific non-cached vars
if(UNIX AND NOT APPLE)
	set(WITH_X11 ON)
endif()

# Blender internal features
option(WITH_BLENDER "Build blender (disable to build only the blender player)" ON)
mark_as_advanced(WITH_BLENDER)

option(WITH_INTERNATIONAL "Enable I18N (International fonts and text)" ON)

option(WITH_PYTHON        "Enable Embedded Python API  (only disable for development)" ON)
option(WITH_PYTHON_SECURITY "Disables execution of scripts within blend files by default" ON) 
mark_as_advanced(WITH_PYTHON)  # dont want people disabling this unless they really know what they are doing.
mark_as_advanced(WITH_PYTHON_SECURITY)  # some distributions see this as a security issue, rather than have them patch it, make a build option.

option(WITH_PYTHON_SAFETY "Enable internal API error checking to track invalid data to prevent crash on access (at the expense of some effeciency, only enable for development)." OFF)
mark_as_advanced(WITH_PYTHON_SAFETY)
option(WITH_PYTHON_MODULE "Enable building as a python module which runs without a user interface, like running regular blender in background mode (experimental, only enable for development), installs to PYTHON_SITE_PACKAGES (or CMAKE_INSTALL_PREFIX if WITH_INSTALL_PORTABLE is enabled)." OFF)
if(APPLE)
	option(WITH_PYTHON_FRAMEWORK "Enable building using the Python available in the framework (OSX only)" OFF)
endif()

option(WITH_BUILDINFO     "Include extra build details (only disable for development & faster builds)" ${_init_BUILDINFO})
if(${CMAKE_VERSION} VERSION_LESS 2.8.8)
	# add_library OBJECT arg unsupported
	set(WITH_BUILDINFO OFF)
endif()
set(BUILDINFO_OVERRIDE_DATE "" CACHE STRING "Use instead of the current date for reproducable builds (empty string disables this option)")
set(BUILDINFO_OVERRIDE_TIME "" CACHE STRING "Use instead of the current time for reproducable builds (empty string disables this option)")
mark_as_advanced(BUILDINFO_OVERRIDE_DATE)
mark_as_advanced(BUILDINFO_OVERRIDE_TIME)

option(WITH_IK_ITASC      "Enable ITASC IK solver (only disable for development & for incompatible C++ compilers)" ON)
option(WITH_IK_SOLVER     "Enable Legacy IK solver (only disable for development)" ON)
option(WITH_FFTW3         "Enable FFTW3 support (Used for smoke and audio effects)" ${_init_FFTW3})
option(WITH_BULLET        "Enable Bullet (Physics Engine)" ON)
option(WITH_SYSTEM_BULLET "Use the systems bullet library (currently unsupported due to missing features in upstream!)" )
mark_as_advanced(WITH_SYSTEM_BULLET)
option(WITH_GAMEENGINE    "Enable Game Engine" ${_init_GAMEENGINE})
option(WITH_PLAYER        "Build Player" OFF)
option(WITH_OPENCOLORIO   "Enable OpenColorIO color management" ${_init_OPENCOLORIO})

# Compositor
option(WITH_COMPOSITOR         "Enable the tile based nodal compositor" ON)

option(WITH_OPENSUBDIV    "Enable OpenSubdiv for surface subdivision" _init_OPENSUBDIV)

option(WITH_OPENVDB       "Enable features relying on OpenVDB" OFF)
option(WITH_OPENVDB_BLOSC "Enable blosc compression for OpenVDB, only enable if OpenVDB was built with blosc support" OFF)

# GHOST Windowing Library Options
option(WITH_GHOST_DEBUG   "Enable debugging output for the GHOST library" OFF)
mark_as_advanced(WITH_GHOST_DEBUG)

option(WITH_GHOST_SDL    "Enable building Blender against SDL for windowing rather than the native APIs" OFF)
mark_as_advanced(WITH_GHOST_SDL)

if(WITH_X11)
	option(WITH_GHOST_XDND    "Enable drag'n'drop support on X11 using XDND protocol" ON)
endif()

# Misc...
option(WITH_HEADLESS      "Build without graphical support (renderfarm, server mode only)" OFF)
mark_as_advanced(WITH_HEADLESS)

option(WITH_AUDASPACE    "Build with blenders audio library (only disable if you know what you're doing!)" ON)
option(WITH_SYSTEM_AUDASPACE "Build with external audaspace library installed on the system (only enable if you know what you're doing!)" OFF)
mark_as_advanced(WITH_AUDASPACE)
mark_as_advanced(WITH_SYSTEM_AUDASPACE)

if(NOT WITH_AUDASPACE)
	set(WITH_SYSTEM_AUDASPACE OFF)
endif()

option(WITH_OPENMP        "Enable OpenMP (has to be supported by the compiler)" ON)
if(UNIX AND NOT APPLE)
	option(WITH_OPENMP_STATIC "Link OpenMP statically (only used by the release environment)" OFF)
	mark_as_advanced(WITH_OPENMP_STATIC)
endif()

if(WITH_X11)
	option(WITH_X11_XINPUT    "Enable X11 Xinput (tablet support and unicode input)"  ON)
	option(WITH_X11_XF86VMODE "Enable X11 video mode switching"                       ON)
	option(WITH_X11_ALPHA     "Enable X11 transparent background"                     ON)
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
option(WITH_SYSTEM_OPENJPEG "Use the operating systems OpenJPEG library" OFF)

if(UNIX AND NOT APPLE)
	option(WITH_SYSTEM_EIGEN3 "Use the systems Eigen3 library" OFF)
endif()


# Modifiers
option(WITH_MOD_FLUID           "Enable Elbeem Modifier (Fluid Simulation)" ON)
option(WITH_MOD_SMOKE           "Enable Smoke Modifier (Smoke Simulation)" ON)
option(WITH_MOD_BOOLEAN         "Enable Boolean Modifier" ON)
option(WITH_MOD_REMESH          "Enable Remesh Modifier" ON)
# option(WITH_MOD_CLOTH_ELTOPO    "Enable Experimental cloth solver" OFF)  # this is now only available in a branch
# mark_as_advanced(WITH_MOD_CLOTH_ELTOPO)
option(WITH_MOD_OCEANSIM        "Enable Ocean Modifier" OFF)

# Image format support
option(WITH_OPENIMAGEIO         "Enable OpenImageIO Support (http://www.openimageio.org)" ON)
option(WITH_IMAGE_OPENEXR       "Enable OpenEXR Support (http://www.openexr.com)" ${_init_IMAGE_OPENEXR})
option(WITH_IMAGE_OPENJPEG      "Enable OpenJpeg Support (http://www.openjpeg.org)" ON)
option(WITH_IMAGE_TIFF          "Enable LibTIFF Support" ON)
option(WITH_IMAGE_DDS           "Enable DDS Image Support" ON)
option(WITH_IMAGE_CINEON        "Enable CINEON and DPX Image Support" ON)
option(WITH_IMAGE_HDR           "Enable HDR Image Support" ON)
option(WITH_IMAGE_FRAMESERVER   "Enable image FrameServer Support for rendering" ON)

# Audio/Video format support
option(WITH_CODEC_AVI           "Enable Blenders own AVI file support (raw/jpeg)" ON)
option(WITH_CODEC_FFMPEG        "Enable FFMPeg Support (http://ffmpeg.org)" ${_init_CODEC_FFMPEG})
option(WITH_CODEC_SNDFILE       "Enable libsndfile Support (http://www.mega-nerd.com/libsndfile)" OFF)

if(APPLE)
	option(WITH_CODEC_QUICKTIME     "Enable Quicktime Support" ON)
endif()

# 3D format support
# Disable opencollada when we don't have precompiled libs
option(WITH_OPENCOLLADA   "Enable OpenCollada Support (http://www.opencollada.org)" ${_init_OPENCOLLADA})

# Sound output
option(WITH_SDL           "Enable SDL for sound and joystick support" ${_init_SDL})
option(WITH_OPENAL        "Enable OpenAL Support (http://www.openal.org)" ON)
option(WITH_JACK          "Enable Jack Support (http://www.jackaudio.org)" ${_init_JACK})
if(UNIX AND NOT APPLE)
	option(WITH_JACK_DYNLOAD  "Enable runtime dynamic Jack libraries loading" OFF)
endif()
if(UNIX AND NOT APPLE)
	option(WITH_SDL_DYNLOAD  "Enable runtime dynamic SDL libraries loading" OFF)
endif()

# Compression
option(WITH_LZO           "Enable fast LZO compression (used for pointcache)" ON)
option(WITH_LZMA          "Enable best LZMA compression, (used for pointcache)" ON)
if(UNIX AND NOT APPLE)
	option(WITH_SYSTEM_LZO    "Use the system LZO library" OFF)
endif()

# Camera/motion tracking
option(WITH_LIBMV         "Enable Libmv structure from motion library" ON)
option(WITH_LIBMV_SCHUR_SPECIALIZATIONS "Enable fixed-size schur specializations." OFF)
mark_as_advanced(WITH_LIBMV_SCHUR_SPECIALIZATIONS)

# Freestyle
option(WITH_FREESTYLE     "Enable Freestyle (advanced edges rendering)" ON)

# Misc
if(WIN32)
	option(WITH_INPUT_IME "Enable Input Method Editor (IME) for complex Asian character input" ON)
endif()
option(WITH_INPUT_NDOF "Enable NDOF input devices (SpaceNavigator and friends)" ${_init_INPUT_NDOF})
option(WITH_RAYOPTIMIZATION	"Enable use of SIMD (SSE) optimizations for the raytracer" ON)
if(UNIX AND NOT APPLE)
	option(WITH_INSTALL_PORTABLE "Install redistributeable runtime, otherwise install into CMAKE_INSTALL_PREFIX" ON)
	option(WITH_STATIC_LIBS "Try to link with static libraries, as much as possible, to make blender more portable across distributions" OFF)
	if(WITH_STATIC_LIBS)
		option(WITH_BOOST_ICU "Boost uses ICU library (required for linking with static Boost built with libicu)." OFF)
		mark_as_advanced(WITH_BOOST_ICU)
	endif()
endif()

option(WITH_PYTHON_INSTALL       "Copy system python into the blender install folder" ON)
if(WITH_PYTHON_INSTALL)
	option(WITH_PYTHON_INSTALL_NUMPY "Copy system numpy into the blender install folder"  ON)
	set(PYTHON_NUMPY_PATH            "" CACHE PATH "Path to python site-packages or dist-packages containing 'numpy' module")
	mark_as_advanced(PYTHON_NUMPY_PATH)

	if(UNIX AND NOT APPLE)
		option(WITH_PYTHON_INSTALL_REQUESTS "Copy system requests into the blender install folder" ON)
		set(PYTHON_REQUESTS_PATH "" CACHE PATH "Path to python site-packages or dist-packages containing 'requests' module")
		mark_as_advanced(PYTHON_REQUESTS_PATH)
	endif()
endif()

option(WITH_CPU_SSE              "Enable SIMD instruction if they're detected on the host machine" ON)
mark_as_advanced(WITH_CPU_SSE)

# Cycles
option(WITH_CYCLES					"Enable Cycles Render Engine" ON)
option(WITH_CYCLES_STANDALONE		"Build Cycles standalone application" OFF)
option(WITH_CYCLES_STANDALONE_GUI	"Build Cycles standalone with GUI" OFF)
option(WITH_CYCLES_OSL				"Build Cycles with OSL support" ${_init_CYCLES_OSL})
option(WITH_CYCLES_CUDA_BINARIES	"Build Cycles CUDA binaries" OFF)
set(CYCLES_CUDA_BINARIES_ARCH sm_20 sm_21 sm_30 sm_35 sm_37 sm_50 sm_52 CACHE STRING "CUDA architectures to build binaries for")
mark_as_advanced(CYCLES_CUDA_BINARIES_ARCH)
unset(PLATFORM_DEFAULT)
option(WITH_CYCLES_LOGGING	"Build Cycles with logging support" ON)
option(WITH_CYCLES_DEBUG	"Build Cycles with extra debug capabilities" OFF)
option(WITH_CYCLES_NATIVE_ONLY	"Build Cycles with native kernel only (which fits current CPU, use for development only)" OFF)
mark_as_advanced(WITH_CYCLES_LOGGING)
mark_as_advanced(WITH_CYCLES_DEBUG)
mark_as_advanced(WITH_CYCLES_NATIVE_ONLY)

option(WITH_CUDA_DYNLOAD "Dynamically load CUDA libraries at runtime" ON)
mark_as_advanced(WITH_CUDA_DYNLOAD)

# LLVM
option(WITH_LLVM					"Use LLVM" OFF)
if(APPLE)
	option(LLVM_STATIC					"Link with LLVM static libraries" ON) # we prefer static llvm build on Apple, dyn build possible though
else()
	option(LLVM_STATIC					"Link with LLVM static libraries" OFF)
endif()
mark_as_advanced(LLVM_STATIC)

# disable for now, but plan to support on all platforms eventually
option(WITH_MEM_JEMALLOC   "Enable malloc replacement (http://www.canonware.com/jemalloc)" ON)
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

# Unit testsing
option(WITH_GTESTS "Enable GTest unit testing" OFF)


# Documentation
if(UNIX AND NOT APPLE)
	option(WITH_DOC_MANPAGE "Create a manual page (Unix manpage)" OFF)
endif()


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

if(WITH_GL_PROFILE_COMPAT)
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

if(WIN32)
	getDefaultWindowsPrefixBase(CMAKE_GENERIC_PROGRAM_FILES)
	set(CPACK_INSTALL_PREFIX ${CMAKE_GENERIC_PROGRAM_FILES}/${})
endif()

# Experimental support of C11 and C++11
#
# We default options to whatever default standard in the current compiler.
if(CMAKE_COMPILER_IS_GNUCC AND (NOT "${CMAKE_C_COMPILER_VERSION}" VERSION_LESS "6.0") AND (NOT WITH_CXX11))
	set(_c11_init ON)
	set(_cxx11_init ON)
else()
	set(_c11_init OFF)
	set(_cxx11_init OFF)
endif()

option(WITH_C11 "Build with C11 standard enabled, for development use only!" ${_c11_init})
mark_as_advanced(WITH_C11)
option(WITH_CXX11 "Build with C++11 standard enabled, for development use only!" ${_cxx11_init})
mark_as_advanced(WITH_CXX11)

# Dependency graph
option(WITH_LEGACY_DEPSGRAPH "Build Blender with legacy dependency graph" ON)
mark_as_advanced(WITH_LEGACY_DEPSGRAPH)

# avoid using again
option_defaults_clear()

# end option(...)



# By default we want to install to the directory we are compiling our executables
# unless specified otherwise, which we currently do not allow
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
	if(WIN32)
		set(CMAKE_INSTALL_PREFIX ${EXECUTABLE_OUTPUT_PATH}/\${BUILD_TYPE} CACHE PATH "default install path" FORCE)
	elseif(APPLE)
		set(CMAKE_INSTALL_PREFIX ${EXECUTABLE_OUTPUT_PATH}/\${BUILD_TYPE} CACHE PATH "default install path" FORCE)
	else()
		if(WITH_INSTALL_PORTABLE)
			set(CMAKE_INSTALL_PREFIX ${EXECUTABLE_OUTPUT_PATH} CACHE PATH "default install path" FORCE)
		endif()
	endif()
endif()



# Apple

if(APPLE)
	# require newer cmake on osx because of version handling,
	# older cmake cannot handle 2 digit subversion!
	cmake_minimum_required(VERSION 3.0.0)

	if(NOT CMAKE_OSX_ARCHITECTURES)
		set(CMAKE_OSX_ARCHITECTURES x86_64 CACHE STRING
			"Choose the architecture you want to build Blender for: i386, x86_64 or ppc"
			FORCE)
	endif()

	if(NOT DEFINED OSX_SYSTEM)
		execute_process(
		        COMMAND xcodebuild -version -sdk macosx SDKVersion
		        OUTPUT_VARIABLE OSX_SYSTEM
		        OUTPUT_STRIP_TRAILING_WHITESPACE)
	endif()

	# workaround for incorrect cmake xcode lookup for developer previews - XCODE_VERSION does not
	# take xcode-select path into account but would always look  into /Applications/Xcode.app
	# while dev versions are named Xcode<version>-DP<preview_number>
	execute_process(
	        COMMAND xcode-select --print-path
	        OUTPUT_VARIABLE XCODE_CHECK OUTPUT_STRIP_TRAILING_WHITESPACE)
	string(REPLACE "/Contents/Developer" "" XCODE_BUNDLE ${XCODE_CHECK}) # truncate to bundlepath in any case
	
	if(${CMAKE_GENERATOR} MATCHES "Xcode")
	
		# earlier xcode has no bundled developer dir, no sense in getting xcode path from
		if(${XCODE_VERSION} VERSION_GREATER 4.2) 
			# reduce to XCode name without dp extension
			string(SUBSTRING "${XCODE_CHECK}" 14 6 DP_NAME) 
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

	message(STATUS "Detected OS X ${OSX_SYSTEM} and Xcode ${XCODE_VERSION} at ${XCODE_BUNDLE}")

	if(${XCODE_VERSION} VERSION_LESS 4.3)
		# use guaranteed existing sdk
		set(CMAKE_OSX_SYSROOT /Developer/SDKs/MacOSX${OSX_SYSTEM}.sdk CACHE PATH "" FORCE)
	else()
		# note: xcode-select path could be ambigous,
		# cause /Applications/Xcode.app/Contents/Developer or /Applications/Xcode.app would be allowed
		# so i use a selfcomposed bundlepath here  
		set(OSX_SYSROOT_PREFIX ${XCODE_BUNDLE}/Contents/Developer/Platforms/MacOSX.platform)
		message(STATUS "OSX_SYSROOT_PREFIX: " ${OSX_SYSROOT_PREFIX})
		set(OSX_DEVELOPER_PREFIX /Developer/SDKs/MacOSX${OSX_SYSTEM}.sdk) # use guaranteed existing sdk
		set(CMAKE_OSX_SYSROOT ${OSX_SYSROOT_PREFIX}/${OSX_DEVELOPER_PREFIX} CACHE PATH "" FORCE)
		if(${CMAKE_GENERATOR} MATCHES "Xcode")
			# to silence sdk not found warning, just overrides CMAKE_OSX_SYSROOT
			set(CMAKE_XCODE_ATTRIBUTE_SDKROOT macosx${OSX_SYSTEM})
		endif()
	endif()

	if(OSX_SYSTEM MATCHES 10.9)
		# make sure syslibs and headers are looked up in sdk ( expecially for 10.9 openGL atm. )
		set(CMAKE_FIND_ROOT_PATH ${CMAKE_OSX_SYSROOT})
	endif()

	if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
		# 10.6 is our min. target, if you use higher sdk, weak linking happens
		set(CMAKE_OSX_DEPLOYMENT_TARGET "10.6" CACHE STRING "" FORCE)
	endif()
	
	if(NOT ${CMAKE_GENERATOR} MATCHES "Xcode")
		# force CMAKE_OSX_DEPLOYMENT_TARGET for makefiles, will not work else ( cmake bug ? )
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}")
		add_definitions("-DMACOSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}")
	endif()
endif()


#-----------------------------------------------------------------------------
# Check for conflicting/unsupported configurations

if(NOT WITH_BLENDER AND NOT WITH_PLAYER AND NOT WITH_CYCLES_STANDALONE)
	message(FATAL_ERROR
		"At least one of WITH_BLENDER or WITH_PLAYER or "
		"WITH_CYCLES_STANDALONE must be enabled, nothing to do!"
	)
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

# enable boost for cycles, audaspace or i18n
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
	set_and_warn(WITH_AUDASPACE      OFF)
	set_and_warn(WITH_INTERNATIONAL  OFF)
	set_and_warn(WITH_OPENVDB        OFF)
	set_and_warn(WITH_OPENCOLORIO    OFF)
	set_and_warn(WITH_MOD_BOOLEAN    OFF)

	set_and_warn(WITH_OPENAL         OFF)  # depends on AUDASPACE
	set_and_warn(WITH_GAMEENGINE     OFF)  # depends on AUDASPACE
	set_and_warn(WITH_PLAYER         OFF)  # depends on GAMEENGINE
elseif(WITH_CYCLES OR WITH_OPENIMAGEIO OR WITH_AUDASPACE OR WITH_INTERNATIONAL OR
       WITH_OPENVDB OR WITH_OPENCOLORIO OR WITH_MOD_BOOLEAN)
	# Keep enabled
else()
	# New dependency graph needs either Boost or C++11 for function bindings.
	if(NOT USE_CXX11)
		# Enabled but we don't need it
		set(WITH_BOOST OFF)
	endif()
endif()

# auto enable openimageio for cycles
if(WITH_CYCLES)
	set(WITH_OPENIMAGEIO ON)

	# auto enable llvm for cycles_osl
	if(WITH_CYCLES_OSL)
		set(WITH_LLVM ON CACHE BOOL "" FORCE)
	endif()
else()
	set(WITH_CYCLES_OSL OFF)
endif()

# auto enable openimageio linking dependencies
if(WITH_OPENIMAGEIO)
	set(WITH_IMAGE_OPENEXR ON)
	set(WITH_IMAGE_TIFF ON)
endif()

# don't store paths to libs for portable distribution
if(WITH_INSTALL_PORTABLE)
	set(CMAKE_SKIP_BUILD_RPATH TRUE)
endif()

if(WITH_GHOST_SDL OR WITH_HEADLESS)
	set(WITH_X11           OFF)
	set(WITH_X11_XINPUT    OFF)
	set(WITH_X11_XF86VMODE OFF)
	set(WITH_X11_ALPHA     OFF)
	set(WITH_GHOST_XDND    OFF)
	set(WITH_INPUT_IME     OFF)
endif()

if(WITH_CPU_SSE)
	TEST_SSE_SUPPORT(COMPILER_SSE_FLAG COMPILER_SSE2_FLAG)
else()
	message(STATUS "SSE and SSE2 optimizations are DISABLED!")
	set(COMPILER_SSE_FLAG)
	set(COMPILER_SSE2_FLAG)
endif()

if(WITH_BUILDINFO)
	find_package(Git)
	if(NOT GIT_FOUND)
		message(WARNING "Git was not found, disabling WITH_BUILDINFO")
		set(WITH_BUILDINFO OFF)
	endif()
endif()

TEST_SHARED_PTR_SUPPORT()
TEST_UNORDERED_MAP_SUPPORT()

if(WITH_AUDASPACE)
	if(WITH_SYSTEM_AUDASPACE)
		set(AUDASPACE_DEFINITIONS
			-DWITH_AUDASPACE
			-DWITH_SYSTEM_AUDASPACE
			"-DAUD_DEVICE_H=<AUD_Device.h>"
			"-DAUD_SPECIAL_H=<AUD_Special.h>"
			"-DAUD_SOUND_H=<AUD_Sound.h>"
			"-DAUD_HANDLE_H=<AUD_Handle.h>"
			"-DAUD_SEQUENCE_H=<AUD_Sequence.h>"
			"-DAUD_TYPES_H=<AUD_Types.h>"
			"-DAUD_PYTHON_H=<python/PyAPI.h>"
		)
	else()
		set(AUDASPACE_C_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/intern/audaspace/intern")
		set(AUDASPACE_PY_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/intern/audaspace/intern")
		set(AUDASPACE_DEFINITIONS
			-DWITH_AUDASPACE
			"-DAUD_DEVICE_H=<AUD_C-API.h>"
			"-DAUD_SPECIAL_H=<AUD_C-API.h>"
			"-DAUD_SOUND_H=<AUD_C-API.h>"
			"-DAUD_HANDLE_H=<AUD_C-API.h>"
			"-DAUD_SEQUENCE_H=<AUD_C-API.h>"
			"-DAUD_TYPES_H=<AUD_Space.h>"
		)
	endif()
endif()

#-----------------------------------------------------------------------------
# Check for valid directories
# ... a partial checkout may cause this.
#
# note: we need to check for a known subdir in both cases.
#       since uninitialized git submodules will give blank dirs

if(WITH_INTERNATIONAL)
	if(NOT EXISTS "${CMAKE_SOURCE_DIR}/release/datafiles/locale/languages")
		message(WARNING
			"Translation path '${CMAKE_SOURCE_DIR}/release/datafiles/locale' is missing, "
			"This is a 'git submodule', which are known not to work with bridges to other version "
			"control systems, disabling 'WITH_INTERNATIONAL'."
		)
		set(WITH_INTERNATIONAL OFF)
	endif()
endif()

if(WITH_PYTHON)
	if(NOT EXISTS "${CMAKE_SOURCE_DIR}/release/scripts/addons/modules")
		message(WARNING
			"Addons path '${CMAKE_SOURCE_DIR}/release/scripts/addons' is missing, "
			"This is a 'git submodule', which are known not to work with bridges to other version "
			"control systems: * CONTINUING WITHOUT ADDONS *"
		)
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

	if(WITH_X11_ALPHA)
		find_library(X11_Xrender_LIB Xrender  ${X11_LIB_SEARCH_PATH})
		mark_as_advanced(X11_Xrender_LIB)
		if (X11_Xrender_LIB)
			list(APPEND PLATFORM_LINKLIBS ${X11_Xrender_LIB})
		else()
			set(WITH_X11_ALPHA OFF)
		endif()
	endif()

endif()


# ----------------------------------------------------------------------------
# Main Platform Checks
#
# - UNIX
# - WIN32
# - APPLE

if(UNIX AND NOT APPLE)
	macro(find_package_wrapper)
		if(WITH_STATIC_LIBS)
			find_package_static(${ARGV})
		else()
			find_package(${ARGV})
		endif()
	endmacro()

	find_package_wrapper(JPEG REQUIRED)
	find_package_wrapper(PNG REQUIRED)
	find_package_wrapper(ZLIB REQUIRED)
	find_package_wrapper(Freetype REQUIRED)

	if(WITH_LZO AND WITH_SYSTEM_LZO)
		find_package_wrapper(LZO)
		if(NOT LZO_FOUND)
			message(FATAL_ERROR "Failed finding system LZO version!")
		endif()
	endif()

	if(WITH_SYSTEM_EIGEN3)
		find_package_wrapper(Eigen3)
		if(NOT EIGEN3_FOUND)
			message(FATAL_ERROR "Failed finding system Eigen3 version!")
		endif()
	endif()
	# else values are set below for all platforms

	if(WITH_PYTHON)
		# No way to set py35, remove for now.
		# find_package(PythonLibs)

		# Use our own instead, since without py is such a rare case,
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
	if(WITH_SYSTEM_AUDASPACE)
		find_package_wrapper(Audaspace)
		if(NOT AUDASPACE_FOUND OR NOT AUDASPACE_C_FOUND)
			message(FATAL_ERROR "Audaspace external library not found!")
		endif()
	endif()

	if(WITH_OPENAL)
		find_package_wrapper(OpenAL)
		if(NOT OPENAL_FOUND)
			set(WITH_OPENAL OFF)
		endif()
	endif()

	if(WITH_SDL)
		if(WITH_SDL_DYNLOAD)
			set(SDL_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/extern/sdlew/include/SDL2")
			set(SDL_LIBRARY)
		else()
			find_package_wrapper(SDL2)
			if(SDL2_FOUND)
				# Use same names for both versions of SDL until we move to 2.x.
				set(SDL_INCLUDE_DIR "${SDL2_INCLUDE_DIR}")
				set(SDL_LIBRARY "${SDL2_LIBRARY}")
				set(SDL_FOUND "${SDL2_FOUND}")
			else()
				find_package_wrapper(SDL)
			endif()
			mark_as_advanced(
				SDL_INCLUDE_DIR
				SDL_LIBRARY
			)
			# unset(SDLMAIN_LIBRARY CACHE)
			if(NOT SDL_FOUND)
				set(WITH_SDL OFF)
			endif()
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
		if(SPACENAV_FOUND)
			# use generic names within blenders buildsystem.
			set(NDOF_INCLUDE_DIRS ${SPACENAV_INCLUDE_DIRS})
			set(NDOF_LIBRARIES ${SPACENAV_LIBRARIES})
		else()
			set(WITH_INPUT_NDOF OFF)
		endif()
	endif()

	if(WITH_CYCLES_OSL)
		set(CYCLES_OSL ${LIBDIR}/osl CACHE PATH "Path to OpenShadingLanguage installation")
		if(NOT OSL_ROOT)
			set(OSL_ROOT ${CYCLES_OSL})
		endif()
		find_package_wrapper(OpenShadingLanguage)
		if(OSL_FOUND)
			if(${OSL_LIBRARY_VERSION_MAJOR} EQUAL "1" AND ${OSL_LIBRARY_VERSION_MINOR} LESS "6")
				# Note: --whole-archive is needed to force loading of all symbols in liboslexec,
				# otherwise LLVM is missing the osl_allocate_closure_component function
				set(OSL_LIBRARIES
					${OSL_OSLCOMP_LIBRARY}
					-Wl,--whole-archive ${OSL_OSLEXEC_LIBRARY}
					-Wl,--no-whole-archive ${OSL_OSLQUERY_LIBRARY}
				)
			endif()
		else()
			message(STATUS "OSL not found, disabling it from Cycles")
			set(WITH_CYCLES_OSL OFF)
		endif()
	endif()

	if(WITH_OPENVDB)
		find_package_wrapper(OpenVDB)
		find_package_wrapper(TBB)
		if(NOT OPENVDB_FOUND OR NOT TBB_FOUND)
			set(WITH_OPENVDB OFF)
			set(WITH_OPENVDB_BLOSC OFF)
			message(STATUS "OpenVDB not found, disabling it")
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
			if(WITH_CYCLES_OSL)
				if(NOT (${OSL_LIBRARY_VERSION_MAJOR} EQUAL "1" AND ${OSL_LIBRARY_VERSION_MINOR} LESS "6"))
					list(APPEND __boost_packages wave)
				else()
				endif()
			endif()
			if(WITH_INTERNATIONAL)
				list(APPEND __boost_packages locale)
			endif()
			if(WITH_CYCLES_NETWORK)
				list(APPEND __boost_packages serialization)
			endif()
			if(WITH_OPENVDB)
				list(APPEND __boost_packages iostreams)
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
		if(NOT OPENIMAGEIO_PUGIXML_FOUND AND WITH_CYCLES_STANDALONE)
			find_package_wrapper(PugiXML)
		else()
			set(PUGIXML_INCLUDE_DIR "${OPENIMAGEIO_INCLUDE_DIR/OpenImageIO}")
			set(PUGIXML_LIBRARIES "")
		endif()

		set(OPENIMAGEIO_LIBRARIES
			${OPENIMAGEIO_LIBRARIES}
			${PNG_LIBRARIES}
			${JPEG_LIBRARIES}
			${ZLIB_LIBRARIES}
			${BOOST_LIBRARIES}
		)
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

	if(WITH_LLVM)
		find_package_wrapper(LLVM)

		if(NOT LLVM_FOUND)
			set(WITH_LLVM OFF)
			message(STATUS "LLVM not found")
		endif()
	endif()

	if(WITH_LLVM OR WITH_SDL_DYNLOAD)
		# Fix for conflict with Mesa llvmpipe
		set(PLATFORM_LINKFLAGS
			"${PLATFORM_LINKFLAGS} -Wl,--version-script='${CMAKE_SOURCE_DIR}/source/creator/blender.map'"
		)
	endif()

	if(WITH_OPENSUBDIV)
		find_package_wrapper(OpenSubdiv)

		set(OPENSUBDIV_LIBRARIES ${OPENSUBDIV_LIBRARIES})
		set(OPENSUBDIV_LIBPATH)  # TODO, remove and reference the absolute path everywhere

		if(NOT OPENSUBDIV_FOUND)
			set(WITH_OPENSUBDIV OFF)
			message(STATUS "OpenSubdiv not found")
		endif()
	endif()

	# OpenSuse needs lutil, ArchLinux not, for now keep, can avoid by using --as-needed
	list(APPEND PLATFORM_LINKLIBS -lutil -lc -lm)

	find_package(Threads REQUIRED)
	list(APPEND PLATFORM_LINKLIBS ${CMAKE_THREAD_LIBS_INIT})
	# used by other platforms
	set(PTHREADS_LIBRARIES ${CMAKE_THREAD_LIBS_INIT})

	if(CMAKE_DL_LIBS)
		list(APPEND PLATFORM_LINKLIBS ${CMAKE_DL_LIBS})
	endif()

	if(CMAKE_SYSTEM_NAME MATCHES "Linux")
		if(NOT WITH_PYTHON_MODULE)
			# binreloc is linux only
			set(BINRELOC_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/extern/binreloc/include)
			set(WITH_BINRELOC ON)
		endif()
	endif()

	# lfs on glibc, all compilers should use
	add_definitions(-D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE)

	# GNU Compiler
	if(CMAKE_COMPILER_IS_GNUCC)
		set(PLATFORM_CFLAGS "-pipe -fPIC -funsigned-char -fno-strict-aliasing")

		# use ld.gold linker if available, could make optional
		execute_process(
		        COMMAND ${CMAKE_C_COMPILER} -fuse-ld=gold -Wl,--version
		        ERROR_QUIET OUTPUT_VARIABLE LD_VERSION)
		if("${LD_VERSION}" MATCHES "GNU gold")
			set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fuse-ld=gold")
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fuse-ld=gold")
		else()
			message(STATUS "GNU gold linker isn't available, using the default system linker.")
		endif()
		unset(LD_VERSION)

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
		# Minimum MSVC Version
		set(_min_ver "18.0.31101")
		if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS ${_min_ver})
			message(FATAL_ERROR
			        "Visual Studio 2013 (Update 4, ${_min_ver}) required, "
			        "found (${CMAKE_CXX_COMPILER_VERSION})")
		endif()
		unset(_min_ver)

		# needed for some MSVC installations
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SAFESEH:NO")
		set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /SAFESEH:NO")
		set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} /SAFESEH:NO")

		list(APPEND PLATFORM_LINKLIBS
			ws2_32 vfw32 winmm kernel32 user32 gdi32 comdlg32
			advapi32 shfolder shell32 ole32 oleaut32 uuid psapi Dbghelp
		)

		if(WITH_INPUT_IME)
			list(APPEND PLATFORM_LINKLIBS imm32)
		endif()

		add_definitions(
			-D_CRT_NONSTDC_NO_DEPRECATE
			-D_CRT_SECURE_NO_DEPRECATE
			-D_SCL_SECURE_NO_DEPRECATE
			-D_CONSOLE
			-D_LIB
		)

		# MSVC11 needs _ALLOW_KEYWORD_MACROS to build
		add_definitions(-D_ALLOW_KEYWORD_MACROS)

		if(CMAKE_CL_64)
			# We want to support Vista level ABI for x64
			add_definitions(-D_WIN32_WINNT=0x600)
		endif()

		# Make cmake find the msvc redistributables
		set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP TRUE)
		include(InstallRequiredSystemLibraries)

		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /nologo /J /Gd /MP /EHsc")
		set(CMAKE_C_FLAGS     "${CMAKE_C_FLAGS} /nologo /J /Gd /MP")

		set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
		set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /MTd")
		set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
		set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /MT")
		set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL} /MT")
		set(CMAKE_C_FLAGS_MINSIZEREL "${CMAKE_C_FLAGS_MINSIZEREL} /MT")
		set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /MT")
		set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} /MT")

		set(PLATFORM_LINKFLAGS "/SUBSYSTEM:CONSOLE /STACK:2097152 /INCREMENTAL:NO ")
		set(PLATFORM_LINKFLAGS "${PLATFORM_LINKFLAGS} /NODEFAULTLIB:msvcrt.lib /NODEFAULTLIB:msvcmrt.lib /NODEFAULTLIB:msvcurt.lib /NODEFAULTLIB:msvcrtd.lib ")

		# Ignore meaningless for us linker warnings.
		set(PLATFORM_LINKFLAGS "${PLATFORM_LINKFLAGS} /ignore:4049 /ignore:4217 /ignore:4221")
		set(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} /ignore:4221")

		# MSVC only, Mingw doesnt need
		if(CMAKE_CL_64)
			set(PLATFORM_LINKFLAGS "/MACHINE:X64 /OPT:NOREF ${PLATFORM_LINKFLAGS}")
		else()
			set(PLATFORM_LINKFLAGS "/MACHINE:IX86 /LARGEADDRESSAWARE ${PLATFORM_LINKFLAGS}")
		endif()

		set(PLATFORM_LINKFLAGS_DEBUG "/IGNORE:4099 /NODEFAULTLIB:libcmt.lib /NODEFAULTLIB:libc.lib")

		# Use dynamic loading for OpenMP
		if(WITH_OPENMP)
			if(MSVC_VERSION EQUAL 1800)
				set(OPENMP_DLL_NAME "vcomp120")
			else()
				set(OPENMP_DLL_NAME "vcomp140")
			endif()
			set(PLATFORM_LINKFLAGS "${PLATFORM_LINKFLAGS} /DELAYLOAD:${OPENMP_DLL_NAME}.dll delayimp.lib")
			set(PLATFORM_LINKFLAGS_DEBUG "${PLATFORM_LINKFLAGS_DEBUG} /DELAYLOAD:${OPENMP_DLL_NAME}d.dll delayimp.lib")
		endif()

		if(NOT DEFINED LIBDIR)

			# Setup 64bit and 64bit windows systems
			if(CMAKE_CL_64)
				message(STATUS "64 bit compiler detected.")
				set(LIBDIR_BASE "win64")
			else()
				message(STATUS "32 bit compiler detected.")
				set(LIBDIR_BASE "windows")
			endif()

			if(MSVC_VERSION EQUAL 1900)
				message(STATUS "Visual Studio 2015 detected.")
				set(LIBDIR ${CMAKE_SOURCE_DIR}/../lib/${LIBDIR_BASE}_vc14)
			else()
				message(STATUS "Visual Studio 2013 detected.")
				set(LIBDIR ${CMAKE_SOURCE_DIR}/../lib/${LIBDIR_BASE}_vc12)
			endif()
		else()
			message(STATUS "Using pre-compiled LIBDIR: ${LIBDIR}")
		endif()
		if(NOT EXISTS "${LIBDIR}/")
			message(FATAL_ERROR "Windows requires pre-compiled libs at: '${LIBDIR}'")
		endif()

		# Add each of our libraries to our cmake_prefix_path so find_package() could work
		file(GLOB children RELATIVE ${LIBDIR} ${LIBDIR}/*)
		foreach(child ${children})
			if(IS_DIRECTORY ${LIBDIR}/${child})
				list(APPEND CMAKE_PREFIX_PATH  ${LIBDIR}/${child})
			endif()
		endforeach()

		set(ZLIB_INCLUDE_DIRS ${LIBDIR}/zlib/include)
		set(ZLIB_LIBRARIES ${LIBDIR}/zlib/lib/libz_st.lib)
		set(ZLIB_INCLUDE_DIR ${LIBDIR}/zlib/include)
		set(ZLIB_LIBRARY ${LIBDIR}/zlib/lib/libz_st.lib)
		set(ZLIB_DIR ${LIBDIR}/zlib)
		#find_package(zlib) # we want to find before finding things that depend on it like png


		find_package(png)
		if(NOT PNG_FOUND)
			message(WARNING "Using HARDCODED libpng locations")
			set(PNG_PNG_INCLUDE_DIR ${LIBDIR}/png/include)
			set(PNG_LIBRARIES libpng)
			set(PNG "${LIBDIR}/png")
			set(PNG_INCLUDE_DIRS "${PNG}/include")
			set(PNG_LIBPATH ${PNG}/lib) # not cmake defined
		endif()

		set(JPEG_NAMES ${JPEG_NAMES} libjpeg)
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
				set(OPENEXR_INCLUDE_DIRS ${OPENEXR_INCLUDE_DIR} ${OPENEXR}/include/OpenEXR)
				set(OPENEXR_LIBPATH ${OPENEXR}/lib)
				set(OPENEXR_LIBRARIES
					optimized ${OPENEXR_LIBPATH}/Iex-2_2.lib
					optimized ${OPENEXR_LIBPATH}/Half.lib
					optimized ${OPENEXR_LIBPATH}/IlmImf-2_2.lib
					optimized ${OPENEXR_LIBPATH}/Imath-2_2.lib
					optimized ${OPENEXR_LIBPATH}/IlmThread-2_2.lib
					debug ${OPENEXR_LIBPATH}/Iex-2_2_d.lib
					debug ${OPENEXR_LIBPATH}/Half_d.lib
					debug ${OPENEXR_LIBPATH}/IlmImf-2_2_d.lib
					debug ${OPENEXR_LIBPATH}/Imath-2_2_d.lib
					debug ${OPENEXR_LIBPATH}/IlmThread-2_2_d.lib
				)
			endif()
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
			set(PYTHON_VERSION 3.5) # CACHE STRING)

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
			endif()
			if(WITH_INTERNATIONAL)
				list(APPEND boost_extra_libs locale)
			endif()
			if(WITH_OPENVDB)
				list(APPEND boost_extra_libs iostreams)
			endif()
			set(Boost_USE_STATIC_RUNTIME ON) # prefix lib
			set(Boost_USE_MULTITHREADED ON) # suffix -mt
			set(Boost_USE_STATIC_LIBS ON) # suffix -s
			find_package(Boost COMPONENTS date_time filesystem thread regex system ${boost_extra_libs})
			if(NOT Boost_FOUND)
				message(WARNING "USING HARDCODED boost locations")
				set(BOOST ${LIBDIR}/boost)
				set(BOOST_INCLUDE_DIR ${BOOST}/include)
				if(MSVC12)
					set(BOOST_LIBPATH ${BOOST}/lib)
					set(BOOST_POSTFIX "vc120-mt-s-1_60.lib")
					set(BOOST_DEBUG_POSTFIX "vc120-mt-sgd-1_60.lib")
				else()
					set(BOOST_LIBPATH ${BOOST}/lib)
					set(BOOST_POSTFIX "vc140-mt-s-1_60.lib")
					set(BOOST_DEBUG_POSTFIX "vc140-mt-sgd-1_60.lib")
				endif()
				set(BOOST_LIBRARIES
					optimized libboost_date_time-${BOOST_POSTFIX}
					optimized libboost_filesystem-${BOOST_POSTFIX}
					optimized libboost_regex-${BOOST_POSTFIX}
					optimized libboost_system-${BOOST_POSTFIX}
					optimized libboost_thread-${BOOST_POSTFIX}
					debug libboost_date_time-${BOOST_DEBUG_POSTFIX}
					debug libboost_filesystem-${BOOST_DEBUG_POSTFIX}
					debug libboost_regex-${BOOST_DEBUG_POSTFIX}
					debug libboost_system-${BOOST_DEBUG_POSTFIX}
					debug libboost_thread-${BOOST_DEBUG_POSTFIX}
				)
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
			set(OPENCOLORIO_DEFINITIONS "-DOCIO_STATIC_BUILD")
			set(OPENIMAGEIO_IDIFF "${OPENIMAGEIO}/bin/idiff.exe")
			add_definitions(-DOIIO_STATIC_BUILD)
		endif()

		if(WITH_LLVM)
			set(LLVM_ROOT_DIR ${LIBDIR}/llvm CACHE PATH	"Path to the LLVM installation")
			file(GLOB LLVM_LIBRARY_OPTIMIZED ${LLVM_ROOT_DIR}/lib/*.lib)

			if(EXISTS ${LLVM_ROOT_DIR}/debug/lib)
				foreach(LLVM_OPTIMIZED_LIB ${LLVM_LIBRARY_OPTIMIZED})
					get_filename_component(LIBNAME ${LLVM_OPTIMIZED_LIB} ABSOLUTE)
					list(APPEND LLVM_LIBS optimized ${LIBNAME})
				endforeach(LLVM_OPTIMIZED_LIB)
			
				file(GLOB LLVM_LIBRARY_DEBUG ${LLVM_ROOT_DIR}/debug/lib/*.lib)

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

		if(WITH_OPENVDB)
			set(BLOSC_LIBRARIES optimized ${LIBDIR}/blosc/lib/libblosc.lib debug ${LIBDIR}/blosc/lib/libblosc_d.lib)
			set(TBB_LIBRARIES optimized ${LIBDIR}/tbb/lib/tbb.lib debug ${LIBDIR}/tbb/lib/tbb_debug.lib)
			set(TBB_INCLUDE_DIR ${LIBDIR}/tbb/include)
			set(OPENVDB ${LIBDIR}/openvdb)
			set(OPENVDB_INCLUDE_DIRS ${OPENVDB}/include ${TBB_INCLUDE_DIR})
			set(OPENVDB_LIBRARIES optimized openvdb debug openvdb_d ${TBB_LIBRARIES} ${BLOSC_LIBRARIES})
			set(OPENVDB_LIBPATH ${LIBDIR}/openvdb/lib)
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

		if(WITH_OPENSUBDIV)
			set(OPENSUBDIV_INCLUDE_DIR ${LIBDIR}/opensubdiv/include)
			set(OPENSUBDIV_LIBPATH ${LIBDIR}/opensubdiv/lib)
			set(OPENSUBDIV_LIBRARIES ${OPENSUBDIV_LIBPATH}/osdCPU.lib ${OPENSUBDIV_LIBPATH}/osdGPU.lib)
			find_package(OpenSubdiv)
		endif()

		if(WITH_SDL)
			set(SDL ${LIBDIR}/sdl)
			set(SDL_INCLUDE_DIR ${SDL}/include)
			set(SDL_LIBPATH ${SDL}/lib)
			# MinGW TODO: Update MinGW to SDL2
			if(NOT CMAKE_COMPILER_IS_GNUCC)
				set(SDL_LIBRARY SDL2)
			else()
				set(SDL_LIBRARY SDL)
			endif()
		endif()

		# Audio IO
		if(WITH_SYSTEM_AUDASPACE)
			set(AUDASPACE_INCLUDE_DIRS ${LIBDIR}/audaspace/include/audaspace)
			set(AUDASPACE_LIBRARIES ${LIBDIR}/audaspace/lib/audaspace.lib)
			set(AUDASPACE_C_INCLUDE_DIRS ${LIBDIR}/audaspace/include/audaspace)
			set(AUDASPACE_C_LIBRARIES ${LIBDIR}/audaspace/lib/audaspace-c.lib)
			set(AUDASPACE_PY_INCLUDE_DIRS ${LIBDIR}/audaspace/include/audaspace)
			set(AUDASPACE_PY_LIBRARIES ${LIBDIR}/audaspace/lib/audaspace-py.lib)
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
			WITH_MINGW64
		)
		
		if(NOT DEFINED LIBDIR)
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
		else()
			message(STATUS "Using pre-compiled LIBDIR: ${LIBDIR}")
		endif()
		if(NOT EXISTS "${LIBDIR}/")
			message(FATAL_ERROR "Windows requires pre-compiled libs at: '${LIBDIR}'")
		endif()

		list(APPEND PLATFORM_LINKLIBS
			-lshell32 -lshfolder -lgdi32 -lmsvcrt -lwinmm -lmingw32 -lm -lws2_32
			-lz -lstdc++ -lole32 -luuid -lwsock32 -lpsapi -ldbghelp
		)

		if(WITH_INPUT_IME)
			list(APPEND PLATFORM_LINKLIBS -limm32)
		endif()

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
			set(OPENCOLLADA_LIBRARIES
				OpenCOLLADAStreamWriter
				OpenCOLLADASaxFrameworkLoader
				OpenCOLLADAFramework
				OpenCOLLADABaseUtils
				GeneratedSaxParser
				UTF MathMLSolver buffer ftoa xml
			)
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
			set(PYTHON_VERSION 3.5) #  CACHE STRING)
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
					debug boost_locale-${BOOST_DEBUG_POSTFIX}
				)
			endif()
			if(WITH_CYCLES_OSL)
				set(BOOST_LIBRARIES ${BOOST_LIBRARIES}
					optimized boost_wave-${BOOST_POSTFIX}
					debug boost_wave-${BOOST_DEBUG_POSTFIX}
				)
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
			set(OPENIMAGEIO_IDIFF "${OPENIMAGEIO}/bin/idiff.exe")
		endif()
		
		if(WITH_LLVM)
			set(LLVM_ROOT_DIR ${LIBDIR}/llvm CACHE PATH	"Path to the LLVM installation")
			set(LLVM_LIBPATH ${LLVM_ROOT_DIR}/lib)
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

		if(WITH_SDL)
			set(SDL ${LIBDIR}/sdl)
			set(SDL_INCLUDE_DIR ${SDL}/include)
			set(SDL_LIBRARY SDL)
			set(SDL_LIBPATH ${SDL}/lib)
		endif()

		if(WITH_OPENVDB)
			set(OPENVDB ${LIBDIR}/openvdb)
			set(OPENVDB_INCLUDE_DIRS ${OPENVDB}/include)
			set(OPENVDB_LIBRARIES openvdb ${TBB_LIBRARIES})
			set(OPENVDB_LIBPATH ${LIBDIR}/openvdb/lib)
			set(OPENVDB_DEFINITIONS)
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
		if(MSVC)
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
		list(APPEND OSL_LIBRARIES
			optimized ${OSL_LIB_COMP}
			optimized ${OSL_LIB_EXEC}
			optimized ${OSL_LIB_QUERY}
			debug ${OSL_LIB_EXEC_DEBUG}
			debug ${OSL_LIB_COMP_DEBUG}
			debug ${OSL_LIB_QUERY_DEBUG}
		)
		find_path(OSL_INCLUDE_DIR OSL/oslclosure.h PATHS ${CYCLES_OSL}/include)
		find_program(OSL_COMPILER NAMES oslc PATHS ${CYCLES_OSL}/bin)
	
		if(OSL_INCLUDE_DIR AND OSL_LIBRARIES AND OSL_COMPILER)
			set(OSL_FOUND TRUE)
		else()
			message(STATUS "OSL not found")
			set(WITH_CYCLES_OSL OFF)
		endif()
	endif()

elseif(APPLE)

	if(NOT DEFINED LIBDIR)
		set(LIBDIR ${CMAKE_SOURCE_DIR}/../lib/darwin-9.x.universal)
	else()
		message(STATUS "Using pre-compiled LIBDIR: ${LIBDIR}")
	endif()
	if(NOT EXISTS "${LIBDIR}/")
		message(FATAL_ERROR "Mac OSX requires pre-compiled libs at: '${LIBDIR}'")
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

	if(WITH_OPENSUBDIV)
		set(OPENSUBDIV ${LIBDIR}/opensubdiv)
		set(OPENSUBDIV_LIBPATH ${OPENSUBDIV}/lib)
		find_library(OSL_LIB_UTIL NAMES osdutil PATHS ${OPENSUBDIV_LIBPATH})
		find_library(OSL_LIB_CPU NAMES osdCPU PATHS ${OPENSUBDIV_LIBPATH})
		find_library(OSL_LIB_GPU NAMES osdGPU PATHS ${OPENSUBDIV_LIBPATH})
		set(OPENSUBDIV_INCLUDE_DIR ${OPENSUBDIV}/include)
		set(OPENSUBDIV_INCLUDE_DIRS ${OPENSUBDIV_INCLUDE_DIR})
		list(APPEND OPENSUBDIV_LIBRARIES ${OSL_LIB_UTIL} ${OSL_LIB_CPU} ${OSL_LIB_GPU})
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
		# we use precompiled libraries for py 3.5 and up by default
		set(PYTHON_VERSION 3.5)
		if(NOT WITH_PYTHON_MODULE AND NOT WITH_PYTHON_FRAMEWORK)
			# normally cached but not since we include them with blender
			set(PYTHON_INCLUDE_DIR "${LIBDIR}/python/include/python${PYTHON_VERSION}m")
			set(PYTHON_EXECUTABLE "${LIBDIR}/python/bin/python${PYTHON_VERSION}m")
			set(PYTHON_LIBRARY python${PYTHON_VERSION}m)
			set(PYTHON_LIBPATH "${LIBDIR}/python/lib/python${PYTHON_VERSION}")
			# set(PYTHON_LINKFLAGS "-u _PyMac_Error")  # won't  build with this enabled
		else()
			# module must be compiled against Python framework
			set(_py_framework "/Library/Frameworks/Python.framework/Versions/${PYTHON_VERSION}")

			set(PYTHON_INCLUDE_DIR "${_py_framework}/include/python${PYTHON_VERSION}m")
			set(PYTHON_EXECUTABLE "${_py_framework}/bin/python${PYTHON_VERSION}m")
			set(PYTHON_LIBPATH "${_py_framework}/lib/python${PYTHON_VERSION}/config-${PYTHON_VERSION}m")
			#set(PYTHON_LIBRARY python${PYTHON_VERSION})
			#set(PYTHON_LINKFLAGS "-u _PyMac_Error -framework Python")  # won't  build with this enabled

			unset(_py_framework)
		endif()
		
		# uncached vars
		set(PYTHON_INCLUDE_DIRS "${PYTHON_INCLUDE_DIR}")
		set(PYTHON_LIBRARIES  "${PYTHON_LIBRARY}")

		if(NOT EXISTS "${PYTHON_EXECUTABLE}")
			message(FATAL_ERROR "Python executable missing: ${PYTHON_EXECUTABLE}")
		endif()
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
		set(OPENEXR_INCLUDE_DIRS ${OPENEXR_INCLUDE_DIR} ${OPENEXR}/include/OpenEXR)
		set(OPENEXR_LIBRARIES Iex Half IlmImf Imath IlmThread)
		set(OPENEXR_LIBPATH ${OPENEXR}/lib)
	endif()

	if(WITH_CODEC_FFMPEG)
		set(FFMPEG ${LIBDIR}/ffmpeg)
		set(FFMPEG_INCLUDE_DIRS ${FFMPEG}/include)
		set(FFMPEG_LIBRARIES
			avcodec avdevice avformat avutil
			mp3lame swscale x264 xvidcore theora theoradec theoraenc vorbis vorbisenc vorbisfile ogg
		)
		set(FFMPEG_LIBPATH ${FFMPEG}/lib)
	endif()

	find_library(SYSTEMSTUBS_LIBRARY
		NAMES
		SystemStubs
		PATHS
	)
	mark_as_advanced(SYSTEMSTUBS_LIBRARY)
	if(SYSTEMSTUBS_LIBRARY)
		list(APPEND PLATFORM_LINKLIBS SystemStubs)
	endif()

	set(PLATFORM_CFLAGS "-pipe -funsigned-char")
	set(PLATFORM_LINKFLAGS
		"-fexceptions -framework CoreServices -framework Foundation -framework IOKit -framework AppKit -framework Cocoa -framework Carbon -framework AudioUnit -framework AudioToolbox -framework CoreAudio"
	)
	if(WITH_CODEC_QUICKTIME)
		set(PLATFORM_LINKFLAGS "${PLATFORM_LINKFLAGS} -framework QTKit")
		if(CMAKE_OSX_ARCHITECTURES MATCHES i386)
			set(PLATFORM_LINKFLAGS "${PLATFORM_LINKFLAGS} -framework QuickTime")
			# libSDL still needs 32bit carbon quicktime
		endif()
	endif()

	if(WITH_CXX11)
		list(APPEND PLATFORM_LINKLIBS c++)
	else()
		list(APPEND PLATFORM_LINKLIBS stdc++)
	endif()

	if(WITH_JACK)
		set(PLATFORM_LINKFLAGS "${PLATFORM_LINKFLAGS} -F/Library/Frameworks -weak_framework jackmp")
	endif()
	
	if(WITH_PYTHON_MODULE OR WITH_PYTHON_FRAMEWORK)
		# force cmake to link right framework
		set(PLATFORM_LINKFLAGS "${PLATFORM_LINKFLAGS} /Library/Frameworks/Python.framework/Versions/${PYTHON_VERSION}/Python")
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
		set(OPENCOLLADA_LIBRARIES
			OpenCOLLADASaxFrameworkLoader
			-lOpenCOLLADAFramework
			-lOpenCOLLADABaseUtils
			-lOpenCOLLADAStreamWriter
			-lMathMLSolver
			-lGeneratedSaxParser
			-lxml2 -lbuffer -lftoa
		)
		# Use UTF functions from collada if LLVM is not enabled
		if(NOT WITH_LLVM)
			list(APPEND OPENCOLLADA_LIBRARIES -lUTF)
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
		set(SDL_LIBRARY SDL2)
		set(SDL_LIBPATH ${SDL}/lib)
		set(PLATFORM_LINKFLAGS "${PLATFORM_LINKFLAGS} -lazy_framework ForceFeedback")
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

	if(WITH_BOOST)
		set(BOOST ${LIBDIR}/boost)
		set(BOOST_INCLUDE_DIR ${BOOST}/include)
		set(BOOST_LIBRARIES
			boost_date_time-mt
			boost_filesystem-mt
			boost_regex-mt
			boost_system-mt
			boost_thread-mt
			boost_wave-mt
		)
		if(WITH_INTERNATIONAL)
			list(APPEND BOOST_LIBRARIES boost_locale-mt)
		endif()
		if(WITH_CYCLES_NETWORK)
			list(APPEND BOOST_LIBRARIES boost_serialization-mt)
		endif()
		if(WITH_OPENVDB)
			list(APPEND BOOST_LIBRARIES boost_iostreams-mt)
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
		set(OPENIMAGEIO_LIBRARIES
			${OPENIMAGEIO}/lib/libOpenImageIO.a
			${PNG_LIBRARIES}
			${JPEG_LIBRARIES}
			${TIFF_LIBRARY}
			${OPENEXR_LIBRARIES}
			${ZLIB_LIBRARIES}
		)
		set(OPENIMAGEIO_LIBPATH
			${OPENIMAGEIO}/lib
			${JPEG_LIBPATH}
			${PNG_LIBPATH}
			${TIFF_LIBPATH}
			${OPENEXR_LIBPATH}
			${ZLIB_LIBPATH}
		)
		set(OPENIMAGEIO_DEFINITIONS "-DOIIO_STATIC_BUILD")
		set(OPENIMAGEIO_IDIFF "${LIBDIR}/openimageio/bin/idiff")
	endif()

	if(WITH_OPENCOLORIO)
		set(OPENCOLORIO ${LIBDIR}/opencolorio)
		set(OPENCOLORIO_INCLUDE_DIRS ${OPENCOLORIO}/include)
		set(OPENCOLORIO_LIBRARIES OpenColorIO tinyxml yaml-cpp)
		set(OPENCOLORIO_LIBPATH ${OPENCOLORIO}/lib)
	endif()

	if(WITH_OPENVDB)
		set(OPENVDB ${LIBDIR}/openvdb)
		set(OPENVDB_INCLUDE_DIRS ${OPENVDB}/include)
		set(TBB_INCLUDE_DIRS ${LIBDIR}/tbb/include)
		set(TBB_LIBRARIES ${LIBDIR}/tbb/lib/libtbb.a)
		set(OPENVDB_LIBRARIES openvdb blosc ${TBB_LIBRARIES})
		set(OPENVDB_LIBPATH ${LIBDIR}/openvdb/lib)
		set(OPENVDB_DEFINITIONS)
	endif()

	if(WITH_LLVM)
		set(LLVM_ROOT_DIR ${LIBDIR}/llvm CACHE PATH	"Path to the LLVM installation")
		set(LLVM_VERSION "3.4" CACHE STRING	"Version of LLVM to use")
		if(EXISTS "${LLVM_ROOT_DIR}/bin/llvm-config")
			set(LLVM_CONFIG "${LLVM_ROOT_DIR}/bin/llvm-config")
		else()
			set(LLVM_CONFIG llvm-config)
		endif()
		execute_process(COMMAND ${LLVM_CONFIG} --version
		                OUTPUT_VARIABLE LLVM_VERSION
		                OUTPUT_STRIP_TRAILING_WHITESPACE)
		execute_process(COMMAND ${LLVM_CONFIG} --prefix
		                OUTPUT_VARIABLE LLVM_ROOT_DIR
		                OUTPUT_STRIP_TRAILING_WHITESPACE)
		execute_process(COMMAND ${LLVM_CONFIG} --libdir
		                OUTPUT_VARIABLE LLVM_LIBPATH
		                OUTPUT_STRIP_TRAILING_WHITESPACE)
		find_library(LLVM_LIBRARY
		             NAMES LLVMAnalysis # first of a whole bunch of libs to get
		             PATHS ${LLVM_LIBPATH})

		if(LLVM_LIBRARY AND LLVM_ROOT_DIR AND LLVM_LIBPATH)
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
		find_path(OSL_INCLUDE_DIR OSL/oslclosure.h PATHS ${CYCLES_OSL}/include)
		find_program(OSL_COMPILER NAMES oslc PATHS ${CYCLES_OSL}/bin)
	
		if(OSL_INCLUDE_DIR AND OSL_LIBRARIES AND OSL_COMPILER)
			set(OSL_FOUND TRUE)
		else()
			message(STATUS "OSL not found")
			set(WITH_CYCLES_OSL OFF)
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
				# They are linked also to omp lib, so we need it in builddir for runtime exexcution,
				# TODO: remove all unneeded dependencies from these

				# for intermediate binaries, in respect to lib ID
				execute_process(
				        COMMAND ditto -arch ${CMAKE_OSX_ARCHITECTURES}
				        ${LIBDIR}/openmp/lib/libiomp5.dylib
				        ${CMAKE_BINARY_DIR}/Resources/lib/libiomp5.dylib)
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
	set(PLATFORM_LINKFLAGS
		"${PLATFORM_LINKFLAGS} -Xlinker -unexported_symbols_list -Xlinker ${CMAKE_SOURCE_DIR}/source/creator/osx_locals.map"
	)

	if(WITH_CXX11)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
		set(PLATFORM_LINKFLAGS "${PLATFORM_LINKFLAGS} -stdlib=libc++")
	endif()

	# Suppress ranlib "has no symbols" warnings (workaround for T48250)
	set(CMAKE_C_ARCHIVE_CREATE   "<CMAKE_AR> Scr <TARGET> <LINK_FLAGS> <OBJECTS>")
	set(CMAKE_CXX_ARCHIVE_CREATE "<CMAKE_AR> Scr <TARGET> <LINK_FLAGS> <OBJECTS>")
	set(CMAKE_C_ARCHIVE_FINISH   "<CMAKE_RANLIB> -no_warning_for_no_symbols -c <TARGET>")
	set(CMAKE_CXX_ARCHIVE_FINISH "<CMAKE_RANLIB> -no_warning_for_no_symbols -c <TARGET>")
endif()

#-----------------------------------------------------------------------------
# Common.

if(NOT WITH_FFTW3 AND WITH_MOD_OCEANSIM)
	message(FATAL_ERROR "WITH_MOD_OCEANSIM requires WITH_FFTW3 to be ON")
endif()

if(WITH_CYCLES)
	if(NOT WITH_OPENIMAGEIO)
		message(FATAL_ERROR
			"Cycles requires WITH_OPENIMAGEIO, the library may not have been found. "
			"Configure OIIO or disable WITH_CYCLES"
		)
	endif()
	if(NOT WITH_BOOST)
		message(FATAL_ERROR
			"Cycles requires WITH_BOOST, the library may not have been found. "
			"Configure BOOST or disable WITH_CYCLES"
		)
	endif()

	if(WITH_CYCLES_OSL)
		if(NOT WITH_LLVM)
			message(FATAL_ERROR
				"Cycles OSL requires WITH_LLVM, the library may not have been found. "
				"Configure LLVM or disable WITH_CYCLES_OSL"
			)
		endif()
	endif()
endif()

if(WITH_INTERNATIONAL)
	if(NOT WITH_BOOST)
		message(FATAL_ERROR
			"Internationalization requires WITH_BOOST, the library may not have been found. "
			"Configure BOOST or disable WITH_INTERNATIONAL"
		)
	endif()
endif()

# See TEST_SSE_SUPPORT() for how this is defined.

# Do it globally, SSE2 is required for quite some time now.
# Doing it now allows to use SSE/SSE2 in inline headers.
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
		set(OPENJPEG_DEFINES "")
	else()
		set(OPENJPEG_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/extern/libopenjpeg")
		set(OPENJPEG_DEFINES "-DOPJ_STATIC")
	endif()
	# Special handling of Windows platform where openjpeg is always static.
	if(WIN32)
		set(OPENJPEG_DEFINES "-DOPJ_STATIC")
	endif()
endif()

if(NOT WITH_SYSTEM_EIGEN3)
	set(EIGEN3_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/extern/Eigen3)
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
			message(FATAL_ERROR
				"Unable to find OpenGL ES libraries. "
				"Install them or disable WITH_SYSTEM_GLES."
			)
		endif()

		list(APPEND BLENDER_GL_LIBRARIES OPENGLES_LIBRARY)

	else()
		set(OPENGLES_LIBRARY "" CACHE FILEPATH "OpenGL ES 2.0 library file")
		mark_as_advanced(OPENGLES_LIBRARY)

		list(APPEND BLENDER_GL_LIBRARIES "${OPENGLES_LIBRARY}")

		if(NOT OPENGLES_LIBRARY)
			message(FATAL_ERROR
				"To compile WITH_GL_EGL you need to set OPENGLES_LIBRARY "
				"to the file path of an OpenGL ES 2.0 library."
			)
		endif()

	endif()

	if(WIN32)
		# Setup paths to files needed to install and redistribute Windows Blender with OpenGL ES

		set(OPENGLES_DLL "" CACHE FILEPATH "OpenGL ES 2.0 redistributable DLL file")
		mark_as_advanced(OPENGLES_DLL)

		if(NOT OPENGLES_DLL)
			message(FATAL_ERROR
				"To compile WITH_GL_PROFILE_ES20 you need to set OPENGLES_DLL to the file "
				"path of an OpenGL ES 2.0 runtime dynamic link library (DLL)."
			)
		endif()

		if(WITH_GL_ANGLE)
			list(APPEND GL_DEFINITIONS -DWITH_ANGLE)

			set(D3DCOMPILER_DLL "" CACHE FILEPATH "Direct3D Compiler redistributable DLL file (needed by ANGLE)")

			get_filename_component(D3DCOMPILER_FILENAME "${D3DCOMPILER_DLL}" NAME)
			list(APPEND GL_DEFINITIONS "-DD3DCOMPILER=\"\\\"${D3DCOMPILER_FILENAME}\\\"\"")

			mark_as_advanced(D3DCOMPILER_DLL)

			if(D3DCOMPILER_DLL STREQUAL "")
				message(FATAL_ERROR
					"To compile WITH_GL_ANGLE you need to set D3DCOMPILER_DLL to the file "
					"path of a copy of the DirectX redistributable DLL file: D3DCompiler_46.dll"
				)
			endif()

		endif()

	endif()

endif()

if(WITH_GL_EGL)
	list(APPEND GL_DEFINITIONS -DWITH_GL_EGL)

	if(WITH_SYSTEM_GLES)
		if(NOT OPENGLES_EGL_LIBRARY)
			message(FATAL_ERROR
				"Unable to find OpenGL ES libraries. "
				"Install them or disable WITH_SYSTEM_GLES."
			)
		endif()

		list(APPEND BLENDER_GL_LIBRARIES OPENGLES_EGL_LIBRARY)

	else()
		set(OPENGLES_EGL_LIBRARY "" CACHE FILEPATH "EGL library file")
		mark_as_advanced(OPENGLES_EGL_LIBRARY)

		list(APPEND BLENDER_GL_LIBRARIES "${OPENGLES_LIBRARY}" "${OPENGLES_EGL_LIBRARY}")

		if(NOT OPENGLES_EGL_LIBRARY)
			message(FATAL_ERROR
				"To compile WITH_GL_EGL you need to set OPENGLES_EGL_LIBRARY "
				"to the file path of an EGL library."
			)
		endif()

	endif()

	if(WIN32)
		# Setup paths to files needed to install and redistribute Windows Blender with OpenGL ES

		set(OPENGLES_EGL_DLL "" CACHE FILEPATH "EGL redistributable DLL file")
		mark_as_advanced(OPENGLES_EGL_DLL)

		if(NOT OPENGLES_EGL_DLL)
			message(FATAL_ERROR
				"To compile WITH_GL_EGL you need to set OPENGLES_EGL_DLL "
				"to the file path of an EGL runtime dynamic link library (DLL)."
			)
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
		if(NOT WITH_OPENMP_STATIC)
			set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
		else()
			# Typically avoid adding flags as defines but we can't
			# pass OpenMP flags to the linker for static builds, meaning
			# we can't add any OpenMP related flags to CFLAGS variables
			# since they're passed to the linker as well.
			add_definitions("${OpenMP_C_FLAGS}")

			find_library_static(OpenMP_LIBRARIES gomp ${CMAKE_CXX_IMPLICIT_LINK_DIRECTORIES})
		endif()
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
# Configure GLog/GFlags

if(WITH_LIBMV OR WITH_GTESTS OR (WITH_CYCLES AND WITH_CYCLES_LOGGING))
	set(GLOG_DEFINES
		-DGOOGLE_GLOG_DLL_DECL=
	)

	set(GFLAGS_DEFINES
		-DGFLAGS_DLL_DEFINE_FLAG=
		-DGFLAGS_DLL_DECLARE_FLAG=
		-DGFLAGS_DLL_DECL=
	)
endif()

#-----------------------------------------------------------------------------
# Configure Ceres

if(WITH_LIBMV)
	set(CERES_DEFINES)

	if(WITH_CXX11)
		# nothing to be done
	elseif(SHARED_PTR_FOUND)
		if(SHARED_PTR_TR1_MEMORY_HEADER)
			list(APPEND CERES_DEFINES -DCERES_TR1_MEMORY_HEADER)
		endif()
		if(SHARED_PTR_TR1_NAMESPACE)
			list(APPEND CERES_DEFINES -DCERES_TR1_SHARED_PTR)
		endif()
	else()
		message(FATAL_ERROR "Ceres: Unable to find shared_ptr.")
	endif()

	if(WITH_CXX11)
		list(APPEND CERES_DEFINES -DCERES_STD_UNORDERED_MAP)
	elseif(HAVE_STD_UNORDERED_MAP_HEADER)
		if(HAVE_UNORDERED_MAP_IN_STD_NAMESPACE)
			list(APPEND CERES_DEFINES -DCERES_STD_UNORDERED_MAP)
		else()
			if(HAVE_UNORDERED_MAP_IN_TR1_NAMESPACE)
				list(APPEND CERES_DEFINES -DCERES_STD_UNORDERED_MAP_IN_TR1_NAMESPACE)
			else()
				list(APPEND CERES_DEFINES -DCERES_NO_UNORDERED_MAP)
				message(STATUS "Ceres: Replacing unordered_map/set with map/set (warning: slower!)")
			endif()
		endif()
	else()
		if(HAVE_UNORDERED_MAP_IN_TR1_NAMESPACE)
			list(APPEND CERES_DEFINES -DCERES_TR1_UNORDERED_MAP)
		else()
			list(APPEND CERES_DEFINES -DCERES_NO_UNORDERED_MAP)
			message(STATUS "Ceres: Replacing unordered_map/set with map/set (warning: slower!)")
		endif()
	endif()
endif()

#-----------------------------------------------------------------------------
# Extra compile flags

if(CMAKE_COMPILER_IS_GNUCC)

	ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_ALL -Wall)
	ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_CAST_ALIGN -Wcast-align)
	ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_ERROR_IMPLICIT_FUNCTION_DECLARATION -Werror=implicit-function-declaration)
	ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_ERROR_RETURN_TYPE  -Werror=return-type)
	ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_ERROR_VLA -Werror=vla)
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
	ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_FORMAT_SIGN -Wformat-signedness)

	# gcc 4.2 gives annoying warnings on every file with this
	if(NOT "${CMAKE_C_COMPILER_VERSION}" VERSION_LESS "4.3")
		ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_UNINITIALIZED -Wuninitialized)
	endif()

	# versions before gcc4.6 give many BLI_math warnings
	if(NOT "${CMAKE_C_COMPILER_VERSION}" VERSION_LESS "4.6")
		ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_REDUNDANT_DECLS       -Wredundant-decls)
		ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_REDUNDANT_DECLS -Wredundant-decls)
	endif()

	# versions before gcc4.8 include global name-space.
	if(NOT "${CMAKE_C_COMPILER_VERSION}" VERSION_LESS "4.8")
		ADD_CHECK_C_COMPILER_FLAG(C_WARNINGS C_WARN_SHADOW -Wshadow)
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
	ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_ERROR_RETURN_TYPE  -Werror=return-type)
	ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_ERROR_IMPLICIT_FUNCTION_DECLARATION -Werror=implicit-function-declaration)
	ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_NO_CHAR_SUBSCRIPTS -Wno-char-subscripts)
	ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_NO_UNKNOWN_PRAGMAS -Wno-unknown-pragmas)
	ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_POINTER_ARITH -Wpointer-arith)
	ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_UNUSED_PARAMETER -Wunused-parameter)
	ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_WRITE_STRINGS -Wwrite-strings)
	ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_UNDEF -Wundef)
	ADD_CHECK_CXX_COMPILER_FLAG(CXX_WARNINGS CXX_WARN_FORMAT_SIGN -Wformat-signedness)

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
elseif(CMAKE_C_COMPILER_ID MATCHES "MSVC")
	# most msvc warnings are C & C++
	set(_WARNINGS
		# warning level:
		"/W3"
		"/w34062"  # switch statement contains 'default' but no 'case' labels
		# disable:
		"/wd4018"  # signed/unsigned mismatch
		"/wd4146"  # unary minus operator applied to unsigned type, result still unsigned
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
endif()

# ensure python header is found since detection can fail, this could happen
# with _any_ library but since we used a fixed python version this tends to
# be most problematic.
if(WITH_PYTHON)
	if(NOT EXISTS "${PYTHON_INCLUDE_DIR}/Python.h")
		message(FATAL_ERROR
			"Missing: \"${PYTHON_INCLUDE_DIR}/Python.h\",\n"
			"Set the cache entry 'PYTHON_INCLUDE_DIR' to point "
			"to a valid python include path. Containing "
			"Python.h for python version \"${PYTHON_VERSION}\""
		)
	endif()

	if(WIN32 OR APPLE)
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

if(WITH_CXX11)
	if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_C_COMPILER_ID MATCHES "Clang")
		# TODO(sergey): Do we want c++11 or gnu-c++11 here?
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
	elseif(MSVC12)
		# Nothing special is needed, C++11 features are available by default.
	else()
		message(FATAL_ERROR "Compiler ${CMAKE_C_COMPILER_ID} is not supported for C++11 build yet")
	endif()
else()
	# GCC-6 switched to C++11 by default, which would break linking with existing libraries
	# by default. So we explicitly disable C++11 for a new GCC so no linking issues happens.
	if(CMAKE_COMPILER_IS_GNUCC AND (NOT "${CMAKE_C_COMPILER_VERSION}" VERSION_LESS "6.0"))
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++98")
		# We also disable any of C++11 ABI from usage, so we wouldn't even try to
		# link to stuff from std::__cxx11 namespace.
		add_definitions("-D_GLIBCXX_USE_CXX11_ABI=0")
	endif()
endif()

# Visual Studio has all standards it supports available by default
if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_C_COMPILER_ID MATCHES "Clang" OR CMAKE_C_COMPILER_ID MATCHES "Intel")
	# Use C99 + GNU extensions, works with GCC, Clang, ICC
	if(WITH_C11)
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu11")
	else()
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99")
	endif()
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
	OSL_INCLUDE_DIR
)

mark_as_advanced(
	LLVM_CONFIG
	LLVM_ROOT_DIR
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
	if(WITH_CUDA_DYNLOAD)
		add_subdirectory(extern/cuew)
	endif()
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

	set(_config_msg "\nBlender Configuration\n=====================")

	function(info_cfg_option
		_setting
		)

		set(_msg "  - ${_setting}")
		string(LENGTH "${_msg}" _len)
		while("32" GREATER "${_len}")
			set(_msg "${_msg} ")
			math(EXPR _len "${_len} + 1")
		endwhile()

		set(_config_msg "${_config_msg}\n${_msg}${${_setting}}" PARENT_SCOPE)
	endfunction()

	function(info_cfg_text
		_text
		)

		set(_config_msg "${_config_msg}\n\n  ${_text}" PARENT_SCOPE)
	endfunction()

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
	info_cfg_option(WITH_OPENVDB)

	info_cfg_text("Compiler Options:")
	info_cfg_option(WITH_BUILDINFO)
	info_cfg_option(WITH_OPENMP)
	info_cfg_option(WITH_RAYOPTIMIZATION)

	info_cfg_text("System Options:")
	info_cfg_option(WITH_INSTALL_PORTABLE)
	info_cfg_option(WITH_X11_ALPHA)
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
	info_cfg_option(WITH_IMAGE_TIFF)

	info_cfg_text("Audio:")
	info_cfg_option(WITH_OPENAL)
	info_cfg_option(WITH_SDL)
	info_cfg_option(WITH_SDL_DYNLOAD)
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

	info_cfg_text("")

	message("${_config_msg}")
endif()

if(0)
	print_all_vars()
endif()

