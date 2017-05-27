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
# Contributor(s): Sergey Sharybin.
#
# ***** END GPL LICENSE BLOCK *****

# Libraries configuration for Windows.

add_definitions(-DWIN32)

if(MSVC)
	include(platform_win32_msvc)
else()
	message(FATAL_ERROR "Compiler is unsupported")
endif()

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
