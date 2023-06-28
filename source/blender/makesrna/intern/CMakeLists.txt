# SPDX-FileCopyrightText: 2006 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

if(CMAKE_COMPILER_IS_GNUCC)
  # add here so we fail early.
  string(APPEND CMAKE_C_FLAGS " -Werror=implicit-function-declaration")
endif()

# Needed for `mallocn.c`.
if(HAVE_MALLOC_STATS_H)
  add_definitions(-DHAVE_MALLOC_STATS_H)
endif()

# files rna_access.cc rna_define.c makesrna.c intentionally excluded.
set(DEFSRC
  rna_ID.c
  rna_action.c
  rna_animation.c
  rna_animviz.c
  rna_armature.cc
  rna_asset.c
  rna_attribute.c
  rna_boid.c
  rna_brush.c
  rna_cachefile.c
  rna_camera.c
  rna_cloth.c
  rna_collection.c
  rna_color.c
  rna_constraint.c
  rna_context.c
  rna_curve.cc
  rna_curveprofile.c
  rna_curves.cc
  rna_depsgraph.c
  rna_dynamicpaint.c
  rna_fcurve.c
  rna_fluid.c
  rna_gpencil_legacy.c
  rna_gpencil_legacy_modifier.c
  rna_image.c
  rna_key.c
  rna_lattice.cc
  rna_layer.c
  rna_light.c
  rna_lightprobe.c
  rna_linestyle.c
  rna_main.c
  rna_mask.cc
  rna_material.c
  rna_mesh.cc
  rna_meta.cc
  rna_modifier.c
  rna_movieclip.c
  rna_nla.c
  rna_nodetree.cc
  rna_object.cc
  rna_object_force.c
  rna_packedfile.c
  rna_palette.c
  rna_particle.c
  rna_pointcloud.c
  rna_pose.cc
  rna_render.c
  rna_rigidbody.c
  rna_rna.c
  rna_scene.c
  rna_screen.c
  rna_sculpt_paint.c
  rna_sequencer.c
  rna_shader_fx.c
  rna_sound.c
  rna_space.cc
  rna_speaker.c
  rna_test.c
  rna_text.c
  rna_texture.c
  rna_timeline.c
  rna_tracking.c
  rna_ui.cc
  rna_userdef.c
  rna_vfont.c
  rna_volume.c
  rna_wm.c
  rna_wm_gizmo.c
  rna_workspace.c
  rna_world.c
  rna_xr.c
)

if(WITH_EXPERIMENTAL_FEATURES)
  add_definitions(-DWITH_SIMULATION_DATABLOCK)
  add_definitions(-DWITH_GREASE_PENCIL_V3)
  list(APPEND DEFSRC
    rna_grease_pencil.cc
    rna_simulation.c
  )
endif()

set(APISRC
  rna_action_api.c
  rna_animation_api.c
  rna_armature_api.cc
  rna_camera_api.c
  rna_curve_api.cc
  rna_fcurve_api.c
  rna_image_api.c
  rna_lattice_api.cc
  rna_main_api.c
  rna_material_api.c
  rna_mesh_api.cc
  rna_meta_api.c
  rna_object_api.cc
  rna_pose_api.c
  rna_scene_api.c
  rna_sequencer_api.c
  rna_sound_api.c
  rna_space_api.cc
  rna_text_api.c
  rna_texture_api.c
  rna_ui_api.cc
  rna_vfont_api.c
  rna_wm_api.c
  rna_wm_gizmo_api.c
  rna_workspace_api.c
)

string(REGEX REPLACE "rna_([a-zA-Z0-9_-]*).c" "${CMAKE_CURRENT_BINARY_DIR}/rna_\\1_gen.c" GENSRC "${DEFSRC}")
list(APPEND GENSRC
  "${CMAKE_CURRENT_BINARY_DIR}/rna_prototypes_gen.h"
  "${CMAKE_CURRENT_BINARY_DIR}/../RNA_prototypes.h"
)
set_source_files_properties(${GENSRC} PROPERTIES GENERATED TRUE)

# --------------------------
# CFLAGS for Generated Files
#
# less strict flags for generated source
set(GENSRC_FLAGS)
set(GENSRC_CFLAGS)
set(GENSRC_CXXFLAGS)
if(CMAKE_COMPILER_IS_GNUCC OR (CMAKE_C_COMPILER_ID MATCHES "Clang"))
  set(GENSRC_CFLAGS "-Wno-missing-prototypes")
  set(GENSRC_CXXFLAGS "-Wno-missing-declarations")
endif()
if(CMAKE_C_COMPILER_ID MATCHES "Clang")
  string(APPEND GENSRC_FLAGS " -Wno-missing-variable-declarations")
elseif(MSVC)
  # Restore warn C4100 (unreferenced formal parameter) back to w4
  remove_cc_flag(/w34100)
endif()

