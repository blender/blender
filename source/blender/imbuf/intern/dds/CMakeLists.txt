# $Id: CMakeLists.txt 14444 2008-04-16 22:40:48Z hos $
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
# Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# The Original Code is Copyright (C) 2006, Blender Foundation
# All rights reserved.
#
# The Original Code is: all of this file.
#
# Contributor(s): 
#
# ***** END GPL LICENSE BLOCK *****

FILE (GLOB SRC *.cpp)

SET(INC 
	.
	../../../blenkernel
	../../../makesdna
	../../
	..
	../../../blenlib
	intern/include
	../../../../../intern/guardedalloc
)

if(WITH_DDS)
	ADD_DEFINITIONS(-DWITH_DDS)
ENDIF(WITH_DDS)

BLENDERLIB(bf_dds "${SRC}" "${INC}")                 
#env.BlenderLib ('bf_dds', source_files, incs, defs, libtype=['core','player'], priority = [90, 200])
