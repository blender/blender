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
# Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# The Original Code is Copyright (C) 2006, Blender Foundation
# All rights reserved.
#
# The Original Code is: all of this file.
#
# Contributor(s): Jacques Beaurain.
#
# ***** END GPL LICENSE BLOCK *****

INCLUDE_DIRECTORIES(../../../../intern/guardedalloc ..)
FILE(GLOB INC_FILES ../*.h)

# Build makesdna executable
SET(SRC makesdna.c ../../../../intern/guardedalloc/intern/mallocn.c ../../../../intern/guardedalloc/intern/mmap_win.c)
ADD_EXECUTABLE(makesdna ${SRC} ${INC_FILES})

# Output dna.c
ADD_CUSTOM_COMMAND(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/dna.c
	COMMAND ${CMAKE_BINARY_DIR}/bin/${CMAKE_CFG_INTDIR}/makesdna ${CMAKE_CURRENT_BINARY_DIR}/dna.c ${CMAKE_SOURCE_DIR}/source/blender/makesdna/
	DEPENDS makesdna
)

# Build bf_dna library
SET(SRC dna_genfile.c dna.c)
BLENDERLIB(bf_dna "${SRC}" "${INC}")

MESSAGE(STATUS "Configuring makesdna")
