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
# ***** END GPL LICENSE BLOCK *****

set(INC
  ../include
  ../../blenkernel
  ../../blenlib
  ../../blentranslation
  ../../depsgraph
  ../../gpu
  ../../makesdna
  ../../makesrna
  ../../windowmanager
  ../../../../intern/clog
  ../../../../intern/glew-mx
  ../../../../intern/guardedalloc
  ../../../../intern/mantaflow/extern
)

set(SRC
  dynamicpaint_ops.c
  particle_boids.c
  particle_edit.c
  particle_edit_undo.c
  particle_object.c
  physics_fluid.c
  physics_ops.c
  physics_pointcache.c
  rigidbody_constraint.c
  rigidbody_object.c
  rigidbody_world.c

  particle_edit_utildefines.h
  physics_intern.h
)

set(LIB
  bf_blenkernel
  bf_blenlib
)

if(WITH_MOD_FLUID)
  list(APPEND LIB
    bf_intern_mantaflow
  )
  add_definitions(-DWITH_FLUID)
endif()

if(WITH_INTERNATIONAL)
  add_definitions(-DWITH_INTERNATIONAL)
endif()

if(WITH_BULLET)
  list(APPEND INC
    ../../../../intern/rigidbody
  )
  add_definitions(-DWITH_BULLET)
endif()


blender_add_lib(bf_editor_physics "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
