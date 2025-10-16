# SPDX-FileCopyrightText: 2006 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  intern
  intern/lineart
  ../editors/include
  ../makesrna
  ../../../intern/eigen

  # RNA_prototypes.hh
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS
  ${ZLIB_INCLUDE_DIRS}
)

set(SRC
  intern/MOD_armature.cc
  intern/MOD_array.cc
  intern/MOD_bevel.cc
  intern/MOD_boolean.cc
  intern/MOD_build.cc
  intern/MOD_cast.cc
  intern/MOD_cloth.cc
  intern/MOD_collision.cc
  intern/MOD_correctivesmooth.cc
  intern/MOD_curve.cc
  intern/MOD_datatransfer.cc
  intern/MOD_decimate.cc
  intern/MOD_displace.cc
  intern/MOD_dynamicpaint.cc
  intern/MOD_edgesplit.cc
  intern/MOD_explode.cc
  intern/MOD_fluid.cc
  intern/MOD_grease_pencil_armature.cc
  intern/MOD_grease_pencil_array.cc
  intern/MOD_grease_pencil_build.cc
  intern/MOD_grease_pencil_color.cc
  intern/MOD_grease_pencil_dash.cc
  intern/MOD_grease_pencil_envelope.cc
  intern/MOD_grease_pencil_hook.cc
  intern/MOD_grease_pencil_lattice.cc
  intern/MOD_grease_pencil_length.cc
  intern/MOD_grease_pencil_mirror.cc
  intern/MOD_grease_pencil_multiply.cc
  intern/MOD_grease_pencil_noise.cc
  intern/MOD_grease_pencil_offset.cc
  intern/MOD_grease_pencil_opacity.cc
  intern/MOD_grease_pencil_outline.cc
  intern/MOD_grease_pencil_shrinkwrap.cc
  intern/MOD_grease_pencil_simplify.cc
  intern/MOD_grease_pencil_smooth.cc
  intern/MOD_grease_pencil_subdiv.cc
  intern/MOD_grease_pencil_texture.cc
  intern/MOD_grease_pencil_thickness.cc
  intern/MOD_grease_pencil_time.cc
  intern/MOD_grease_pencil_tint.cc
  intern/MOD_grease_pencil_util.cc
  intern/MOD_grease_pencil_weight_angle.cc
  intern/MOD_grease_pencil_weight_proximity.cc
  intern/MOD_hook.cc
  intern/MOD_laplaciandeform.cc
  intern/MOD_laplaciansmooth.cc
  intern/MOD_lattice.cc
  intern/MOD_lineart.cc
  intern/MOD_mask.cc
  intern/MOD_mesh_to_volume.cc
  intern/MOD_meshcache.cc
  intern/MOD_meshcache_mdd.cc
  intern/MOD_meshcache_pc2.cc
  intern/MOD_meshcache_util.cc
  intern/MOD_meshdeform.cc
  intern/MOD_meshsequencecache.cc
  intern/MOD_mirror.cc
  intern/MOD_multires.cc
  intern/MOD_nodes.cc
  intern/MOD_none.cc
  intern/MOD_normal_edit.cc
  intern/MOD_ocean.cc
  intern/MOD_particleinstance.cc
  intern/MOD_particlesystem.cc
  intern/MOD_remesh.cc
  intern/MOD_screw.cc
  intern/MOD_shapekey.cc
  intern/MOD_shrinkwrap.cc
  intern/MOD_simpledeform.cc
  intern/MOD_skin.cc
  intern/MOD_smooth.cc
  intern/MOD_softbody.cc
  intern/MOD_solidify.cc
  intern/MOD_solidify_extrude.cc
  intern/MOD_solidify_nonmanifold.cc
  intern/MOD_subsurf.cc
  intern/MOD_surface.cc
  intern/MOD_surfacedeform.cc
  intern/MOD_triangulate.cc
  intern/MOD_ui_common.cc
  intern/MOD_util.cc
  intern/MOD_uvproject.cc
  intern/MOD_uvwarp.cc
  intern/MOD_volume_displace.cc
  intern/MOD_volume_to_mesh.cc
  intern/MOD_warp.cc
  intern/MOD_wave.cc
  intern/MOD_weighted_normal.cc
  intern/MOD_weightvg_util.cc
  intern/MOD_weightvgedit.cc
  intern/MOD_weightvgmix.cc
  intern/MOD_weightvgproximity.cc
  intern/MOD_weld.cc
  intern/MOD_wireframe.cc
  intern/lineart/lineart_chain.cc
  intern/lineart/lineart_cpu.cc
  intern/lineart/lineart_shadow.cc
  intern/lineart/lineart_util.cc

  MOD_modifiertypes.hh
  MOD_nodes.hh
  intern/MOD_grease_pencil_util.hh
  intern/MOD_meshcache_util.hh
  intern/MOD_solidify_util.hh
  intern/MOD_ui_common.hh
  intern/MOD_util.hh
  intern/MOD_weightvg_util.hh
  intern/lineart/MOD_lineart.hh
  intern/lineart/lineart_intern.hh
)

set(LIB
  PRIVATE bf::blenfont
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::blenloader
  PRIVATE bf::blentranslation
  PRIVATE bf::bmesh
  PRIVATE bf::depsgraph
  PUBLIC bf::dna
  PRIVATE bf::functions
  PRIVATE bf::geometry
  PRIVATE bf::intern::guardedalloc
  PRIVATE bf::extern::fmtlib
  PRIVATE bf::extern::xxhash
  PRIVATE bf::nodes
  PRIVATE bf::render
  PRIVATE bf::windowmanager
)

if(WITH_ALEMBIC)
  add_definitions(-DWITH_ALEMBIC)
  list(APPEND INC
    ../io/alembic
  )
  list(APPEND LIB
    bf_io_alembic
  )
endif()

if(WITH_USD)
  add_definitions(-DWITH_USD)
  list(APPEND INC
    ../io/usd
  )
  list(APPEND LIB
    bf_io_usd
  )
endif()

if(WITH_MOD_REMESH)
  list(APPEND INC
    ../../../intern/dualcon
  )
  list(APPEND LIB
    bf_intern_dualcon
  )
  add_definitions(-DWITH_MOD_REMESH)
endif()

if(WITH_MOD_FLUID)
  add_definitions(-DWITH_FLUID)
endif()

if(WITH_MOD_OCEANSIM)
  add_definitions(-DWITH_OCEANSIM)
endif()

if(WITH_OPENSUBDIV)
  add_definitions(-DWITH_OPENSUBDIV)
endif()

if(WITH_BULLET)
  list(APPEND LIB
    ${BULLET_LIBRARIES}
  )
  add_definitions(-DWITH_BULLET)
endif()

# To disable adaptive subdivision test in subdivision-surface UI without cycles.
if(WITH_CYCLES)
  add_definitions(-DWITH_CYCLES)
endif()

if(WITH_GMP)
  add_definitions(-DWITH_GMP)

  list(APPEND INC_SYS
    ${GMP_INCLUDE_DIRS}
  )

  list(APPEND LIB
    ${GMP_LIBRARIES}
  )
endif()

if(WITH_TBB)
  if(WIN32)
    # TBB includes Windows.h which will define min/max macros
    # that will collide with the STL versions.
    add_definitions(-DNOMINMAX)
  endif()
endif()

if(WITH_EXPERIMENTAL_FEATURES)
  add_definitions(-DWITH_SIMULATION_DATABLOCK)
endif()

# So we can have special tricks in modifier system.

blender_add_lib(bf_modifiers "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

# RNA_prototypes.hh
add_dependencies(bf_modifiers bf_rna)
