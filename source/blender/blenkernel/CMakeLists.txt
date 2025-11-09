# SPDX-FileCopyrightText: 2006 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  PUBLIC .
  ../ikplugin
  ../makesrna
  ../modifiers
  ../nodes/geometry/include
  ../shader_fx
  ../simulation
  ../windowmanager
  ../../../intern/eigen
  ../../../intern/ghost
  ../../../intern/iksolver/extern
  ../../../intern/libmv
  ../../../intern/mantaflow/extern
  ../../../intern/memutil
  ../../../intern/mikktspace
  ../../../intern/opensubdiv

  # RNA_prototypes.hh
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS
  ${ZLIB_INCLUDE_DIRS}
  ${ZSTD_INCLUDE_DIRS}

  # For `vfontdata_freetype.cc`.
  ${FREETYPE_INCLUDE_DIRS}
)

set(SRC
  ${CMAKE_SOURCE_DIR}/release/datafiles/userdef/userdef_default.c
  intern/action.cc
  intern/action_bones.cc
  intern/action_mirror.cc
  intern/addon.cc
  intern/anim_data.cc
  intern/anim_data_bmain_utils.cc
  intern/anim_path.cc
  intern/anim_sys.cc
  intern/anim_visualization.cc
  intern/anonymous_attribute_id.cc
  intern/appdir.cc
  intern/armature.cc
  intern/armature_deform.cc
  intern/armature_selection.cc
  intern/armature_update.cc
  intern/asset.cc
  intern/asset_edit.cc
  intern/asset_weak_reference.cc
  intern/attribute.cc
  intern/attribute_access.cc
  intern/attribute_legacy_convert.cc
  intern/attribute_math.cc
  intern/attribute_storage.cc
  intern/attribute_storage_access.cc
  intern/autoexec.cc
  intern/bake_data_block_map.cc
  intern/bake_geometry_nodes_modifier.cc
  intern/bake_geometry_nodes_modifier_pack.cc
  intern/bake_items.cc
  intern/bake_items_paths.cc
  intern/bake_items_serialize.cc
  intern/bake_items_socket.cc
  intern/blender.cc
  intern/blender_cli_command.cc
  intern/blender_copybuffer.cc
  intern/blender_undo.cc
  intern/blender_user_menu.cc
  intern/blendfile.cc
  intern/blendfile_link_append.cc
  intern/boids.cc
  intern/bpath.cc
  intern/brush.cc
  intern/bvhutils.cc
  intern/cachefile.cc
  intern/callbacks.cc
  intern/camera.cc
  intern/cloth.cc
  intern/collection.cc
  intern/collision.cc
  intern/colorband.cc
  intern/colortools.cc
  intern/compositor.cc
  intern/compute_contexts.cc
  intern/constraint.cc
  intern/context.cc
  intern/cpp_types.cc
  intern/crazyspace.cc
  intern/cryptomatte.cc
  intern/curve.cc
  intern/curve_bevel.cc
  intern/curve_bezier.cc
  intern/curve_catmull_rom.cc
  intern/curve_convert.cc
  intern/curve_decimate.cc
  intern/curve_deform.cc
  intern/curve_legacy_convert.cc
  intern/curve_nurbs.cc
  intern/curve_poly.cc
  intern/curve_to_mesh_convert.cc
  intern/curveprofile.cc
  intern/curves.cc
  intern/curves_attributes.cc
  intern/curves_geometry.cc
  intern/curves_utils.cc
  intern/customdata.cc
  intern/customdata_file.cc
  intern/data_transfer.cc
  intern/deform.cc
  intern/displist.cc
  intern/dynamicpaint.cc
  intern/editlattice.cc
  intern/editmesh.cc
  intern/editmesh_bvh.cc
  intern/editmesh_cache.cc
  intern/editmesh_tangent.cc
  intern/effect.cc
  intern/fcurve.cc
  intern/fcurve_cache.cc
  intern/fcurve_driver.cc
  intern/file_handler.cc
  intern/fluid.cc
  intern/fmodifier.cc
  intern/freestyle.cc
  intern/geometry_compare.cc
  intern/geometry_component_curves.cc
  intern/geometry_component_edit_data.cc
  intern/geometry_component_grease_pencil.cc
  intern/geometry_component_instances.cc
  intern/geometry_component_mesh.cc
  intern/geometry_component_pointcloud.cc
  intern/geometry_component_volume.cc
  intern/geometry_fields.cc
  intern/geometry_set.cc
  intern/geometry_set_instances.cc
  intern/gpencil_geom_legacy.cc
  intern/gpencil_legacy.cc
  intern/gpencil_modifier_legacy.cc
  intern/grease_pencil.cc
  intern/grease_pencil_attributes.cc
  intern/grease_pencil_convert_legacy.cc
  intern/grease_pencil_vertex_groups.cc
  intern/icons.cc
  intern/icons_rasterize.cc
  intern/id_hash.cc
  intern/idprop.cc
  intern/idprop_create.cc
  intern/idprop_serialize.cc
  intern/idprop_utils.cc
  intern/idtype.cc
  intern/image.cc
  intern/image_format.cc
  intern/image_gen.cc
  intern/image_gpu.cc
  intern/image_partial_update.cc
  intern/image_save.cc
  intern/instances.cc
  intern/instances_attributes.cc
  intern/kelvinlet.cc
  intern/key.cc
  intern/keyconfig.cc
  intern/lattice.cc
  intern/lattice_deform.cc
  intern/layer.cc
  intern/layer_utils.cc
  intern/lib_id.cc
  intern/lib_id_delete.cc
  intern/lib_id_eval.cc
  intern/lib_id_remapper.cc
  intern/lib_override.cc
  intern/lib_override_proxy_conversion.cc
  intern/lib_query.cc
  intern/lib_remap.cc
  intern/library.cc
  intern/light.cc
  intern/light_linking.cc
  intern/lightprobe.cc
  intern/linestyle.cc
  intern/main.cc
  intern/main_idmap.cc
  intern/main_invariants.cc
  intern/main_namemap.cc
  intern/mask.cc
  intern/mask_evaluate.cc
  intern/mask_rasterize.cc
  intern/material.cc
  intern/mball.cc
  intern/mball_tessellate.cc
  intern/mesh.cc
  intern/mesh_attributes.cc
  intern/mesh_calc_edges.cc
  intern/mesh_convert.cc
  intern/mesh_data_update.cc
  intern/mesh_debug.cc
  intern/mesh_evaluate.cc
  intern/mesh_fair.cc
  intern/mesh_flip_faces.cc
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
  intern/mesh_topology_state.cc
  intern/mesh_validate.cc
  intern/mesh_wrapper.cc
  intern/modifier.cc
  intern/movieclip.cc
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
  intern/nla.cc
  intern/node.cc
  intern/node_enum_definition.cc
  intern/node_runtime.cc
  intern/node_socket_value.cc
  intern/node_tree_dot_export.cc
  intern/node_tree_field_inferencing.cc
  intern/node_tree_interface.cc
  intern/node_tree_reference_lifetimes.cc
  intern/node_tree_structure_type_inferencing.cc
  intern/node_tree_update.cc
  intern/node_tree_zones.cc
  intern/object.cc
  intern/object_deform.cc
  intern/object_dupli.cc
  intern/object_update.cc
  intern/ocean.cc
  intern/ocean_spectrum.cc
  intern/outliner_treehash.cc
  intern/packedFile.cc
  intern/paint.cc
  intern/paint_canvas.cc
  intern/paint_runtime.cc
  intern/particle.cc
  intern/particle_child.cc
  intern/particle_distribute.cc
  intern/particle_system.cc
  intern/path_templates.cc
  intern/pbvh.cc
  intern/pbvh_bmesh.cc
  intern/pbvh_pixels.cc
  intern/pbvh_pixels_copy.cc
  intern/pbvh_uv_islands.cc
  intern/pointcache.cc
  intern/pointcloud.cc
  intern/pointcloud_attributes.cc
  intern/pose_backup.cc
  intern/preferences.cc
  intern/preview_image.cc
  intern/report.cc
  intern/rigidbody.cc
  intern/scene.cc
  intern/screen.cc
  intern/shader_fx.cc
  intern/shrinkwrap.cc
  intern/softbody.cc
  intern/sound.cc
  intern/speaker.cc
  intern/studiolight.cc
  intern/subdiv.cc
  intern/subdiv_ccg.cc
  intern/subdiv_ccg_mask.cc
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
  intern/text.cc
  intern/text_suggestions.cc
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
  intern/unit.cc
  intern/uvproject.cc
  intern/vfont.cc
  intern/vfont_curve.cc
  intern/vfontdata_freetype.cc
  intern/viewer_path.cc
  intern/volume.cc
  intern/volume_grid.cc
  intern/volume_grid_fields.cc
  intern/volume_grid_file_cache.cc
  intern/volume_render.cc
  intern/volume_to_mesh.cc
  intern/wm_runtime.cc
  intern/workspace.cc
  intern/world.cc

  BKE_action.hh
  BKE_addon.h
  BKE_anim_data.hh
  BKE_anim_path.h
  BKE_anim_visualization.h
  BKE_animsys.h
  BKE_anonymous_attribute_id.hh
  BKE_anonymous_attribute_make.hh
  BKE_appdir.hh
  BKE_armature.hh
  BKE_asset.hh
  BKE_asset_edit.hh
  BKE_attribute.h
  BKE_attribute.hh
  BKE_attribute_filter.hh
  BKE_attribute_filters.hh
  BKE_attribute_legacy_convert.hh
  BKE_attribute_math.hh
  BKE_attribute_storage.hh
  BKE_attribute_storage_blend_write.hh
  BKE_autoexec.hh
  BKE_bake_data_block_id.hh
  BKE_bake_data_block_map.hh
  BKE_bake_geometry_nodes_modifier.hh
  BKE_bake_geometry_nodes_modifier_pack.hh
  BKE_bake_items.hh
  BKE_bake_items_paths.hh
  BKE_bake_items_serialize.hh
  BKE_bake_items_socket.hh
  BKE_blender.hh
  BKE_blender_cli_command.hh
  BKE_blender_copybuffer.hh
  BKE_blender_undo.hh
  BKE_blender_user_menu.hh
  BKE_blender_version.h
  BKE_blendfile.hh
  BKE_blendfile_link_append.hh
  BKE_boids.h
  BKE_bpath.hh
  BKE_brush.hh
  BKE_bvhutils.hh
  BKE_cachefile.hh
  BKE_callbacks.hh
  BKE_camera.h
  BKE_ccg.hh
  BKE_cloth.hh
  BKE_collection.hh
  BKE_collision.h
  BKE_colorband.hh
  BKE_colortools.hh
  BKE_compositor.hh
  BKE_compute_context_cache.hh
  BKE_compute_context_cache_fwd.hh
  BKE_compute_contexts.hh
  BKE_constraint.h
  BKE_context.hh
  BKE_cpp_types.hh
  BKE_crazyspace.hh
  BKE_cryptomatte.h
  BKE_cryptomatte.hh
  BKE_curve.hh
  BKE_curve_legacy_convert.hh
  BKE_curve_to_mesh.hh
  BKE_curveprofile.h
  BKE_curves.h
  BKE_curves.hh
  BKE_curves_utils.hh
  BKE_customdata.hh
  BKE_customdata_file.h
  BKE_data_transfer.h
  BKE_deform.hh
  BKE_displist.h
  BKE_duplilist.hh
  BKE_dynamicpaint.h
  BKE_editlattice.h
  BKE_editmesh.hh
  BKE_editmesh_bvh.hh
  BKE_editmesh_cache.hh
  BKE_editmesh_tangent.hh
  BKE_effect.h
  BKE_fcurve.hh
  BKE_fcurve_driver.h
  BKE_file_handler.hh
  BKE_fluid.h
  BKE_freestyle.h
  BKE_geometry_compare.hh
  BKE_geometry_fields.hh
  BKE_geometry_nodes_gizmos_transforms.hh
  BKE_geometry_nodes_reference_set.hh
  BKE_geometry_set.hh
  BKE_geometry_set_instances.hh
  BKE_global.hh
  BKE_gpencil_geom_legacy.h
  BKE_gpencil_legacy.h
  BKE_gpencil_modifier_legacy.h
  BKE_grease_pencil.h
  BKE_grease_pencil.hh
  BKE_grease_pencil_legacy_convert.hh
  BKE_grease_pencil_vertex_groups.hh
  BKE_icons.hh
  BKE_id_hash.hh
  BKE_idprop.hh
  BKE_idtype.hh
  BKE_image.hh
  BKE_image_format.hh
  BKE_image_partial_update.hh
  BKE_image_save.hh
  BKE_image_wrappers.hh
  BKE_instances.hh
  BKE_kelvinlet.h
  BKE_key.hh
  BKE_keyconfig.h
  BKE_lattice.hh
  BKE_layer.hh
  BKE_lib_id.hh
  BKE_lib_override.hh
  BKE_lib_query.hh
  BKE_lib_remap.hh
  BKE_library.hh
  BKE_light.h
  BKE_light_linking.h
  BKE_lightprobe.h
  BKE_linestyle.h
  BKE_main.hh
  BKE_main_idmap.hh
  BKE_main_invariants.hh
  BKE_main_namemap.hh
  BKE_mask.h
  BKE_material.hh
  BKE_mball.hh
  BKE_mball_tessellate.hh
  BKE_mesh.h
  BKE_mesh.hh
  BKE_mesh_fair.hh
  BKE_mesh_iterators.hh
  BKE_mesh_legacy_convert.hh
  BKE_mesh_mapping.hh
  BKE_mesh_mirror.hh
  BKE_mesh_remap.hh
  BKE_mesh_remesh_voxel.hh
  BKE_mesh_runtime.hh
  BKE_mesh_sample.hh
  BKE_mesh_tangent.hh
  BKE_mesh_topology_state.hh
  BKE_mesh_types.hh
  BKE_mesh_wrapper.hh
  BKE_modifier.hh
  BKE_movieclip.h
  BKE_multires.hh
  BKE_nla.hh
  BKE_node.hh
  BKE_node_enum.hh
  BKE_node_legacy_types.hh
  BKE_node_runtime.hh
  BKE_node_socket_value.hh
  BKE_node_socket_value_fwd.hh
  BKE_node_tree_dot_export.hh
  BKE_node_tree_interface.hh
  BKE_node_tree_reference_lifetimes.hh
  BKE_node_tree_update.hh
  BKE_node_tree_zones.hh
  BKE_object.hh
  BKE_object_deform.h
  BKE_object_types.hh
  BKE_ocean.h
  BKE_outliner_treehash.hh
  BKE_packedFile.hh
  BKE_paint.hh
  BKE_paint_bvh.hh
  BKE_paint_bvh_pixels.hh
  BKE_paint_types.hh
  BKE_particle.h
  BKE_path_templates.hh
  BKE_pointcache.h
  BKE_pointcloud.hh
  BKE_pose_backup.h
  BKE_preferences.h
  BKE_preview_image.hh
  BKE_report.hh
  BKE_rigidbody.h
  BKE_scene.hh
  BKE_scene_runtime.hh
  BKE_screen.hh
  BKE_shader_fx.h
  BKE_shrinkwrap.hh
  BKE_softbody.h
  BKE_sound.hh
  BKE_speaker.hh
  BKE_studiolight.h
  BKE_subdiv.hh
  BKE_subdiv_ccg.hh
  BKE_subdiv_deform.hh
  BKE_subdiv_eval.hh
  BKE_subdiv_foreach.hh
  BKE_subdiv_mesh.hh
  BKE_subdiv_modifier.hh
  BKE_subdiv_topology.hh
  BKE_text.h
  BKE_text_suggestions.h
  BKE_texture.h
  BKE_tracking.h
  BKE_type_conversions.hh
  BKE_undo_system.hh
  BKE_unit.hh
  BKE_uvproject.h
  BKE_vfont.hh
  BKE_vfontdata.hh
  BKE_viewer_path.hh
  BKE_volume.hh
  BKE_volume_enums.hh
  BKE_volume_grid.hh
  BKE_volume_grid_fields.hh
  BKE_volume_grid_file_cache.hh
  BKE_volume_grid_fwd.hh
  BKE_volume_grid_process.hh
  BKE_volume_grid_type_traits.hh
  BKE_volume_openvdb.hh
  BKE_volume_render.hh
  BKE_volume_to_mesh.hh
  BKE_wm_runtime.hh
  BKE_workspace.hh
  BKE_world.h

  nla_private.h
  particle_private.h
  tracking_private.h

  intern/attribute_access_intern.hh
  intern/attribute_storage_access.hh
  intern/data_transfer_intern.hh
  intern/lib_intern.hh
  intern/multires_inline.hh
  intern/multires_reshape.hh
  intern/multires_unsubdivide.hh
  intern/ocean_intern.h
  intern/pbvh_intern.hh
  intern/pbvh_pixels_copy.hh
  intern/pbvh_uv_islands.hh
  intern/subdiv_converter.hh
  intern/subdiv_inline.hh
)

