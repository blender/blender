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

if(CMAKE_COMPILER_IS_GNUCC)
  # add here so we fail early.
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror=implicit-function-declaration")
endif()

# files rna_access.c rna_define.c makesrna.c intentionally excluded.
set(DEFSRC
  rna_ID.c
  rna_action.c
  rna_animation.c
  rna_animviz.c
  rna_armature.c
  rna_boid.c
  rna_brush.c
  rna_cachefile.c
  rna_camera.c
  rna_cloth.c
  rna_collection.c
  rna_color.c
  rna_constraint.c
  rna_context.c
  rna_curve.c
  rna_curveprofile.c
  rna_depsgraph.c
  rna_dynamicpaint.c
  rna_fcurve.c
  rna_fluid.c
  rna_gpencil.c
  rna_gpencil_modifier.c
  rna_image.c
  rna_key.c
  rna_lattice.c
  rna_layer.c
  rna_light.c
  rna_lightprobe.c
  rna_linestyle.c
  rna_main.c
  rna_mask.c
  rna_material.c
  rna_mesh.c
  rna_meta.c
  rna_modifier.c
  rna_movieclip.c
  rna_nla.c
  rna_nodetree.c
  rna_object.c
  rna_object_force.c
  rna_packedfile.c
  rna_palette.c
  rna_particle.c
  rna_pose.c
  rna_render.c
  rna_rigidbody.c
  rna_rna.c
  rna_scene.c
  rna_screen.c
  rna_sculpt_paint.c
  rna_sequencer.c
  rna_shader_fx.c
  rna_sound.c
  rna_space.c
  rna_speaker.c
  rna_test.c
  rna_text.c
  rna_texture.c
  rna_timeline.c
  rna_tracking.c
  rna_ui.c
  rna_userdef.c
  rna_vfont.c
  rna_volume.c
  rna_wm.c
  rna_wm_gizmo.c
  rna_workspace.c
  rna_world.c
  rna_xr.c
)

if(WITH_NEW_OBJECT_TYPES)
  list(APPEND DEFSRC
    rna_hair.c
    rna_pointcloud.c
  )
endif()

if (WITH_NEW_SIMULATION_TYPE)
  list(APPEND DEFSRC
    rna_simulation.c
  )
endif()


set(APISRC
  rna_action_api.c
  rna_animation_api.c
  rna_armature_api.c
  rna_camera_api.c
  rna_curve_api.c
  rna_fcurve_api.c
  rna_image_api.c
  rna_lattice_api.c
  rna_main_api.c
  rna_material_api.c
  rna_mesh_api.c
  rna_meta_api.c
  rna_object_api.c
  rna_pose_api.c
  rna_scene_api.c
  rna_sequencer_api.c
  rna_sound_api.c
  rna_space_api.c
  rna_text_api.c
  rna_texture_api.c
  rna_ui_api.c
  rna_vfont_api.c
  rna_wm_api.c
  rna_wm_gizmo_api.c
  rna_workspace_api.c
)

string(REGEX REPLACE "rna_([a-zA-Z0-9_-]*).c" "${CMAKE_CURRENT_BINARY_DIR}/rna_\\1_gen.c" GENSRC "${DEFSRC}")
list(APPEND GENSRC
  "${CMAKE_CURRENT_BINARY_DIR}/rna_prototypes_gen.h"
)
set_source_files_properties(${GENSRC} PROPERTIES GENERATED TRUE)

# --------------------------
# CFLAGS for Generated Files
#
# less strict flags for generated source
set(GENSRC_CFLAGS)
if(CMAKE_COMPILER_IS_GNUCC OR (CMAKE_C_COMPILER_ID MATCHES "Clang"))
  set(GENSRC_CFLAGS "-Wno-missing-prototypes")
endif()
if(CMAKE_C_COMPILER_ID MATCHES "Clang")
  set(GENSRC_CFLAGS "${GENSRC_CFLAGS} -Wno-missing-variable-declarations")
endif()

if(GENSRC_CFLAGS)
  set_source_files_properties(${GENSRC} PROPERTIES COMPILE_FLAGS "${GENSRC_CFLAGS}")
endif()
unset(GENSRC_CFLAGS)


set(SRC_RNA_INC
  ../RNA_access.h
  ../RNA_define.h
  ../RNA_documentation.h
  ../RNA_enum_types.h
  ../RNA_types.h
)

