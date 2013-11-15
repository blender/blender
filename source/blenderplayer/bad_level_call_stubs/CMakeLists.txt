
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

# this warning on generated files gets annoying
remove_strict_flags()

set(INC 
	.
	..
	../../blender/blenlib
	../../blender/blenkernel
	../../blender/blenloader
	../../blender/makesdna
	../../blender/makesrna
	../../../intern/guardedalloc
)

set(INC_SYS
	
)

set(SRC
	stubs.c
)

if(WITH_BUILDINFO)
	list(APPEND SRC
		../../creator/buildinfo.c
	)
	add_definitions(-DBUILD_DATE="\"\""
					-DBUILD_TIME="\"\""
					-DBUILD_COMMIT_TIMESTAMP=0
					-DBUILD_COMMIT_DATE="\"\""
					-DBUILD_COMMIT_TIME="\"\""
					-DBUILD_HASH="\"\""
					-DBUILD_PLATFORM="\"\""
					-DBUILD_BRANCH="\"\""
					-DBUILD_TYPE="\"\""
					)
endif()

if(WITH_GAMEENGINE)
	add_definitions(-DWITH_GAMEENGINE)
endif()

if(WITH_FREESTYLE)
	add_definitions(-DWITH_FREESTYLE)
endif()

blender_add_lib_nolist(blenkernel_blc "${SRC}" "${INC}" "${INC_SYS}")
