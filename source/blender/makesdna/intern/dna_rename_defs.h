/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 * DNA handling
 */

/** \file
 * \ingroup DNA
 *
 * Defines in this header are only used to define blend file storage.
 * This allows us to rename variables & structs without breaking compatibility.
 *
 * - When renaming the member of a struct which has it's self been renamed
 *   refer to the newer name, not the original.
 *
 * - Changes here only change generated code for `makesdna.c` and `makesrna.c`
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

/* Match RNA names where possible, keep sorted. */

DNA_STRUCT_RENAME(Lamp, Light)
DNA_STRUCT_RENAME(SpaceButs, SpaceProperties)
DNA_STRUCT_RENAME(SpaceIpo, SpaceGraph)
DNA_STRUCT_RENAME(SpaceOops, SpaceOutliner)
DNA_STRUCT_RENAME_ELEM(BPoint, alfa, tilt)
DNA_STRUCT_RENAME_ELEM(BezTriple, alfa, tilt)
DNA_STRUCT_RENAME_ELEM(Bone, curveInX, curve_in_x)
DNA_STRUCT_RENAME_ELEM(Bone, curveInY, curve_in_y)
DNA_STRUCT_RENAME_ELEM(Bone, curveOutX, curve_out_x)
DNA_STRUCT_RENAME_ELEM(Bone, curveOutY, curve_out_y)
DNA_STRUCT_RENAME_ELEM(Bone, scaleIn, scale_in_x)
DNA_STRUCT_RENAME_ELEM(Bone, scaleOut, scale_out_x)
DNA_STRUCT_RENAME_ELEM(Camera, YF_dofdist, dof_distance)
DNA_STRUCT_RENAME_ELEM(Camera, clipend, clip_end)
DNA_STRUCT_RENAME_ELEM(Camera, clipsta, clip_start)
DNA_STRUCT_RENAME_ELEM(Collection, dupli_ofs, instance_offset)
DNA_STRUCT_RENAME_ELEM(Object, col, color)
DNA_STRUCT_RENAME_ELEM(Object, dup_group, instance_collection)
DNA_STRUCT_RENAME_ELEM(Object, dupfacesca, instance_faces_scale)
DNA_STRUCT_RENAME_ELEM(Object, size, scale)
DNA_STRUCT_RENAME_ELEM(ParticleSettings, dup_group, instance_collection)
DNA_STRUCT_RENAME_ELEM(ParticleSettings, dup_ob, instance_object)
DNA_STRUCT_RENAME_ELEM(ParticleSettings, dupliweights, instance_weights)
DNA_STRUCT_RENAME_ELEM(ThemeSpace, scrubbing_background, time_scrub_background)
DNA_STRUCT_RENAME_ELEM(View3D, far, clip_end)
DNA_STRUCT_RENAME_ELEM(View3D, near, clip_start)
DNA_STRUCT_RENAME_ELEM(bPoseChannel, curveInX, curve_in_x)
DNA_STRUCT_RENAME_ELEM(bPoseChannel, curveInY, curve_in_y)
DNA_STRUCT_RENAME_ELEM(bPoseChannel, curveOutX, curve_out_x)
DNA_STRUCT_RENAME_ELEM(bPoseChannel, curveOutY, curve_out_y)
DNA_STRUCT_RENAME_ELEM(bPoseChannel, scaleIn, scale_in_x)
DNA_STRUCT_RENAME_ELEM(bPoseChannel, scaleOut, scale_out_x)
DNA_STRUCT_RENAME_ELEM(bSameVolumeConstraint, flag, free_axis)
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