set(GENSRC_C ${GENSRC})
list(FILTER GENSRC_C INCLUDE REGEX ".*\.c$")
set(GENSRC_CXX ${GENSRC})
list(FILTER GENSRC_CXX INCLUDE REGEX ".*\.cc$")

if(GENSRC_FLAGS)
  set_source_files_properties(${GENSRC} PROPERTIES COMPILE_FLAGS "${GENSRC_FLAGS}")
endif()

if(GENSRC_CFLAGS)
  set_source_files_properties(${GENSRC_C} PROPERTIES COMPILE_FLAGS "${GENSRC_CFLAGS}")
endif()

if(GENSRC_CXXFLAGS)
  set_source_files_properties(${GENSRC_CXX} PROPERTIES COMPILE_FLAGS "${GENSRC_CXXFLAGS}")
endif()

unset(GENSRC_C)
unset(GENSRC_CXX)
unset(GENSRC_FLAGS)
unset(GENSRC_CFLAGS)
unset(GENSRC_CXXFLAGS)


# NOTE: Disable clang-tidy because generated files are stored outside of the source,
# so the clang-tidy can not find our .clang-tidy and fall-backs to own set of rules
# which are too noisy for Blender.
#
# In the future clang-tidy would either need to be inlined checks and passed via the
# command line (instead of using .clang-tidy file). Or, maybe, there is a way to
# pass configuration file to the clang-tidy command.
unset(CMAKE_C_CLANG_TIDY)
unset(CMAKE_CXX_CLANG_TIDY)

set(SRC_RNA_INC
  ../RNA_access.h
  ../RNA_define.h
  ../RNA_documentation.h
  ../RNA_enum_items.h
  ../RNA_enum_types.h
  ../RNA_path.h
  ../RNA_types.h
)

set(SRC
  makesrna.c
  rna_define.c
  ${DEFSRC}
  ${APISRC}
  ../../../../intern/clog/clog.c
  ../../../../intern/guardedalloc/intern/leak_detector.cc
  ../../../../intern/guardedalloc/intern/mallocn.c
  ../../../../intern/guardedalloc/intern/mallocn_guarded_impl.c
  ../../../../intern/guardedalloc/intern/mallocn_lockfree_impl.c
  ../../../../intern/guardedalloc/intern/memory_usage.cc

  # Needed for defaults.
  ../../../../release/datafiles/userdef/userdef_default.c
  ../../../../release/datafiles/userdef/userdef_default_theme.c
)

set(INC
  .
  ..
  ../../asset_system
  ../../blenfont
  ../../blenkernel
  ../../blenlib
  ../../blenloader
  ../../blentranslation
  ../../bmesh
  ../../depsgraph
  ../../draw
  ../../gpu
  ../../ikplugin
  ../../imbuf
  ../../makesdna
  ../../modifiers
  ../../nodes
  ../../sequencer
  ../../simulation
  ../../windowmanager
  ../../editors/include
  ../../render
  ../../../../intern/clog
  ../../../../intern/cycles/blender
  ../../../../intern/guardedalloc
  ../../../../intern/memutil
  ../../../../intern/mantaflow/extern

  ${CMAKE_BINARY_DIR}/source/blender/makesdna/intern

  # dna_type_offsets.h
  ${CMAKE_CURRENT_BINARY_DIR}/../../makesdna/intern
  # RNA_prototypes.h
  ${CMAKE_CURRENT_BINARY_DIR}/../../makesrna/
)

set(INC_SYS

)

if(WITH_CYCLES)
  add_definitions(-DWITH_CYCLES)
endif()

if(WITH_PYTHON)
  add_definitions(-DWITH_PYTHON)
  list(APPEND INC
    ../../python
  )
endif()

if(WITH_IMAGE_OPENEXR)
  add_definitions(-DWITH_OPENEXR)
endif()

if(WITH_IMAGE_OPENJPEG)
  add_definitions(-DWITH_OPENJPEG)
endif()

if(WITH_IMAGE_CINEON)
  add_definitions(-DWITH_CINEON)
endif()

if(WITH_IMAGE_WEBP)
  add_definitions(-DWITH_WEBP)
endif()

if(WITH_AUDASPACE)
  list(APPEND INC_SYS
    ${AUDASPACE_C_INCLUDE_DIRS}
  )
  if(WITH_SYSTEM_AUDASPACE)
    list(APPEND LIB
      ${AUDASPACE_C_LIBRARIES}
      ${AUDASPACE_PY_LIBRARIES}
    )
  endif()
  add_definitions(-DWITH_AUDASPACE)
endif()

if(WITH_CODEC_FFMPEG)
  list(APPEND INC
    ../../../../intern/ffmpeg
  )
  list(APPEND INC_SYS
    ${FFMPEG_INCLUDE_DIRS}
  )
  list(APPEND LIB
    ${FFMPEG_LIBRARIES}
  )
  add_definitions(-DWITH_FFMPEG)
endif()

