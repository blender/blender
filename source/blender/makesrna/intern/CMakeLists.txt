# $Id: CMakeLists.txt 17433 2008-11-12 21:16:53Z blendix $
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

SET(SRC
     makesrna.c
     rna_actuator.c
     rna_armature.c
     rna_brush.c
     rna_camera.c
     rna_color.c
     rna_constraint.c
     rna_controller.c
     rna_curve.c
     rna_define.c
     rna_fluidsim.c
     rna_group.c
     rna_ID.c
     rna_image.c
     rna_ipo.c
     rna_key.c
     rna_lamp.c
     rna_lattice.c
     rna_main.c
     rna_material.c
     rna_mesh.c
     rna_meta.c
     rna_modifier.c
     rna_nodetree.c
     rna_object.c
     rna_packedfile.c
     rna_property.c
     rna_radio.c
     rna_rna.c
     rna_scene.c
     rna_screen.c
     rna_sensor.c
     rna_sequence.c
     rna_sound.c
     rna_text.c
     rna_vfont.c
     rna_wm.c
     rna_world.c
     ../../../../intern/guardedalloc/intern/mallocn.c
     ../../../../intern/guardedalloc/intern/mmap_win.c)

INCLUDE_DIRECTORIES(../../../../intern/guardedalloc .. ../../makesdna ../../blenkernel ../../blenlib .)
FILE(GLOB INC_FILES ../*.h ../../makesdna/*.h)

# Build makesrna executable
ADD_EXECUTABLE(makesrna ${SRC} ${INC_FILES})
TARGET_LINK_LIBRARIES(makesrna bf_dna)

# Output rna.c
ADD_CUSTOM_COMMAND(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/rna.c
  COMMAND ${CMAKE_BINARY_DIR}/bin/${CMAKE_CFG_INTDIR}/makesrna ${CMAKE_CURRENT_BINARY_DIR}/rna.c ${CMAKE_SOURCE_DIR}/source/blender/makesrna/
  DEPENDS makesrna
)

# Build bf_rna
SET(SRC rna_access.c rna_dependency.c rna.c)
BLENDERLIB(bf_rna "${SRC}" "${INC}")

MESSAGE(STATUS "Configuring makesrna")
