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
# ***** END GPL LICENSE BLOCK *****

set(INC
	src
	src/gflags
)

set(INC_SYS
)

set(SRC
	src/gflags.cc
	src/gflags_completions.cc
	src/gflags_reporting.cc

	src/gflags/config.h
	src/gflags/gflags_completions.h
	src/gflags/gflags_declare.h
	src/gflags/gflags_gflags.h
	src/gflags/gflags.h
	src/mutex.h
	src/util.h
)

if(WIN32)
	list(APPEND SRC
		src/windows_port.cc
		src/windows_port.h
	)
endif()

add_definitions(${GFLAGS_DEFINES})

blender_add_lib(extern_gflags "${SRC}" "${INC}" "${INC_SYS}")
