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

set(INC
  .
  ../blenfont
  ../blenlib
  ../blenloader
  ../blentranslation
  ../bmesh
  ../depsgraph
  ../draw
  ../functions
  ../gpencil_modifiers
  ../gpu
  ../ikplugin
  ../imbuf
  ../makesdna
  ../makesrna
  ../modifiers
  ../nodes
  ../render
  ../sequencer
  ../shader_fx
  ../simulation
  ../../../intern/eigen
  ../../../intern/ghost
  ../../../intern/glew-mx
  ../../../intern/guardedalloc
  ../../../intern/iksolver/extern
  ../../../intern/atomic
  ../../../intern/clog
  ../../../intern/libmv
  ../../../intern/mantaflow/extern
  ../../../intern/memutil
  ../../../intern/mikktspace
  ../../../intern/opensubdiv
  ../../../extern/curve_fit_nd

  # dna_type_offsets.h
  ${CMAKE_CURRENT_BINARY_DIR}/../makesdna/intern
)

set(INC_SYS
  ${ZLIB_INCLUDE_DIRS}
)

set(SRC
  ${CMAKE_SOURCE_DIR}/release/datafiles/userdef/userdef_default.c
  intern/CCGSubSurf.c
  intern/CCGSubSurf_legacy.c
  intern/CCGSubSurf_util.c
  intern/DerivedMesh.cc
  intern/action.c
  intern/addon.c
  intern/anim_data.c
  intern/anim_path.c
  intern/anim_sys.c
  intern/anim_visualization.c
  intern/appdir.c
  intern/armature.c
  intern/armature_deform.c
  intern/armature_pose.cc
  intern/armature_update.c
  intern/asset.cc
  intern/attribute.c
  intern/attribute_access.cc
  intern/attribute_math.cc
  intern/autoexec.c
  intern/blender.c
  intern/blender_copybuffer.c
  intern/blender_undo.c
  intern/blender_user_menu.c
  intern/blendfile.c
  intern/boids.c
  intern/bpath.c
  intern/brush.c
  intern/bvhutils.c
  intern/cachefile.c
  intern/callbacks.c
  intern/camera.c
  intern/cdderivedmesh.c
  intern/cloth.c
  intern/collection.c
  intern/collision.c
  intern/colorband.c
  intern/colortools.c
  intern/constraint.c
  intern/context.c
  intern/crazyspace.c
  intern/cryptomatte.cc
  intern/curve.c
  intern/curve_bevel.c
  intern/curve_convert.c
  intern/curve_decimate.c
  intern/curve_deform.c
  intern/curveprofile.c
  intern/customdata.c
  intern/customdata_file.c
  intern/data_transfer.c
  intern/deform.c
  intern/displist.c
  intern/displist_tangent.c
  intern/dynamicpaint.c
  intern/editlattice.c
  intern/editmesh.c
  intern/editmesh_bvh.c
  intern/editmesh_cache.c
  intern/editmesh_tangent.c
  intern/effect.c
  intern/fcurve.c
  intern/fcurve_driver.c
  intern/fluid.c
  intern/fmodifier.c
  intern/font.c
  intern/freestyle.c
  intern/geometry_component_instances.cc
  intern/geometry_component_mesh.cc
  intern/geometry_component_pointcloud.cc
  intern/geometry_component_volume.cc
  intern/geometry_set.cc
  intern/geometry_set_instances.cc
  intern/gpencil.c
  intern/gpencil_curve.c
  intern/gpencil_geom.c
  intern/gpencil_modifier.c
  intern/hair.c
  intern/icons.cc
  intern/icons_rasterize.c
  intern/idprop.c
  intern/idprop_utils.c
  intern/idtype.c
  intern/image.c
  intern/image_gen.c
  intern/image_gpu.c
  intern/image_save.c
  intern/ipo.c
  intern/kelvinlet.c
  intern/key.c
  intern/keyconfig.c
  intern/lattice.c
  intern/lattice_deform.c
  intern/layer.c
  intern/layer_utils.c
  intern/lib_id.c
  intern/lib_id_delete.c
  intern/lib_override.c
  intern/lib_query.c
  intern/lib_remap.c
  intern/library.c
  intern/light.c
  intern/lightprobe.c
  intern/linestyle.c
  intern/main.c
  intern/main_idmap.c
  intern/mask.c
  intern/mask_evaluate.c
  intern/mask_rasterize.c
  intern/material.c
  intern/mball.c
  intern/mball_tessellate.c
  intern/mesh.c
  intern/mesh_boolean_convert.cc
  intern/mesh_convert.c
  intern/mesh_evaluate.c
  intern/mesh_fair.cc
  intern/mesh_iterators.c
  intern/mesh_mapping.c
  intern/mesh_merge.c
  intern/mesh_mirror.c
  intern/mesh_remap.c
  intern/mesh_remesh_voxel.c
  intern/mesh_runtime.c
  intern/mesh_tangent.c
  intern/mesh_validate.c
  intern/mesh_validate.cc
  intern/mesh_wrapper.c
  intern/modifier.c
  intern/movieclip.c
  intern/multires.c
  intern/multires_reshape.c
  intern/multires_reshape_apply_base.c
  intern/multires_reshape_ccg.c
  intern/multires_reshape_smooth.c
  intern/multires_reshape_subdivide.c
  intern/multires_reshape_util.c
  intern/multires_reshape_vertcos.c
  intern/multires_subdiv.c
  intern/multires_unsubdivide.c
  intern/multires_versioning.c
  intern/nla.c
  intern/node.cc
  intern/node_ui_storage.cc
  intern/object.c
  intern/object_deform.c
  intern/object_dupli.c
  intern/object_facemap.c
  intern/object_update.c
  intern/ocean.c
  intern/ocean_spectrum.c
  intern/outliner_treehash.c
  intern/packedFile.c
  intern/paint.c
  intern/paint_toolslots.c
  intern/particle.c
  intern/particle_child.c
  intern/particle_distribute.c
  intern/particle_system.c
  intern/pbvh.c
  intern/pbvh_bmesh.c
  intern/pointcache.c
  intern/pointcloud.cc
  intern/preferences.c
  intern/report.c
  intern/rigidbody.c
  intern/scene.c
  intern/screen.c
  intern/shader_fx.c
  intern/shrinkwrap.c
  intern/simulation.cc
  intern/softbody.c
  intern/sound.c
  intern/speaker.c
  intern/studiolight.c
  intern/subdiv.c
  intern/subdiv_ccg.c
  intern/subdiv_ccg_mask.c
  intern/subdiv_ccg_material.c
  intern/subdiv_converter.c
  intern/subdiv_converter_mesh.c
  intern/subdiv_deform.c
  intern/subdiv_displacement.c
  intern/subdiv_displacement_multires.c
  intern/subdiv_eval.c
  intern/subdiv_foreach.c
  intern/subdiv_mesh.c
  intern/subdiv_stats.c
  intern/subdiv_topology.c
  intern/subsurf_ccg.c
  intern/text.c
  intern/text_suggestions.c
  intern/texture.c
  intern/tracking.c
  intern/tracking_auto.c
  intern/tracking_detect.c
  intern/tracking_plane_tracker.c
  intern/tracking_region_tracker.c
  intern/tracking_solver.c
  intern/tracking_stabilize.c
  intern/tracking_util.c
  intern/undo_system.c
  intern/unit.c
  intern/volume.cc
  intern/volume_render.cc
  intern/volume_to_mesh.cc
  intern/workspace.c
  intern/world.c
  intern/writeavi.c

  BKE_DerivedMesh.h
  BKE_action.h
  BKE_addon.h
  BKE_anim_data.h
  BKE_anim_path.h
  BKE_anim_visualization.h
  BKE_animsys.h
  BKE_appdir.h
  BKE_armature.h
  BKE_asset.h
  BKE_attribute.h
  BKE_attribute_access.hh
  BKE_attribute_math.hh
  BKE_autoexec.h
  BKE_blender.h
  BKE_blender_copybuffer.h
  BKE_blender_undo.h
  BKE_blender_user_menu.h
  BKE_blender_version.h
  BKE_blendfile.h
  BKE_boids.h
  BKE_bpath.h
  BKE_brush.h
  BKE_bvhutils.h
  BKE_cachefile.h
  BKE_callbacks.h
  BKE_camera.h
  BKE_ccg.h
  BKE_cdderivedmesh.h
  BKE_cloth.h
  BKE_collection.h
  BKE_collision.h
  BKE_colorband.h
  BKE_colortools.h
  BKE_constraint.h
  BKE_context.h
  BKE_crazyspace.h
  BKE_cryptomatte.h
  BKE_cryptomatte.hh
  BKE_curve.h
  BKE_curveprofile.h
  BKE_customdata.h
  BKE_customdata_file.h
  BKE_data_transfer.h
  BKE_deform.h
  BKE_displist.h
  BKE_displist_tangent.h
  BKE_duplilist.h
  BKE_dynamicpaint.h
  BKE_editlattice.h
  BKE_editmesh.h
  BKE_editmesh_bvh.h
  BKE_editmesh_cache.h
  BKE_editmesh_tangent.h
  BKE_effect.h
  BKE_fcurve.h
  BKE_fcurve_driver.h
  BKE_fluid.h
  BKE_font.h
  BKE_freestyle.h
  BKE_geometry_set.h
  BKE_geometry_set.hh
  BKE_geometry_set_instances.hh
  BKE_global.h
  BKE_gpencil.h
  BKE_gpencil_curve.h
  BKE_gpencil_geom.h
  BKE_gpencil_modifier.h
  BKE_hair.h
  BKE_icons.h
  BKE_idprop.h
  BKE_idtype.h
  BKE_image.h
  BKE_image_save.h
  BKE_ipo.h
  BKE_kelvinlet.h
  BKE_key.h
  BKE_keyconfig.h
  BKE_lattice.h
  BKE_layer.h
  BKE_lib_id.h
  BKE_lib_override.h
  BKE_lib_query.h
  BKE_lib_remap.h
  BKE_library.h
  BKE_light.h
  BKE_lightprobe.h
  BKE_linestyle.h
  BKE_main.h
  BKE_main_idmap.h
  BKE_mask.h
  BKE_material.h
  BKE_mball.h
  BKE_mball_tessellate.h
  BKE_mesh.h
  BKE_mesh_boolean_convert.h
  BKE_mesh_fair.h
  BKE_mesh_iterators.h
  BKE_mesh_mapping.h
  BKE_mesh_mirror.h
  BKE_mesh_remap.h
  BKE_mesh_remesh_voxel.h
  BKE_mesh_runtime.h
  BKE_mesh_tangent.h
  BKE_mesh_types.h
  BKE_mesh_wrapper.h
  BKE_modifier.h
  BKE_movieclip.h
  BKE_multires.h
  BKE_nla.h
  BKE_node.h
  BKE_object.h
  BKE_object_deform.h
  BKE_object_facemap.h
  BKE_ocean.h
  BKE_outliner_treehash.h
  BKE_packedFile.h
  BKE_paint.h
  BKE_particle.h
  BKE_pbvh.h
  BKE_persistent_data_handle.hh
  BKE_pointcache.h
  BKE_pointcloud.h
  BKE_preferences.h
  BKE_report.h
  BKE_rigidbody.h
  BKE_scene.h
  BKE_screen.h
  BKE_sequencer_offscreen.h
  BKE_shader_fx.h
  BKE_shrinkwrap.h
  BKE_simulation.h
  BKE_softbody.h
  BKE_sound.h
  BKE_speaker.h
  BKE_studiolight.h
  BKE_subdiv.h
  BKE_subdiv_ccg.h
  BKE_subdiv_deform.h
  BKE_subdiv_eval.h
  BKE_subdiv_foreach.h
  BKE_subdiv_mesh.h
  BKE_subdiv_topology.h
  BKE_subsurf.h
  BKE_text.h
  BKE_text_suggestions.h
  BKE_texture.h
  BKE_tracking.h
  BKE_undo_system.h
  BKE_unit.h
  BKE_volume.h
  BKE_volume_render.h
  BKE_volume_to_mesh.hh
  BKE_workspace.h
  BKE_world.h
  BKE_writeavi.h

  nla_private.h
  particle_private.h
  tracking_private.h
  intern/attribute_access_intern.hh
  intern/CCGSubSurf.h
  intern/CCGSubSurf_inline.h
  intern/CCGSubSurf_intern.h
  intern/data_transfer_intern.h
  intern/lib_intern.h
  intern/multires_inline.h
  intern/multires_reshape.h
  intern/multires_unsubdivide.h
  intern/ocean_intern.h
  intern/pbvh_intern.h
  intern/subdiv_converter.h
  intern/subdiv_inline.h
)

