# SPDX-License-Identifier: GPL-2.0-or-later

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
  ../../../../intern/guardedalloc
  ../../../../intern/mantaflow/extern
  # RNA_prototypes.h
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS
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

if(WITH_BULLET)
  list(APPEND INC
    ../../../../intern/rigidbody
  )
  add_definitions(-DWITH_BULLET)
endif()


blender_add_lib(bf_editor_physics "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

# RNA_prototypes.h
add_dependencies(bf_editor_physics bf_rna)
