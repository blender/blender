/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 *
 * Defines in this header are only used to define blend file storage.
 * This allows us to rename variables & structs without breaking compatibility.
 *
 * - When renaming the member of a struct which has itself been renamed
 *   refer to the newer name, not the original.
 *
 * - Changes here only change generated code for `makesdna.cc` and `makesrna.cc`
 *   without impacting Blender's run-time, besides allowing us to use the new names.
 *
 * - Renaming something that has already been renamed can be done
 *   by editing the existing rename macro.
 *   All references to the previous destination name can be removed since they're
 *   never written to disk.
 *
 * - Old names aren't sanity checked (since this file is the only place that knows about them)
 *   typos in the old names will break both backwards & forwards compatibility **TAKE CARE**.
 *
 * - Before editing rename defines run:
 *
 *   `sha1sum $BUILD_DIR/source/blender/makesdna/intern/dna.c`
 *
 *   Compare the results before & after to ensure all changes are reversed by renaming
 *   and the DNA remains unchanged.
 *
 * \see versioning_dna.c for actual version patching.
 */

/* No include guard (intentional). */

/* Match RNA names where possible. */

/* NOTE: Keep sorted! */

DNA_STRUCT_RENAME(Lamp, Light)
DNA_STRUCT_RENAME(SpaceButs, SpaceProperties)
DNA_STRUCT_RENAME(SpaceIpo, SpaceGraph)
DNA_STRUCT_RENAME(SpaceOops, SpaceOutliner)
DNA_STRUCT_RENAME_ELEM(BPoint, alfa, tilt)
DNA_STRUCT_RENAME_ELEM(BezTriple, alfa, tilt)
DNA_STRUCT_RENAME_ELEM(Bone, curveInX, curve_in_x)
DNA_STRUCT_RENAME_ELEM(Bone, curveInY, curve_in_z)
DNA_STRUCT_RENAME_ELEM(Bone, curveOutX, curve_out_x)
DNA_STRUCT_RENAME_ELEM(Bone, curveOutY, curve_out_z)
DNA_STRUCT_RENAME_ELEM(Bone, scaleIn, scale_in_x)
DNA_STRUCT_RENAME_ELEM(Bone, scaleOut, scale_out_x)
DNA_STRUCT_RENAME_ELEM(Bone, scale_in_y, scale_in_z)
DNA_STRUCT_RENAME_ELEM(Bone, scale_out_y, scale_out_z)
DNA_STRUCT_RENAME_ELEM(BrushGpencilSettings, gradient_f, hardeness)
DNA_STRUCT_RENAME_ELEM(BrushGpencilSettings, gradient_s, aspect_ratio)
DNA_STRUCT_RENAME_ELEM(Camera, YF_dofdist, dof_distance)
DNA_STRUCT_RENAME_ELEM(Camera, clipend, clip_end)
DNA_STRUCT_RENAME_ELEM(Camera, clipsta, clip_start)
DNA_STRUCT_RENAME_ELEM(Collection, dupli_ofs, instance_offset)
DNA_STRUCT_RENAME_ELEM(Curve, ext1, extrude)
DNA_STRUCT_RENAME_ELEM(Curve, ext2, bevel_radius)
DNA_STRUCT_RENAME_ELEM(Curve, len_wchar, len_char32)
DNA_STRUCT_RENAME_ELEM(Curve, loc, texspace_location)
DNA_STRUCT_RENAME_ELEM(Curve, size, texspace_size)
DNA_STRUCT_RENAME_ELEM(Curve, texflag, texspace_flag)
DNA_STRUCT_RENAME_ELEM(Curve, width, offset)
DNA_STRUCT_RENAME_ELEM(CurvesGeometry, curve_size, curve_num)
DNA_STRUCT_RENAME_ELEM(CurvesGeometry, point_size, point_num)
DNA_STRUCT_RENAME_ELEM(CustomDataExternal, filename, filepath)
DNA_STRUCT_RENAME_ELEM(Editing, over_border, overlay_frame_rect)
DNA_STRUCT_RENAME_ELEM(Editing, over_cfra, overlay_frame_abs)
DNA_STRUCT_RENAME_ELEM(Editing, over_flag, overlay_frame_flag)
DNA_STRUCT_RENAME_ELEM(Editing, over_ofs, overlay_frame_ofs)
DNA_STRUCT_RENAME_ELEM(FileGlobal, filename, filepath)
DNA_STRUCT_RENAME_ELEM(FluidDomainSettings, cache_frame_pause_guiding, cache_frame_pause_guide)
DNA_STRUCT_RENAME_ELEM(FluidDomainSettings, guiding_alpha, guide_alpha)
DNA_STRUCT_RENAME_ELEM(FluidDomainSettings, guiding_beta, guide_beta)
DNA_STRUCT_RENAME_ELEM(FluidDomainSettings, guiding_parent, guide_parent)
DNA_STRUCT_RENAME_ELEM(FluidDomainSettings, guiding_source, guide_source)
DNA_STRUCT_RENAME_ELEM(FluidDomainSettings, guiding_vel_factor, guide_vel_factor)
DNA_STRUCT_RENAME_ELEM(FluidEffectorSettings, guiding_mode, guide_mode)
DNA_STRUCT_RENAME_ELEM(GreasePencil, drawing_array_size, drawing_array_num)
DNA_STRUCT_RENAME_ELEM(GreasePencil, material_array_size, material_array_num)
DNA_STRUCT_RENAME_ELEM(GreasePencilLayerFramesMapStorage, size, num)
DNA_STRUCT_RENAME_ELEM(HookModifierData, totindex, indexar_num)
DNA_STRUCT_RENAME_ELEM(Image, name, filepath)
DNA_STRUCT_RENAME_ELEM(Light, energy_new, energy)
DNA_STRUCT_RENAME_ELEM(Light, energy, energy_deprecated)
DNA_STRUCT_RENAME_ELEM(LaplacianDeformModifierData, total_verts, verts_num)
DNA_STRUCT_RENAME_ELEM(Library, name, filepath)
DNA_STRUCT_RENAME_ELEM(LineartGpencilModifierData, line_types, edge_types)
DNA_STRUCT_RENAME_ELEM(LineartGpencilModifierData, transparency_flags, mask_switches)
DNA_STRUCT_RENAME_ELEM(LineartGpencilModifierData, transparency_mask, material_mask_bits)
DNA_STRUCT_RENAME_ELEM(MDefCell, totinfluence, influences_num)
DNA_STRUCT_RENAME_ELEM(MEdge, bweight, bweight_legacy)
DNA_STRUCT_RENAME_ELEM(MEdge, crease, crease_legacy)
DNA_STRUCT_RENAME_ELEM(MEdge, flag, flag_legacy)
DNA_STRUCT_RENAME_ELEM(MPoly, flag, flag_legacy)
DNA_STRUCT_RENAME_ELEM(MPoly, mat_nr, mat_nr_legacy)
DNA_STRUCT_RENAME_ELEM(MVert, bweight, bweight_legacy)
DNA_STRUCT_RENAME_ELEM(MVert, co, co_legacy)
DNA_STRUCT_RENAME_ELEM(MVert, flag, flag_legacy)
DNA_STRUCT_RENAME_ELEM(MaskLayer, restrictflag, visibility_flag)
DNA_STRUCT_RENAME_ELEM(MaterialLineArt, transparency_mask, material_mask_bits)
DNA_STRUCT_RENAME_ELEM(Mesh, loc, texspace_location)
DNA_STRUCT_RENAME_ELEM(Mesh, size, texspace_size)
DNA_STRUCT_RENAME_ELEM(Mesh, texflag, texspace_flag)
DNA_STRUCT_RENAME_ELEM(MeshDeformModifierData, totcagevert, cage_verts_num)
DNA_STRUCT_RENAME_ELEM(MeshDeformModifierData, totinfluence, influences_num)
DNA_STRUCT_RENAME_ELEM(MeshDeformModifierData, totvert, verts_num)
DNA_STRUCT_RENAME_ELEM(MetaBall, loc, texspace_location)
DNA_STRUCT_RENAME_ELEM(MetaBall, size, texspace_size)
DNA_STRUCT_RENAME_ELEM(MetaBall, texflag, texspace_flag)
DNA_STRUCT_RENAME_ELEM(MovieClip, name, filepath)
DNA_STRUCT_RENAME_ELEM(MovieTracking, act_plane_track, act_plane_track_legacy)
DNA_STRUCT_RENAME_ELEM(MovieTracking, act_track, act_track_legacy)
DNA_STRUCT_RENAME_ELEM(MovieTracking, plane_tracks, plane_tracks_legacy)
DNA_STRUCT_RENAME_ELEM(MovieTracking, reconstruction, reconstruction_legacy)
DNA_STRUCT_RENAME_ELEM(MovieTracking, tracks, tracks_legacy)
DNA_STRUCT_RENAME_ELEM(MovieTrackingTrack, pat_min, pat_min_legacy)
DNA_STRUCT_RENAME_ELEM(MovieTrackingTrack, pat_max, pat_max_legacy)
DNA_STRUCT_RENAME_ELEM(MovieTrackingTrack, search_min, search_min_legacy)
DNA_STRUCT_RENAME_ELEM(MovieTrackingTrack, search_max, search_max_legacy)
DNA_STRUCT_RENAME_ELEM(MovieTrackingSettings, keyframe1, keyframe1_legacy)
DNA_STRUCT_RENAME_ELEM(MovieTrackingSettings, keyframe2, keyframe2_legacy)
DNA_STRUCT_RENAME_ELEM(MovieTrackingStabilization, rot_track, rot_track_legacy)
DNA_STRUCT_RENAME_ELEM(MovieTrackingCamera, principal, principal_legacy)
DNA_STRUCT_RENAME_ELEM(NodeCryptomatte, num_inputs, inputs_num)
DNA_STRUCT_RENAME_ELEM(Object, col, color)
DNA_STRUCT_RENAME_ELEM(Object, dup_group, instance_collection)
DNA_STRUCT_RENAME_ELEM(Object, dupfacesca, instance_faces_scale)
DNA_STRUCT_RENAME_ELEM(Object, imat, world_to_object)
DNA_STRUCT_RENAME_ELEM(Object, obmat, object_to_world)
DNA_STRUCT_RENAME_ELEM(Object, restrictflag, visibility_flag)
DNA_STRUCT_RENAME_ELEM(Object, size, scale)
DNA_STRUCT_RENAME_ELEM(Object_Runtime, crazyspace_num_verts, crazyspace_verts_num)
DNA_STRUCT_RENAME_ELEM(ParticleSettings, child_nbr, child_percent)
DNA_STRUCT_RENAME_ELEM(ParticleSettings, dup_group, instance_collection)
DNA_STRUCT_RENAME_ELEM(ParticleSettings, dup_ob, instance_object)
DNA_STRUCT_RENAME_ELEM(ParticleSettings, dupliweights, instance_weights)
DNA_STRUCT_RENAME_ELEM(ParticleSettings, ren_child_nbr, child_render_percent)
DNA_STRUCT_RENAME_ELEM(RenderData, bake_filter, bake_margin)
DNA_STRUCT_RENAME_ELEM(RigidBodyWorld, steps_per_second, substeps_per_frame)
DNA_STRUCT_RENAME_ELEM(SDefBind, numverts, verts_num)
DNA_STRUCT_RENAME_ELEM(SDefVert, numbinds, binds_num)
DNA_STRUCT_RENAME_ELEM(SpaceImage, pixel_snap_mode, pixel_round_mode)
DNA_STRUCT_RENAME_ELEM(SpaceSeq, overlay_type, overlay_frame_type)
DNA_STRUCT_RENAME_ELEM(Strip, dir, dirpath)
DNA_STRUCT_RENAME_ELEM(StripElem, name, filename)
DNA_STRUCT_RENAME_ELEM(StripProxy, dir, dirpath)
DNA_STRUCT_RENAME_ELEM(StripProxy, file, filename)
DNA_STRUCT_RENAME_ELEM(SurfaceDeformModifierData, num_mesh_verts, mesh_verts_num)
DNA_STRUCT_RENAME_ELEM(SurfaceDeformModifierData, numpoly, target_polys_num)
DNA_STRUCT_RENAME_ELEM(SurfaceDeformModifierData, numverts, bind_verts_num)
DNA_STRUCT_RENAME_ELEM(Text, name, filepath)
DNA_STRUCT_RENAME_ELEM(ThemeSpace, scrubbing_background, time_scrub_background)
DNA_STRUCT_RENAME_ELEM(ThemeSpace, show_back_grad, background_type)
DNA_STRUCT_RENAME_ELEM(UVProjectModifierData, num_projectors, projectors_num)
DNA_STRUCT_RENAME_ELEM(UserDef, gp_manhattendist, gp_manhattandist)
DNA_STRUCT_RENAME_ELEM(UserDef, pythondir, pythondir_legacy)
DNA_STRUCT_RENAME_ELEM(VFont, name, filepath)
DNA_STRUCT_RENAME_ELEM(View3D, far, clip_end)
DNA_STRUCT_RENAME_ELEM(View3D, near, clip_start)
DNA_STRUCT_RENAME_ELEM(View3D, ob_centre, ob_center)
DNA_STRUCT_RENAME_ELEM(View3D, ob_centre_bone, ob_center_bone)
DNA_STRUCT_RENAME_ELEM(View3D, ob_centre_cursor, ob_center_cursor)
DNA_STRUCT_RENAME_ELEM(bGPDstroke, gradient_f, hardeness)
DNA_STRUCT_RENAME_ELEM(bGPDstroke, gradient_s, aspect_ratio)
DNA_STRUCT_RENAME_ELEM(bPoseChannel, curveInX, curve_in_x)
DNA_STRUCT_RENAME_ELEM(bPoseChannel, curveInY, curve_in_z)
DNA_STRUCT_RENAME_ELEM(bPoseChannel, curveOutX, curve_out_x)
DNA_STRUCT_RENAME_ELEM(bPoseChannel, curveOutY, curve_out_z)
DNA_STRUCT_RENAME_ELEM(bPoseChannel, scaleIn, scale_in_x)
DNA_STRUCT_RENAME_ELEM(bPoseChannel, scaleOut, scale_out_x)
DNA_STRUCT_RENAME_ELEM(bPoseChannel, scale_in_y, scale_in_z)
DNA_STRUCT_RENAME_ELEM(bPoseChannel, scale_out_y, scale_out_z)
DNA_STRUCT_RENAME_ELEM(bSameVolumeConstraint, flag, free_axis)
DNA_STRUCT_RENAME_ELEM(bSound, name, filepath)
DNA_STRUCT_RENAME_ELEM(bTheme, tact, space_action)
DNA_STRUCT_RENAME_ELEM(bTheme, tbuts, space_properties)
DNA_STRUCT_RENAME_ELEM(bTheme, tclip, space_clip)
DNA_STRUCT_RENAME_ELEM(bTheme, tconsole, space_console)
DNA_STRUCT_RENAME_ELEM(bTheme, text, space_text)
DNA_STRUCT_RENAME_ELEM(bTheme, tfile, space_file)
DNA_STRUCT_RENAME_ELEM(bTheme, tima, space_image)
DNA_STRUCT_RENAME_ELEM(bTheme, tinfo, space_info)
DNA_STRUCT_RENAME_ELEM(bTheme, tipo, space_graph)
DNA_STRUCT_RENAME_ELEM(bTheme, tnla, space_nla)
DNA_STRUCT_RENAME_ELEM(bTheme, tnode, space_node)
DNA_STRUCT_RENAME_ELEM(bTheme, toops, space_outliner)
DNA_STRUCT_RENAME_ELEM(bTheme, tseq, space_sequencer)
DNA_STRUCT_RENAME_ELEM(bTheme, tstatusbar, space_statusbar)
DNA_STRUCT_RENAME_ELEM(bTheme, ttopbar, space_topbar)
DNA_STRUCT_RENAME_ELEM(bTheme, tuserpref, space_preferences)
DNA_STRUCT_RENAME_ELEM(bTheme, tv3d, space_view3d)
DNA_STRUCT_RENAME_ELEM(bUserAssetLibrary, path, dirpath)
/* Write with a different name, old Blender versions crash loading files with non-NULL
 * global_areas. See D9442. */
DNA_STRUCT_RENAME_ELEM(wmWindow, global_area_map, global_areas)

/* NOTE: Keep sorted! */