set(LIB
  bf_blenfont
  bf_blenlib
  bf_blenloader
  bf_blentranslation
  bf_bmesh
  bf_depsgraph
  bf_draw
  bf_functions
  bf_gpencil_modifiers
  bf_gpu
  bf_ikplugin
  bf_imbuf
  bf_intern_clog
  bf_intern_ghost
  bf_intern_guardedalloc
  bf_intern_libmv  # Uses stub when disabled.
  bf_intern_mikktspace
  bf_intern_opensubdiv  # Uses stub when disabled.
  bf_modifiers
  bf_nodes
  bf_rna
  bf_shader_fx
  bf_simulation
)

if(WITH_BINRELOC)
  list(APPEND INC_SYS
    ${BINRELOC_INCLUDE_DIRS}
  )
  list(APPEND LIB
    extern_binreloc
  )
  add_definitions(-DWITH_BINRELOC)
endif()


if(WIN32)
  list(APPEND INC
    ../../../intern/utfconv
  )
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

if(WITH_BULLET)
  list(APPEND INC_SYS
    ${BULLET_INCLUDE_DIRS}
  )
  list(APPEND INC
    ../../../intern/rigidbody
  )

  if(NOT WITH_SYSTEM_BULLET)
    list(APPEND LIB
      extern_bullet
    )
  endif()

  list(APPEND LIB
    bf_intern_rigidbody

    ${BULLET_LIBRARIES}
  )
  add_definitions(-DWITH_BULLET)
