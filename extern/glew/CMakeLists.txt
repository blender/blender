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

set(INC
	include
)

set(INC_SYS

)

if(UNIX)
	list(APPEND INC_SYS
		${X11_X11_INCLUDE_PATH}
	)
endif()

set(SRC
	src/glew.c

	include/GL/glew.h
	include/GL/glxew.h
	include/GL/wglew.h
)

add_definitions(-DGLEW_STATIC)

blender_add_lib(extern_glew "${SRC}" "${INC}" "${INC_SYS}")
