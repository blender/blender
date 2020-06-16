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
# ***** END GPL LICENSE BLOCK *****

# message(STATUS "Configuring makesdna")

add_definitions(-DWITH_DNA_GHASH)

blender_include_dirs(
  ../../../../intern/atomic
  ../../../../intern/guardedalloc
  ../../blenlib
  ..
)


# -----------------------------------------------------------------------------
# Build makesdna executable
set(SRC
  dna_utils.c
  makesdna.c
  ../../blenlib/intern/BLI_ghash.c
  ../../blenlib/intern/BLI_ghash_utils.c
  ../../blenlib/intern/BLI_memarena.c
  ../../blenlib/intern/BLI_mempool.c
  ../../blenlib/intern/hash_mm2a.c  # needed by 'BLI_ghash_utils.c', not used directly.
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
setup_platform_linker_flags()

add_executable(makesdna ${SRC} ${SRC_DNA_INC})

if(WIN32 AND NOT UNIX)
  target_link_libraries(makesdna ${PTHREADS_LIBRARIES})
endif()

# Output dna.c
add_custom_command(
  OUTPUT
    ${CMAKE_CURRENT_BINARY_DIR}/dna.c
    ${CMAKE_CURRENT_BINARY_DIR}/dna_type_offsets.h
    ${CMAKE_CURRENT_BINARY_DIR}/dna_verify.c
  COMMAND
    "$<TARGET_FILE:makesdna>"
    ${CMAKE_CURRENT_BINARY_DIR}/dna.c
    ${CMAKE_CURRENT_BINARY_DIR}/dna_type_offsets.h
    ${CMAKE_CURRENT_BINARY_DIR}/dna_verify.c
    ${CMAKE_SOURCE_DIR}/source/blender/makesdna/
  DEPENDS makesdna
)


# -----------------------------------------------------------------------------
# Build bf_dna library
set(INC
  ${CMAKE_CURRENT_BINARY_DIR}
)

set(INC_SYS

)

set(SRC
  dna_defaults.c
  dna_genfile.c
  dna_utils.c
  ${CMAKE_CURRENT_BINARY_DIR}/dna.c
  ${CMAKE_CURRENT_BINARY_DIR}/dna_verify.c
  ${SRC_DNA_INC}

  ${CMAKE_CURRENT_BINARY_DIR}/dna_type_offsets.h
  dna_rename_defs.h
  dna_utils.h
)

set(LIB
)

set_source_files_properties(
  ${CMAKE_CURRENT_BINARY_DIR}/dna.c
  ${CMAKE_CURRENT_BINARY_DIR}/dna_type_offsets.h
  ${CMAKE_CURRENT_BINARY_DIR}/dna_verify.c
  PROPERTIES GENERATED TRUE
)

blender_add_lib(bf_dna "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")


# -----------------------------------------------------------------------------
# Build bf_dna_blenlib library
set(INC

)

set(INC_SYS

)

set(SRC
  ../../blenlib/intern/BLI_ghash.c
  ../../blenlib/intern/BLI_ghash_utils.c
  ../../blenlib/intern/BLI_linklist.c
  ../../blenlib/intern/BLI_memarena.c
  ../../blenlib/intern/BLI_mempool.c
  ../../blenlib/intern/endian_switch.c
  ../../blenlib/intern/hash_mm2a.c
  ../../blenlib/intern/listbase.c

  ../DNA_brush_defaults.h
  ../DNA_cachefile_defaults.h
  ../DNA_camera_defaults.h
  ../DNA_curve_defaults.h
  ../DNA_defaults.h
  ../DNA_hair_defaults.h
  ../DNA_image_defaults.h
  ../DNA_lattice_defaults.h
  ../DNA_light_defaults.h
  ../DNA_lightprobe_defaults.h
  ../DNA_linestyle_defaults.h
  ../DNA_material_defaults.h
  ../DNA_mesh_defaults.h
  ../DNA_meta_defaults.h
  ../DNA_object_defaults.h
  ../DNA_pointcloud_defaults.h
  ../DNA_scene_defaults.h
  ../DNA_simulation_defaults.h
  ../DNA_speaker_defaults.h
  ../DNA_texture_defaults.h
  ../DNA_vec_defaults.h
  ../DNA_view3d_defaults.h
  ../DNA_volume_defaults.h
  ../DNA_world_defaults.h
)

set(LIB
)

blender_add_lib(bf_dna_blenlib "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
