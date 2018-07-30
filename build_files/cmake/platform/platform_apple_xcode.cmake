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
# The Original Code is Copyright (C) 2016, Blender Foundation
# All rights reserved.
#
# Contributor(s): Jacques Beaurain.
#
# ***** END GPL LICENSE BLOCK *****

# Xcode and system configuration for Apple.

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

# 10.9 is our min. target, if you use higher sdk, weak linking happens
if(CMAKE_OSX_DEPLOYMENT_TARGET)
	if(${CMAKE_OSX_DEPLOYMENT_TARGET} VERSION_LESS 10.9)
		message(STATUS "Setting deployment target to 10.9, lower versions are not supported")
		set(CMAKE_OSX_DEPLOYMENT_TARGET "10.9" CACHE STRING "" FORCE)
	endif()
else()
	set(CMAKE_OSX_DEPLOYMENT_TARGET "10.9" CACHE STRING "" FORCE)
endif()

if(NOT ${CMAKE_GENERATOR} MATCHES "Xcode")
	# force CMAKE_OSX_DEPLOYMENT_TARGET for makefiles, will not work else ( cmake bug ? )
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}")
	add_definitions("-DMACOSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}")
endif()
