# SPDX-FileCopyrightText: 2006 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  ../asset_system
  ../blenfont
  ../blenlib
  ../blenloader
  ../blentranslation
  ../bmesh
  ../depsgraph
  ../draw
  ../functions
  ../gpencil_modifiers_legacy
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
  ../../../intern/guardedalloc
  ../../../intern/iksolver/extern
  ../../../intern/clog
  ../../../intern/libmv
  ../../../intern/mantaflow/extern
  ../../../intern/memutil
  ../../../intern/mikktspace
  ../../../intern/opensubdiv
  ../../../extern/curve_fit_nd
  ../../../extern/fmtlib/include

  # dna_type_offsets.h
  ${CMAKE_CURRENT_BINARY_DIR}/../makesdna/intern
  # RNA_prototypes.h
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS
  ${ZLIB_INCLUDE_DIRS}

  # For `vfontdata_freetype.cc`.
  ${FREETYPE_INCLUDE_DIRS}
)

set(SRC
  ${CMAKE_SOURCE_DIR}/release/datafiles/userdef/userdef_default.c
  intern/CCGSubSurf.c
  intern/CCGSubSurf_legacy.c
  intern/CCGSubSurf_util.c
  intern/DerivedMesh.cc
  intern/action.c
  intern/action_bones.cc
  intern/action_mirror.c
  intern/addon.c
  intern/anim_data.c
  intern/anim_path.c
  intern/anim_sys.c
  intern/anim_visualization.c
  intern/anonymous_attribute_id.cc
  intern/appdir.c
  intern/armature.c
  intern/armature_deform.c
  intern/armature_pose.cc
  intern/armature_selection.cc
  intern/armature_update.c
  intern/asset.cc
  intern/asset_weak_reference.cc
  intern/attribute.cc
  intern/attribute_access.cc
  intern/attribute_math.cc
  intern/autoexec.c
  intern/blender.c
  intern/blender_copybuffer.c
  intern/blender_undo.cc
  intern/blender_user_menu.c
  intern/blendfile.cc
  intern/blendfile_link_append.c
  intern/boids.c
  intern/bpath.c
  intern/brush.cc
  intern/bvhutils.cc
  intern/cachefile.c
  intern/callbacks.c
  intern/camera.c
  intern/cdderivedmesh.cc
  intern/cloth.cc
  intern/collection.c
  intern/collision.cc
  intern/colorband.c
  intern/colortools.c
  intern/compute_contexts.cc
  intern/constraint.c
  intern/context.cc
  intern/cpp_types.cc
  intern/crazyspace.cc
  intern/cryptomatte.cc
  intern/curve.cc
  intern/curve_bevel.c
  intern/curve_bezier.cc
  intern/curve_catmull_rom.cc
  intern/curve_convert.c
  intern/curve_decimate.c
  intern/curve_deform.c
  intern/curve_legacy_convert.cc
  intern/curve_nurbs.cc
  intern/curve_poly.cc
  intern/curve_to_mesh_convert.cc
  intern/curveprofile.cc
  intern/curves.cc
  intern/curves_geometry.cc
  intern/curves_utils.cc
  intern/customdata.cc
  intern/customdata_file.c
  intern/data_transfer.cc
  intern/deform.cc
  intern/displist.cc
  intern/dynamicpaint.cc
  intern/editlattice.c
  intern/editmesh.cc
  intern/editmesh_bvh.c
  intern/editmesh_cache.cc
  intern/editmesh_tangent.cc
  intern/effect.c
  intern/fcurve.c
  intern/fcurve_cache.c
  intern/fcurve_driver.c
  intern/fluid.cc
  intern/fmodifier.c
  intern/freestyle.c
  intern/geometry_component_curves.cc
  intern/geometry_component_edit_data.cc
  intern/geometry_component_instances.cc
  intern/geometry_component_mesh.cc
  intern/geometry_component_pointcloud.cc
  intern/geometry_component_volume.cc
  intern/geometry_fields.cc
  intern/geometry_set.cc
  intern/geometry_set_instances.cc
  intern/gpencil_curve_legacy.c
  intern/gpencil_geom_legacy.cc
  intern/gpencil_legacy.c
  intern/gpencil_modifier_legacy.c
  intern/gpencil_update_cache_legacy.c
  intern/grease_pencil.cc
  intern/grease_pencil_convert_legacy.cc
  intern/icons.cc
  intern/icons_rasterize.c
  intern/idprop.c
  intern/idprop_create.cc
  intern/idprop_serialize.cc
  intern/idprop_utils.c
  intern/idtype.c
  intern/image.cc
  intern/image_format.cc
  intern/image_gen.c
  intern/image_gpu.cc
  intern/image_partial_update.cc
  intern/image_save.cc
  intern/instances.cc
  intern/ipo.c
  intern/kelvinlet.c
  intern/key.cc
  intern/keyconfig.c
  intern/lattice.c
  intern/lattice_deform.c
  intern/layer.cc
  intern/layer_utils.c
  intern/lib_id.c
  intern/lib_id_delete.c
  intern/lib_id_eval.c
  intern/lib_id_remapper.cc
  intern/lib_override.cc
  intern/lib_override_proxy_conversion.c
  intern/lib_query.c
  intern/lib_remap.c
  intern/library.c
  intern/light.c
  intern/light_linking.cc
  intern/lightprobe.cc
  intern/linestyle.cc
  intern/main.c
  intern/main_idmap.c
  intern/main_namemap.cc
  intern/mask.cc
  intern/mask_evaluate.cc
  intern/mask_rasterize.cc
  intern/material.cc
  intern/mball.cc
  intern/mball_tessellate.cc
  intern/mesh.cc
  intern/mesh_boolean_convert.cc
  intern/mesh_calc_edges.cc
  intern/mesh_convert.cc
  intern/mesh_debug.cc
  intern/mesh_evaluate.cc
  intern/mesh_fair.cc
  intern/mesh_iterators.cc
  intern/mesh_legacy_convert.cc
  intern/mesh_mapping.cc
  intern/mesh_merge_customdata.cc
  intern/mesh_mirror.cc
  intern/mesh_normals.cc
  intern/mesh_remap.cc
  intern/mesh_remesh_voxel.cc
  intern/mesh_runtime.cc
  intern/mesh_sample.cc
  intern/mesh_tangent.cc
  intern/mesh_tessellate.cc
  intern/mesh_validate.cc
  intern/mesh_wrapper.cc
  intern/modifier.cc
  intern/movieclip.c
  intern/multires.cc
  intern/multires_reshape.cc
  intern/multires_reshape_apply_base.cc
  intern/multires_reshape_ccg.cc
  intern/multires_reshape_smooth.cc
  intern/multires_reshape_subdivide.cc
  intern/multires_reshape_util.cc
  intern/multires_reshape_vertcos.cc
  intern/multires_subdiv.cc
  intern/multires_unsubdivide.cc
  intern/multires_versioning.cc
  intern/nla.c
  intern/node.cc
  intern/node_runtime.cc
  intern/node_tree_anonymous_attributes.cc
  intern/node_tree_dot_export.cc
  intern/node_tree_field_inferencing.cc
  intern/node_tree_update.cc
  intern/node_tree_zones.cc
  intern/object.cc
  intern/object_deform.c
  intern/object_dupli.cc
  intern/object_update.cc
  intern/ocean.c
  intern/ocean_spectrum.c
  intern/outliner_treehash.cc
  intern/packedFile.c
  intern/paint.cc
  intern/paint_canvas.cc
  intern/paint_toolslots.c
  intern/particle.cc
  intern/particle_child.c
  intern/particle_distribute.c
  intern/particle_system.c
  intern/pbvh.cc
  intern/pbvh_bmesh.cc
  intern/pbvh_colors.cc
  intern/pbvh_pixels.cc
  intern/pbvh_pixels_copy.cc
  intern/pbvh_uv_islands.cc
  intern/pointcache.c
  intern/pointcloud.cc
  intern/pose_backup.cc
  intern/preferences.c
  intern/report.c
  intern/rigidbody.c
  intern/scene.cc
  intern/screen.c
  intern/shader_fx.c
  intern/shrinkwrap.cc
  intern/simulation.cc
  intern/simulation_state.cc
  intern/simulation_state_serialize.cc
  intern/softbody.c
  intern/sound.c
  intern/speaker.c
  intern/studiolight.c
  intern/subdiv.cc
  intern/subdiv_ccg.cc
  intern/subdiv_ccg_mask.cc
  intern/subdiv_ccg_material.cc
  intern/subdiv_converter.cc
  intern/subdiv_converter_mesh.cc
  intern/subdiv_deform.cc
  intern/subdiv_displacement.cc
  intern/subdiv_displacement_multires.cc
  intern/subdiv_eval.cc
  intern/subdiv_foreach.cc
  intern/subdiv_mesh.cc
  intern/subdiv_modifier.cc
  intern/subdiv_stats.cc
  intern/subdiv_topology.cc
  intern/subsurf_ccg.cc
  intern/text.c
  intern/text_suggestions.c
  intern/texture.cc
  intern/tracking.cc
  intern/tracking_auto.cc
  intern/tracking_detect.cc
  intern/tracking_plane_tracker.cc
  intern/tracking_region_tracker.cc
  intern/tracking_solver.cc
  intern/tracking_stabilize.cc
  intern/tracking_util.cc
  intern/type_conversions.cc
  intern/undo_system.cc
  intern/unit.c
  intern/vfont.cc
  intern/vfontdata_freetype.cc
  intern/viewer_path.cc
  intern/volume.cc
  intern/volume_render.cc
  intern/volume_to_mesh.cc
  intern/workspace.cc
  intern/world.c
  intern/writeavi.c

  BKE_DerivedMesh.h
  BKE_action.h
  BKE_action.hh
  BKE_addon.h
  BKE_anim_data.h
  BKE_anim_path.h
  BKE_anim_visualization.h
  BKE_animsys.h
  BKE_anonymous_attribute_id.hh
  BKE_appdir.h
  BKE_armature.h
  BKE_armature.hh
  BKE_asset.h
  BKE_attribute.h
  BKE_attribute.hh
  BKE_attribute_math.hh
  BKE_autoexec.h
  BKE_blender.h
  BKE_blender_copybuffer.h
  BKE_blender_undo.h
  BKE_blender_user_menu.h
  BKE_blender_version.h
  BKE_blendfile.h
  BKE_blendfile_link_append.h
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
  BKE_compute_contexts.hh
  BKE_constraint.h
  BKE_context.h
  BKE_cpp_types.h
  BKE_crazyspace.h
  BKE_crazyspace.hh
  BKE_cryptomatte.h
  BKE_cryptomatte.hh
  BKE_curve.h
  BKE_curve_legacy_convert.hh
  BKE_curve_to_mesh.hh
  BKE_curveprofile.h
  BKE_curves.h
  BKE_curves.hh
  BKE_curves_utils.hh
  BKE_customdata.h
  BKE_customdata_file.h
  BKE_data_transfer.h
  BKE_deform.h
  BKE_displist.h
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
  BKE_freestyle.h
  BKE_geometry_fields.hh
  BKE_geometry_set.hh
  BKE_geometry_set_instances.hh
  BKE_global.h
  BKE_gpencil_curve_legacy.h
  BKE_gpencil_geom_legacy.h
  BKE_gpencil_legacy.h
  BKE_gpencil_modifier_legacy.h
  BKE_gpencil_update_cache_legacy.h
  BKE_grease_pencil.h
  BKE_grease_pencil.hh
  BKE_icons.h
  BKE_idprop.h
  BKE_idprop.hh
  BKE_idtype.h
  BKE_image.h
  BKE_image_format.h
  BKE_image_partial_update.hh
  BKE_image_save.h
  BKE_image_wrappers.hh
  BKE_instances.hh
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
  BKE_light_linking.h
  BKE_lightprobe.h
  BKE_linestyle.h
  BKE_main.h
  BKE_main_idmap.h
  BKE_main_namemap.h
  BKE_mask.h
  BKE_material.h
  BKE_mball.h
  BKE_mball_tessellate.h
  BKE_mesh.h
  BKE_mesh.hh
  BKE_mesh_boolean_convert.hh
  BKE_mesh_fair.h
  BKE_mesh_iterators.h
  BKE_mesh_legacy_convert.h
  BKE_mesh_mapping.h
  BKE_mesh_mirror.h
  BKE_mesh_remap.h
  BKE_mesh_remesh_voxel.h
  BKE_mesh_runtime.h
  BKE_mesh_sample.hh
  BKE_mesh_tangent.h
  BKE_mesh_types.h
  BKE_mesh_wrapper.h
  BKE_modifier.h
  BKE_movieclip.h
  BKE_multires.h
  BKE_nla.h
  BKE_node.h
  BKE_node.hh
  BKE_node_runtime.hh
  BKE_node_tree_anonymous_attributes.hh
  BKE_node_tree_dot_export.hh
  BKE_node_tree_update.h
  BKE_node_tree_zones.hh
  BKE_object.h
  BKE_object_deform.h
  BKE_ocean.h
  BKE_outliner_treehash.hh
  BKE_packedFile.h
  BKE_paint.h
  BKE_particle.h
  BKE_pbvh.h
  BKE_pbvh_api.hh
  BKE_pbvh_pixels.hh
  BKE_pointcache.h
  BKE_pointcloud.h
  BKE_pose_backup.h
  BKE_preferences.h
  BKE_report.h
  BKE_rigidbody.h
  BKE_scene.h
  BKE_screen.h
  BKE_sequencer_offscreen.h
  BKE_shader_fx.h
  BKE_shrinkwrap.h
  BKE_simulation.h
  BKE_simulation_state.hh
  BKE_simulation_state_serialize.hh
  BKE_softbody.h
  BKE_sound.h
  BKE_speaker.h
  BKE_studiolight.h
  BKE_subdiv.h
  BKE_subdiv_ccg.h
  BKE_subdiv_deform.h
  BKE_subdiv_eval.h
  BKE_subdiv_foreach.hh
  BKE_subdiv_mesh.hh
  BKE_subdiv_modifier.h
  BKE_subdiv_topology.h
  BKE_subsurf.h
  BKE_text.h
  BKE_text_suggestions.h
  BKE_texture.h
  BKE_tracking.h
  BKE_type_conversions.hh
  BKE_undo_system.h
  BKE_unit.h
  BKE_vfont.h
  BKE_vfontdata.h
  BKE_viewer_path.h
  BKE_volume.h
  BKE_volume_openvdb.hh
  BKE_volume_render.h
  BKE_volume_to_mesh.hh
  BKE_workspace.h
  BKE_world.h
  BKE_writeavi.h

  nla_private.h
  particle_private.h
  tracking_private.h

  intern/CCGSubSurf.h
  intern/CCGSubSurf_inline.h
  intern/CCGSubSurf_intern.h
  intern/attribute_access_intern.hh
  intern/data_transfer_intern.h
  intern/lib_intern.h
  intern/multires_inline.h
  intern/multires_reshape.hh
  intern/multires_unsubdivide.hh
  intern/ocean_intern.h
  intern/pbvh_intern.hh
  intern/pbvh_pixels_copy.hh
  intern/pbvh_uv_islands.hh
  intern/subdiv_converter.h
  intern/subdiv_inline.h
)

