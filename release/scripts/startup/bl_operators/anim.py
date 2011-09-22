# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8-80 compliant>

import bpy
from bpy.types import Operator


class ANIM_OT_keying_set_export(Operator):
    "Export Keying Set to a python script"
    bl_idname = "anim.keying_set_export"
    bl_label = "Export Keying Set..."

    filepath = bpy.props.StringProperty(name="File Path", description="Filepath to write file to")
    filter_folder = bpy.props.BoolProperty(name="Filter folders", description="", default=True, options={'HIDDEN'})
    filter_text = bpy.props.BoolProperty(name="Filter text", description="", default=True, options={'HIDDEN'})
    filter_python = bpy.props.BoolProperty(name="Filter python", description="", default=True, options={'HIDDEN'})

    def execute(self, context):
        if not self.filepath:
            raise Exception("Filepath not set")

        f = open(self.filepath, "w")
        if not f:
            raise Exception("Could not open file")

        scene = context.scene
        ks = scene.keying_sets.active

        f.write("# Keying Set: %s\n" % ks.name)

        f.write("import bpy\n\n")
        f.write("scene= bpy.data.scenes[0]\n\n")  # XXX, why not use the current scene?

        # Add KeyingSet and set general settings
        f.write("# Keying Set Level declarations\n")
        f.write("ks= scene.keying_sets.new(name=\"%s\")\n" % ks.name)

        if not ks.is_path_absolute:
            f.write("ks.is_path_absolute = False\n")
        f.write("\n")

        f.write("ks.bl_options = %r\n" % ks.bl_options)
        f.write("\n")

        # generate and write set of lookups for id's used in paths
        id_to_paths_cache = {}  # cache for syncing ID-blocks to bpy paths + shorthands

        for ksp in ks.paths:
            if ksp.id is None:
                continue
            if ksp.id in id_to_paths_cache:
                continue

            # - idtype_list is used to get the list of id-datablocks from bpy.data.*
            #   since this info isn't available elsewhere
            # - id.bl_rna.name gives a name suitable for UI,
            #   with a capitalised first letter, but we need
            #   the plural form that's all lower case
            idtype_list = ksp.id.bl_rna.name.lower() + "s"
            id_bpy_path = "bpy.data.%s[\"%s\"]" % (idtype_list, ksp.id.name)

            # shorthand ID for the ID-block (as used in the script)
            short_id = "id_%d" % len(id_to_paths_cache)

            # store this in the cache now
            id_to_paths_cache[ksp.id] = [short_id, id_bpy_path]

        f.write("# ID's that are commonly used\n")
        for id_pair in id_to_paths_cache.values():
            f.write("%s = %s\n" % (id_pair[0], id_pair[1]))
        f.write("\n")

        # write paths
        f.write("# Path Definitions\n")
        for ksp in ks.paths:
            f.write("ksp = ks.paths.add(")

            # id-block + data_path
            if ksp.id:
                # find the relevant shorthand from the cache
                id_bpy_path = id_to_paths_cache[ksp.id][0]
            else:
                id_bpy_path = "None"  # XXX...
            f.write("%s, '%s'" % (id_bpy_path, ksp.data_path))

            # array index settings (if applicable)
            if ksp.use_entire_array:
                f.write(", index=-1")
            else:
                f.write(", index=%d" % ksp.array_index)

            # grouping settings (if applicable)
            # NOTE: the current default is KEYINGSET, but if this changes, change this code too
            if ksp.group_method == 'NAMED':
                f.write(", group_method='%s', group_name=\"%s\"" % (ksp.group_method, ksp.group))
            elif ksp.group_method != 'KEYINGSET':
                f.write(", group_method='%s'" % ksp.group_method)

            # finish off
            f.write(")\n")

        f.write("\n")
        f.close()

        return {'FINISHED'}

    def invoke(self, context, event):
        wm = context.window_manager
        wm.fileselect_add(self)
        return {'RUNNING_MODAL'}
