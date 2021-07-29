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

# message(STATUS "Configuring makesdna")

add_definitions(-DWITH_DNA_GHASH)

blender_include_dirs(
	../../../../intern/guardedalloc
	../../../../intern/atomic
	../../blenlib
	..
)


# -----------------------------------------------------------------------------
# Build makesdna executable
set(SRC
	makesdna.c
	../../../../intern/guardedalloc/intern/mallocn.c
	../../../../intern/guardedalloc/intern/mallocn_guarded_impl.c
	../../../../intern/guardedalloc/intern/mallocn_lockfree_impl.c
)

if(WIN32 AND NOT UNIX)
	list(APPEND SRC
		../../../../intern/guardedalloc/intern/mmap_win.c
	)
endif()

# SRC_DNA_INC is defined in the parent dir

add_cc_flags_custom_test(makesdna)

add_executable(makesdna ${SRC} ${SRC_DNA_INC})

# Output dna.c
add_custom_command(
	OUTPUT
		${CMAKE_CURRENT_BINARY_DIR}/dna.c
		${CMAKE_CURRENT_BINARY_DIR}/dna_type_offsets.h
	COMMAND
		"$<TARGET_FILE:makesdna>"
		${CMAKE_CURRENT_BINARY_DIR}/dna.c
		${CMAKE_CURRENT_BINARY_DIR}/dna_type_offsets.h
		${CMAKE_SOURCE_DIR}/source/blender/makesdna/
	DEPENDS makesdna
)


# -----------------------------------------------------------------------------
# Build bf_dna library
set(INC

)

set(INC_SYS

)

set(SRC
	dna_genfile.c
	${CMAKE_CURRENT_BINARY_DIR}/dna.c
	${SRC_DNA_INC}
)

set_source_files_properties(
	${CMAKE_CURRENT_BINARY_DIR}/dna.c
	${CMAKE_CURRENT_BINARY_DIR}/dna_type_offsets.h
	PROPERTIES GENERATED TRUE
)

blender_add_lib(bf_dna "${SRC}" "${INC}" "${INC_SYS}")


# -----------------------------------------------------------------------------
# Build bf_dna_blenlib library
set(INC

)

set(INC_SYS

)

set(SRC
	../../blenlib/intern/BLI_ghash.c
	../../blenlib/intern/BLI_mempool.c
	../../blenlib/intern/endian_switch.c
	../../blenlib/intern/hash_mm2a.c
	../../blenlib/intern/listbase.c
)

blender_add_lib(bf_dna_blenlib "${SRC}" "${INC}" "${INC_SYS}")