endif()

if(WITH_IMAGE_OPENEXR)
  add_definitions(-DWITH_OPENEXR)
endif()

if(WITH_IMAGE_TIFF)
  add_definitions(-DWITH_TIFF)
endif()

if(WITH_OPENIMAGEIO)
  add_definitions(-DWITH_OPENIMAGEIO)
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

if(WITH_IMAGE_FRAMESERVER)
  add_definitions(-DWITH_FRAMESERVER)
endif()

if(WITH_IMAGE_HDR)
  add_definitions(-DWITH_HDR)
endif()

if(WITH_CODEC_AVI)
  list(APPEND INC
    ../io/avi
  )
  add_definitions(-DWITH_AVI)
endif()

if(WITH_CODEC_FFMPEG)
  list(APPEND SRC
    intern/writeffmpeg.c
    BKE_writeffmpeg.h
  )
  list(APPEND INC
    ../../../intern/ffmpeg
  )
  list(APPEND INC_SYS
    ${FFMPEG_INCLUDE_DIRS}
  )
  list(APPEND LIB
    ${FFMPEG_LIBRARIES}
  )
  add_definitions(-DWITH_FFMPEG)

  remove_strict_c_flags_file(
    intern/writeffmpeg.c
  )
endif()

if(WITH_PYTHON)
  list(APPEND INC
    ../python
  )
  list(APPEND LIB
    bf_python
    bf_python_bmesh
  )
  add_definitions(-DWITH_PYTHON)

  if(WITH_PYTHON_SAFETY)
    add_definitions(-DWITH_PYTHON_SAFETY)
  endif()

  if(WITH_PYTHON_SECURITY)
    add_definitions(-DWITH_PYTHON_SECURITY)
  endif()


  if(PYTHON_EXECUTABLE)
    get_filename_component(_python_exe_name ${PYTHON_EXECUTABLE} NAME)
    add_definitions(-DPYTHON_EXECUTABLE_NAME=${_python_exe_name})
    unset(_python_exe_name)
  endif()
