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

# <pep8 compliant>
import bpy
from rna_prop_ui import PropertyPanel


class SceneButtonsPanel():
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "scene"

    @classmethod
    def poll(cls, context):
        return context.scene


class SCENE_PT_scene(SceneButtonsPanel, bpy.types.Panel):
    bl_label = "Scene"
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw(self, context):
        layout = self.layout
        scene = context.scene

        layout.prop(scene, "camera")
        layout.prop(scene, "background_set", text="Background")


class SCENE_PT_unit(SceneButtonsPanel, bpy.types.Panel):
    bl_label = "Units"
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw(self, context):
        layout = self.layout
        unit = context.scene.unit_settings

        col = layout.column()
        col.row().prop(unit, "system", expand=True)
        col.row().prop(unit, "system_rotation", expand=True)

        row = layout.row()
        row.active = (unit.system != 'NONE')
        row.prop(unit, "scale_length", text="Scale")
        row.prop(unit, "use_separate")


class SCENE_PT_keying_sets(SceneButtonsPanel, bpy.types.Panel):
    bl_label = "Keying Sets"

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        row = layout.row()

        col = row.column()
        col.template_list(scene, "keying_sets", scene.keying_sets, "active_index", rows=2)

        col = row.column(align=True)
        col.operator("anim.keying_set_add", icon='ZOOMIN', text="")
        col.operator("anim.keying_set_remove", icon='ZOOMOUT', text="")

        ks = scene.keying_sets.active
        if ks and ks.is_path_absolute:
            row = layout.row()

            col = row.column()
            col.prop(ks, "name")

            subcol = col.column()
            subcol.operator_context = 'INVOKE_DEFAULT'
            op = subcol.operator("anim.keying_set_export", text="Export to File")
            op.filepath = "keyingset.py"

            col = row.column()
            col.label(text="Keyframing Settings:")
            col.prop(ks, "bl_options")


class SCENE_PT_keying_set_paths(SceneButtonsPanel, bpy.types.Panel):
    bl_label = "Active Keying Set"

    @classmethod
    def poll(cls, context):
        ks = context.scene.keying_sets.active
        return (ks and ks.is_path_absolute)

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        ks = scene.keying_sets.active

        row = layout.row()
        row.label(text="Paths:")

        row = layout.row()

        col = row.column()
        col.template_list(ks, "paths", ks.paths, "active_index", rows=2)

        col = row.column(align=True)
        col.operator("anim.keying_set_path_add", icon='ZOOMIN', text="")
        col.operator("anim.keying_set_path_remove", icon='ZOOMOUT', text="")

        ksp = ks.paths.active
        if ksp:
            col = layout.column()
            col.label(text="Target:")
            col.template_any_ID(ksp, "id", "id_type")
            col.template_path_builder(ksp, "data_path", ksp.id)

            row = layout.row()

            col = row.column()
            col.label(text="Array Target:")
            col.prop(ksp, "use_entire_array")
            if ksp.use_entire_array is False:
                col.prop(ksp, "array_index")

            col = row.column()
            col.label(text="F-Curve Grouping:")
            col.prop(ksp, "group_method")
            if ksp.group_method == 'NAMED':
                col.prop(ksp, "group")

            col.prop(ksp, "bl_options")


class SCENE_PT_physics(SceneButtonsPanel, bpy.types.Panel):
    bl_label = "Gravity"
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw_header(self, context):
        self.layout.prop(context.scene, "use_gravity", text="")

    def draw(self, context):
        layout = self.layout

        scene = context.scene

        layout.active = scene.use_gravity

        layout.prop(scene, "gravity", text="")


class SCENE_PT_simplify(SceneButtonsPanel, bpy.types.Panel):
    bl_label = "Simplify"
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw_header(self, context):
        scene = context.scene
        rd = scene.render
        self.layout.prop(rd, "use_simplify", text="")

    def draw(self, context):
        layout = self.layout
        scene = context.scene
        rd = scene.render

        layout.active = rd.use_simplify

        split = layout.split()

        col = split.column()
        col.prop(rd, "simplify_subdivision", text="Subdivision")
        col.prop(rd, "simplify_child_particles", text="Child Particles")

        col.prop(rd, "use_simplify_triangulate")

        col = split.column()
        col.prop(rd, "simplify_shadow_samples", text="Shadow Samples")
        col.prop(rd, "simplify_ao_sss", text="AO and SSS")


class SCENE_PT_custom_props(SceneButtonsPanel, PropertyPanel, bpy.types.Panel):
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}
    _context_path = "scene"
    _property_type = bpy.types.Scene

#  XXX, move operator to op/ dir


class ANIM_OT_keying_set_export(bpy.types.Operator):
    "Export Keying Set to a python script."
    bl_idname = "anim.keying_set_export"
    bl_label = "Export Keying Set..."

    filepath = bpy.props.StringProperty(name="File Path", description="Filepath to write file to.")
    filter_folder = bpy.props.BoolProperty(name="Filter folders", description="", default=True, options={'HIDDEN'})
    filter_text = bpy.props.BoolProperty(name="Filter text", description="", default=True, options={'HIDDEN'})
    filter_python = bpy.props.BoolProperty(name="Filter python", description="", default=True, options={'HIDDEN'})

    def execute(self, context):
        if not self.filepath:
            raise Exception("Filepath not set.")

        f = open(self.filepath, "w")
        if not f:
            raise Exception("Could not open file.")

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

if __name__ == "__main__":  # only for live edit.
    bpy.utils.register_module(__name__)
