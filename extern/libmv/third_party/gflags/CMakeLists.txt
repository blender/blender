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
# Contributor(s): Blender Foundation,
#                 Sergey Sharybin
#
# ***** END GPL LICENSE BLOCK *****

set(INC
	.
	gflags
)

set(INC_SYS
)

set(SRC
	gflags.cc
	gflags_completions.cc
	gflags_reporting.cc

	config.h
	gflags/gflags_completions.h
	gflags/gflags_declare.h
	gflags/gflags_gflags.h
	gflags/gflags.h
	mutex.h
	util.h
)

if(WIN32)
	list(APPEND SRC
		windows_port.cc
		windows_port.h
	)
endif()

blender_add_lib(extern_gflags "${SRC}" "${INC}" "${INC_SYS}")