endif()

if(WITH_MOD_FLUID)
  list(APPEND LIB
    bf_intern_mantaflow
  )
  add_definitions(-DWITH_FLUID)
endif()

if(WITH_MOD_OCEANSIM)
  add_definitions(-DWITH_OCEANSIM)
endif()

if(WITH_JACK)
  add_definitions(-DWITH_JACK)
endif()

if(WITH_LZO)
  if(WITH_SYSTEM_LZO)
    list(APPEND INC_SYS
      ${LZO_INCLUDE_DIR}
    )
    list(APPEND LIB
      ${LZO_LIBRARIES}
    )
    add_definitions(-DWITH_SYSTEM_LZO)
  else()
    list(APPEND INC_SYS
      ../../../extern/lzo/minilzo
    )
    list(APPEND LIB
      extern_minilzo
    )
  endif()
  add_definitions(-DWITH_LZO)
endif()

if(WITH_LZMA)
  list(APPEND INC_SYS
    ../../../extern/lzma
  )
  list(APPEND LIB
    extern_lzma
  )
  add_definitions(-DWITH_LZMA)
endif()

if(WITH_LIBMV)
  add_definitions(-DWITH_LIBMV)
endif()

if(WITH_FFTW3)
  list(APPEND INC_SYS
    ${FFTW3_INCLUDE_DIRS}
  )
  list(APPEND LIB
    ${FFTW3_LIBRARIES}
  )
  add_definitions(-DFFTW3=1)