set(LIB
  PRIVATE bf::animrig
  PRIVATE bf::asset_system
  PRIVATE bf::blenfont
  PRIVATE bf::blenlib
  PRIVATE bf::blenloader
  PRIVATE bf::blentranslation
  PRIVATE bf::bmesh
  PRIVATE bf::depsgraph
  PRIVATE bf::dna
  PRIVATE bf::draw
  PRIVATE bf::extern::curve_fit_nd
  PRIVATE bf::functions
  PRIVATE bf::gpu
  bf_ikplugin
  PRIVATE bf::imbuf
  PRIVATE bf::imbuf::movie
  PRIVATE bf::intern::clog
  bf_intern_ghost
  PRIVATE bf::intern::guardedalloc
  PUBLIC bf::intern::optional::openvdb  # Contains intern-openvdb items in public headers
  bf_intern_libmv  # Uses stub when disabled.
  bf_intern_mikktspace
  bf_intern_opensubdiv  # Uses stub when disabled.
  bf_modifiers
  PRIVATE bf::nodes
  PRIVATE bf::render
  bf_rna
  PRIVATE bf::sequencer
  bf_shader_fx
  bf_simulation
  PRIVATE bf::extern::fmtlib
  PRIVATE bf::extern::xxhash
  PRIVATE bf::intern::atomic
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

  if(WITH_RUBBERBAND)
    add_definitions(-DWITH_RUBBERBAND)
  endif()
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
  add_definitions(-DWITH_IMAGE_OPENEXR)
endif()

if(WITH_IMAGE_OPENJPEG)
  add_definitions(-DWITH_IMAGE_OPENJPEG)
endif()

if(WITH_IMAGE_CINEON)
  add_definitions(-DWITH_IMAGE_CINEON)
endif()

if(WITH_IMAGE_WEBP)
  add_definitions(-DWITH_IMAGE_WEBP)
endif()

if(WITH_CODEC_FFMPEG)
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

if(WITH_INPUT_IME)
  add_definitions(-DWITH_INPUT_IME)
endif()

# # Warnings as errors, this is too strict!
# if(MSVC)
#    string(APPEND CMAKE_C_FLAGS " /WX")
# endif()

blender_add_lib(bf_blenkernel "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
add_library(bf::blenkernel ALIAS bf_blenkernel)

# RNA_prototypes.hh
add_dependencies(bf_blenkernel bf_rna)


if(WITH_GTESTS)
  set(TEST_SRC
    intern/action_test.cc
    intern/armature_deform_test.cc
    intern/armature_test.cc
    intern/asset_metadata_test.cc
    intern/attribute_storage_test.cc
    intern/bpath_test.cc
    intern/brush_test.cc
    intern/cryptomatte_test.cc
    intern/curves_geometry_test.cc
    intern/deform_test.cc
    intern/fcurve_test.cc
    intern/file_handler_test.cc
    intern/grease_pencil_test.cc
    intern/idprop_serialize_test.cc
    intern/idprop_test.cc
    intern/image_partial_update_test.cc
    intern/image_test.cc
    intern/lattice_deform_test.cc
    intern/layer_test.cc
    intern/lib_id_remapper_test.cc
    intern/lib_id_test.cc
    intern/lib_query_test.cc
    intern/lib_remap_test.cc
    intern/main_test.cc
    intern/nla_test.cc
    intern/path_templates_test.cc
    intern/scene_test.cc
    intern/subdiv_ccg_test.cc
    intern/tracking_test.cc
    intern/volume_test.cc
  )
  set(TEST_INC
    # WARNING: this is a bad-level include which is only acceptable for tests
    # and even then it would be good if the dependency could be removed.
    ../editors/include
  )
  set(TEST_LIB
    ${LIB}
    bf_rna  # RNA_prototypes.hh
  )
  blender_add_test_suite_lib(blenkernel "${TEST_SRC}" "${INC};${TEST_INC}" "${INC_SYS}" "${TEST_LIB}")
endif()
