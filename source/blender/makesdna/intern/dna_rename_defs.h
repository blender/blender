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

/** \file \ingroup DNA
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
 * - Renaming something that has already been renamed can be done by editing the existing rename macro.
 *   All references to the previous destination name can be removed since they're
 *   never written to disk.
 *
 * \see versioning_dna.c for a actual version patching.
 */

/* No include guard (intentional). */

/* Match RNA names where possible, keep sorted. */

DNA_STRUCT_RENAME(SpaceButs, SpaceProperties)
DNA_STRUCT_RENAME(SpaceIpo, SpaceGraph)
DNA_STRUCT_RENAME(SpaceOops, SpaceOutliner)
DNA_STRUCT_RENAME_ELEM(Camera, YF_dofdist, dof_distance)
DNA_STRUCT_RENAME_ELEM(Camera, clipend, clip_end)
DNA_STRUCT_RENAME_ELEM(Camera, clipsta, clip_start)
DNA_STRUCT_RENAME_ELEM(View3D, far, clip_end)
DNA_STRUCT_RENAME_ELEM(View3D, near, clip_start)

#if 0
DNA_STRUCT_RENAME(Lamp, Light)
DNA_STRUCT_RENAME_ELEM(Object, dup_group, instance_collection)
#endif