endif()

if(WITH_INTERNATIONAL)
  add_definitions(-DWITH_INTERNATIONAL)
endif()

if(WITH_FREESTYLE)
  add_definitions(-DWITH_FREESTYLE)
endif()

if(WITH_ALEMBIC)
  list(APPEND INC
    ../io/alembic
  )
  add_definitions(-DWITH_ALEMBIC)
endif()

if(WITH_OPENSUBDIV)
  list(APPEND INC_SYS
    ${OPENSUBDIV_INCLUDE_DIRS}
  )
  list(APPEND LIB
    ${OPENSUBDIV_LIBRARIES}
  )
  add_definitions(-DWITH_OPENSUBDIV)
endif()

if(WITH_OPENVDB)
  list(APPEND INC
    ../../../intern/openvdb
  )
  list(APPEND INC_SYS
    ${OPENVDB_INCLUDE_DIRS}
  )
  list(APPEND LIB
    bf_intern_openvdb
    ${OPENVDB_LIBRARIES}
  )
  add_definitions(-DWITH_OPENVDB ${OPENVDB_DEFINITIONS})
endif()

if(WITH_QUADRIFLOW)
  list(APPEND INC
    ../../../intern/quadriflow
  )
  list(APPEND LIB
    bf_intern_quadriflow
  )
  add_definitions(-DWITH_QUADRIFLOW)
endif()

if(WITH_XR_OPENXR)
  add_definitions(-DWITH_XR_OPENXR)
endif()

if(WITH_TBB)
  add_definitions(-DWITH_TBB)

  list(APPEND INC_SYS
    ${TBB_INCLUDE_DIRS}
  )

  list(APPEND LIB
    ${TBB_LIBRARIES}
  )
endif()

if(WITH_GMP)
  add_definitions(-DWITH_GMP)

  list(APPEND INC_SYS
    ${GMP_INCLUDE_DIRS}
  )
 endif()

# # Warnings as errors, this is too strict!
# if(MSVC)
#    string(APPEND CMAKE_C_FLAGS " /WX")
# endif()

blender_add_lib(bf_blenkernel "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

# Needed so we can use dna_type_offsets.h for defaults initialization.
add_dependencies(bf_blenkernel bf_dna)


if(WITH_GTESTS)
  set(TEST_SRC
    intern/armature_test.cc
    intern/cryptomatte_test.cc
    intern/fcurve_test.cc
    intern/lattice_deform_test.cc
    intern/layer_test.cc
    intern/tracking_test.cc
  )
  set(TEST_INC
    ../editors/include
  )
  include(GTestTesting)
  blender_add_test_lib(bf_blenkernel_tests "${TEST_SRC}" "${INC};${TEST_INC}" "${INC_SYS}" "${LIB}")
endif()