if(WITH_FFTW3)
  add_definitions(-DWITH_FFTW3)
endif()

if(WITH_MOD_FLUID)
  add_definitions(-DWITH_FLUID)
endif()

if(WITH_MOD_OCEANSIM)
  add_definitions(-DWITH_OCEANSIM)
endif()

if(WITH_SDL)
  if(WITH_SDL_DYNLOAD)
    add_definitions(-DWITH_SDL_DYNLOAD)
    list(APPEND INC
      ../../../../extern/sdlew/include
    )
  endif()
  add_definitions(-DWITH_SDL)
endif()

if(WITH_OPENAL)
  add_definitions(-DWITH_OPENAL)
endif()

if(WITH_COREAUDIO)
  add_definitions(-DWITH_COREAUDIO)
endif()

if(WITH_JACK)
  add_definitions(-DWITH_JACK)
endif()

if(WITH_PULSEAUDIO)
  add_definitions(-DWITH_PULSEAUDIO)
endif()

if(WITH_WASAPI)
  add_definitions(-DWITH_WASAPI)
endif()

if(WITH_OPENCOLLADA)
  add_definitions(-DWITH_COLLADA)
endif()

if(WITH_INTERNATIONAL)
  add_definitions(-DWITH_INTERNATIONAL)
endif()

if(WITH_ALEMBIC)
  list(APPEND INC
    ../../io/alembic
  )
  add_definitions(-DWITH_ALEMBIC)
endif()

if(WITH_BULLET)
  list(APPEND INC
    ../../../../intern/rigidbody
  )
  add_definitions(-DWITH_BULLET)
endif()

if(WITH_FREESTYLE)
  list(APPEND INC
    ../../freestyle
  )
  add_definitions(-DWITH_FREESTYLE)
endif()

if(WITH_OPENSUBDIV)
  list(APPEND INC
    ../../../../intern/opensubdiv
  )
  add_definitions(-DWITH_OPENSUBDIV)
endif()

if(WITH_OPENVDB)
  add_definitions(-DWITH_OPENVDB)

  if(WITH_OPENVDB_BLOSC)
    add_definitions(-DWITH_OPENVDB_BLOSC)
  endif()
endif()

if(WITH_INPUT_NDOF)
  add_definitions(-DWITH_INPUT_NDOF)
endif()

if(WITH_XR_OPENXR)
  add_definitions(-DWITH_XR_OPENXR)
endif()

if(WITH_VULKAN_METAL)
  add_definitions(-DWITH_VULKAN_METAL)
endif()

if(WITH_VULKAN_BACKEND)
  add_definitions(-DWITH_VULKAN_BACKEND)
endif()

if(WITH_GMP)
  add_definitions(-DWITH_GMP)
endif()

# Build makesrna executable
add_cc_flags_custom_test(makesrna)

add_executable(makesrna ${SRC} ${SRC_RNA_INC} ${SRC_DNA_INC})
setup_platform_linker_flags(makesrna)
blender_target_include_dirs(makesrna ${INC})
blender_target_include_dirs_sys(makesrna ${INC_SYS})

target_link_libraries(makesrna PRIVATE bf_dna)
target_link_libraries(makesrna PRIVATE bf_intern_atomic)
target_link_libraries(makesrna PRIVATE bf_dna_blenlib)

if(WIN32 AND NOT UNIX)
  if(DEFINED PTHREADS_LIBRARIES)
    target_link_libraries(makesrna PRIVATE ${PTHREADS_LIBRARIES})
  endif()
endif()

# Output `rna_*_gen.c`.
# NOTE: (Linux only): with crashes try add this after COMMAND:
# `valgrind --leak-check=full --track-origins=yes`
add_custom_command(
  OUTPUT ${GENSRC}
  COMMAND "$<TARGET_FILE:makesrna>" ${CMAKE_CURRENT_BINARY_DIR}/ ${CMAKE_CURRENT_BINARY_DIR}/../
  DEPENDS makesrna
)

# Build bf_rna
set(SRC
  rna_access.cc
  rna_access_compare_override.c
  rna_path.cc
  ${GENSRC}

  ${SRC_RNA_INC}
  rna_access_internal.h
  rna_internal.h
  rna_internal_types.h
  rna_mesh_utils.hh
)

set(LIB
  bf_dna
  bf_editor_space_api

  bf_editor_animation
  bf_editor_armature
  bf_editor_asset
  bf_editor_curve
  bf_editor_curves
  bf_editor_gizmo_library
  bf_editor_gpencil_legacy
  bf_editor_io
  bf_editor_mesh
  bf_editor_object
  bf_editor_physics
  bf_editor_render
  bf_editor_scene
  bf_editor_sculpt_paint
  bf_editor_sound
  bf_editor_transform
  bf_editor_undo
)

blender_add_lib(bf_rna "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

# Needed so we can use dna_type_offsets.h for defaults initialization.
add_dependencies(bf_blenkernel bf_dna)
