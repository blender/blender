# $Id$
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
	gen_messaging
	gen_system
	../../intern/string
	../../intern/moto/include
	../../source/blender/blenloader
)

set(SRC
	gen_messaging/intern/messaging.c
	gen_system/GEN_HashedPtr.cpp
	gen_system/SYS_SingletonSystem.cpp
	gen_system/SYS_System.cpp

	gen_messaging/GEN_messaging.h
	gen_system/GEN_HashedPtr.h
	gen_system/GEN_Map.h
	gen_system/SYS_SingletonSystem.h
	gen_system/SYS_System.h
)

blender_add_lib(bf_gen_system "${SRC}" "${INC}")
