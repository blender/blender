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
# The Original Code is Copyright (C) 2012, Blender Foundation
# All rights reserved.
#
# The Original Code is: all of this file.
#
# Contributor(s): Bastien Montagne.
#
# ***** END GPL LICENSE BLOCK *****

set(INC
	.
)

set(INC_SYS
)

set(SRC
	boost_locale_wrapper.cpp

	boost_locale_wrapper.h
)

if(APPLE)
	# Cocoa code to read the locale on OSX
	list(APPEND SRC
		osx_user_locale.mm
	)
endif()

if(WITH_HEADLESS)
	add_definitions(-DWITH_HEADLESS)
endif()

if(WITH_GHOST_SDL)
	add_definitions(-DWITH_GHOST_SDL)
endif()

if(WITH_INTERNATIONAL)
	list(APPEND INC_SYS
		${BOOST_INCLUDE_DIR}
	)
	add_definitions(-DWITH_INTERNATIONAL)
	add_definitions(${BOOST_DEFINITIONS})
endif()

blender_add_lib(bf_intern_locale "${SRC}" "${INC}" "${INC_SYS}")

# -----------------------------------------------------------------------------
# Build msgfmt executable
set(MSFFMT_SRC
	msgfmt.cc
)
add_executable(msgfmt ${MSFFMT_SRC})

if(CMAKE_C_COMPILER_ID MATCHES "Clang" AND (NOT (CMAKE_C_COMPILER_VERSION VERSION_LESS 3.4)))
	# needed for clang 3.4+
	target_link_libraries(msgfmt ${PLATFORM_LINKLIBS})
endif()