set(LIB
  bf_asset_system
  bf_blenfont
  bf_blenlib
  bf_blenloader
  bf_blentranslation
  bf_bmesh
  bf_depsgraph
  bf_draw
  bf_functions
  bf_gpencil_modifiers_legacy
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
  extern_fmtlib
  PRIVATE bf_intern_atomic
  # For `vfontdata_freetype.c`.
  ${FREETYPE_LIBRARIES} ${BROTLI_LIBRARIES}
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

if(WITH_BULLET)
  list(APPEND INC_SYS
    ${BULLET_INCLUDE_DIRS}
  )
  list(APPEND INC
    ../../../intern/rigidbody
  )
  list(APPEND LIB
    bf_intern_rigidbody

    ${BULLET_LIBRARIES}
  )
  add_definitions(-DWITH_BULLET)
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

  if(WITH_PYTHON_MODULE)
    add_definitions(-DWITH_PYTHON_MODULE)
  endif()

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

if(WITH_FREESTYLE)
  add_definitions(-DWITH_FREESTYLE)
endif()

if(WITH_ALEMBIC)
  list(APPEND INC
    ../io/alembic
  )
  add_definitions(-DWITH_ALEMBIC)
endif()

if(WITH_USD)
  list(APPEND INC
    ../io/usd
  )
  add_definitions(-DWITH_USD)
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
# RNA_prototypes.h
add_dependencies(bf_blenkernel bf_rna)


if(WITH_GTESTS)
  set(TEST_SRC
    intern/action_test.cc
    intern/armature_test.cc
    intern/asset_metadata_test.cc
    intern/bpath_test.cc
    intern/cryptomatte_test.cc
    intern/curves_geometry_test.cc
    intern/fcurve_test.cc
    intern/grease_pencil_test.cc
    intern/idprop_serialize_test.cc
    intern/image_partial_update_test.cc
    intern/image_test.cc
    intern/lattice_deform_test.cc
    intern/layer_test.cc
    intern/lib_id_remapper_test.cc
    intern/lib_id_test.cc
    intern/lib_remap_test.cc
    intern/nla_test.cc
    intern/tracking_test.cc
  )
  set(TEST_INC
    ../editors/include
  )
  include(GTestTesting)
  blender_add_test_lib(bf_blenkernel_tests "${TEST_SRC}" "${INC};${TEST_INC}" "${INC_SYS}" "${LIB}")

  # RNA_prototypes.h
  add_dependencies(bf_blenkernel_tests bf_rna)
endif()
