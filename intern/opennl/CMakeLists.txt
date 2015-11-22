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

# External project, better not fix warnings.
remove_strict_flags()

# remove debug flag here since this is not a blender maintained library
# and debug gives a lot of prints on UV unwrapping. developers can enable if they need to.
if(MSVC)
	remove_definitions(-DDEBUG)
else()
	add_definitions(-UDEBUG)
endif()


# quiet compiler warnings about undefined defines
add_definitions(
	-DDEBUGlevel=0
	-DPRNTlevel=0
)

set(INC
	extern
)

set(INC_SYS
	../../extern/colamd/Include
	../../extern/Eigen3
)

set(SRC
	intern/opennl.cpp
	extern/ONL_opennl.h
)

blender_add_lib(bf_intern_opennl "${SRC}" "${INC}" "${INC_SYS}")