set(SRC
  makesrna.c
  rna_define.c
  ${DEFSRC}
  ${APISRC}
  ../../../../intern/clog/clog.c
  ../../../../intern/guardedalloc/intern/mallocn.c
  ../../../../intern/guardedalloc/intern/mallocn_guarded_impl.c
  ../../../../intern/guardedalloc/intern/mallocn_lockfree_impl.c
  ../../../../intern/guardedalloc/intern/mmap_win.c

  # Needed for defaults.
  ../../../../release/datafiles/userdef/userdef_default.c
  ../../../../release/datafiles/userdef/userdef_default_theme.c
)

set(INC
  ../../../../intern/clog

  # Needed for defaults forward declarations.
  ../../blenloader

  ${CMAKE_BINARY_DIR}/source/blender/makesdna/intern
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

if(WITH_OPENIMAGEIO)
  add_definitions(-DWITH_OPENIMAGEIO)
endif()

if(WITH_IMAGE_TIFF)
  add_definitions(-DWITH_TIFF)
endif()

if(WITH_IMAGE_OPENJPEG)
  add_definitions(-DWITH_OPENJPEG)
endif()

if(WITH_IMAGE_DDS)
  add_definitions(-DWITH_DDS)
endif()

if(WITH_IMAGE_CINEON)
  add_definitions(-DWITH_CINEON)
endif()

if(WITH_IMAGE_HDR)
  add_definitions(-DWITH_HDR)
endif()

if(WITH_AUDASPACE)
  add_definitions(-DWITH_AUDASPACE)

  list(APPEND INC_SYS
    ${AUDASPACE_C_INCLUDE_DIRS}
  )
  list(APPEND LIB
    ${AUDASPACE_C_LIBRARIES}
    ${AUDASPACE_PY_LIBRARIES}
  )
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

if(WITH_JACK)
  add_definitions(-DWITH_JACK)
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

if(WITH_NEW_OBJECT_TYPES)
  add_definitions(-DWITH_NEW_OBJECT_TYPES)
endif()

if (WITH_NEW_SIMULATION_TYPE)
  add_definitions(-DWITH_NEW_SIMULATION_TYPE)
endif()


# Build makesrna executable
blender_include_dirs(
  .
  ..
  ../../blenfont
  ../../blenkernel
  ../../blenlib
  ../../blentranslation
  ../../bmesh
  ../../depsgraph
  ../../draw
  ../../gpu
  ../../ikplugin
  ../../imbuf
  ../../makesdna
  ../../nodes/
  ../../physics
  ../../windowmanager
  ../../editors/include
  ../../render/extern/include
  ../../../../intern/cycles/blender
  ../../../../intern/atomic
  ../../../../intern/glew-mx
  ../../../../intern/guardedalloc
  ../../../../intern/memutil
  ../../../../intern/mantaflow/extern
)

blender_include_dirs_sys(
  "${GLEW_INCLUDE_PATH}"
)

add_cc_flags_custom_test(makesrna)
setup_platform_linker_flags()

add_executable(makesrna ${SRC} ${SRC_RNA_INC} ${SRC_DNA_INC})

target_link_libraries(makesrna bf_dna)
target_link_libraries(makesrna bf_dna_blenlib)

if(WIN32 AND NOT UNIX)
  target_link_libraries(makesrna ${PTHREADS_LIBRARIES})
endif()

# Output rna_*_gen.c
# note (linux only): with crashes try add this after COMMAND: valgrind --leak-check=full --track-origins=yes
add_custom_command(
  OUTPUT ${GENSRC}
  COMMAND "$<TARGET_FILE:makesrna>" ${CMAKE_CURRENT_BINARY_DIR}/
  DEPENDS makesrna
)

# Build bf_rna
set(SRC
  rna_access.c
  rna_access_compare_override.c
  ${GENSRC}

  ${SRC_RNA_INC}
  rna_access_internal.h
  rna_internal.h
  rna_internal_types.h
  rna_mesh_utils.h
)

set(LIB
  bf_dna
  bf_editor_space_api

  bf_editor_animation
  bf_editor_armature
  bf_editor_curve
  bf_editor_gizmo_library
  bf_editor_gpencil
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

add_definitions(${GL_DEFINITIONS})

blender_add_lib(bf_rna "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

# Needed so we can use dna_type_offsets.h for defaults initialization.
add_dependencies(bf_blenkernel bf_dna)
