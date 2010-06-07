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

narrowui = bpy.context.user_preferences.view.properties_width_check


class SceneButtonsPanel(bpy.types.Panel):
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "scene"

    def poll(self, context):
        return context.scene


class SCENE_PT_scene(SceneButtonsPanel):
    bl_label = "Scene"
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw(self, context):
        layout = self.layout
        wide_ui = context.region.width > narrowui
        scene = context.scene

        if wide_ui:
            layout.prop(scene, "camera")
            layout.prop(scene, "set", text="Background")
        else:
            layout.prop(scene, "camera", text="")
            layout.prop(scene, "set", text="")


class SCENE_PT_custom_props(SceneButtonsPanel, PropertyPanel):
    _context_path = "scene"


class SCENE_PT_unit(SceneButtonsPanel):
    bl_label = "Units"
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw(self, context):
        layout = self.layout
        wide_ui = context.region.width > narrowui
        unit = context.scene.unit_settings

        col = layout.column()
        col.row().prop(unit, "system", expand=True)

        split = layout.split()
        split.active = (unit.system != 'NONE')

        col = split.column()
        col.prop(unit, "scale_length", text="Scale")

        if wide_ui:
            col = split.column()
        col.prop(unit, "use_separate")

        layout.column().prop(unit, "rotation_units")


class SCENE_PT_keying_sets(SceneButtonsPanel):
    bl_label = "Keying Sets"

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        wide_ui = context.region.width > narrowui
        row = layout.row()

        col = row.column()
        col.template_list(scene, "keying_sets", scene, "active_keying_set_index", rows=2)

        col = row.column(align=True)
        col.operator("anim.keying_set_add", icon='ZOOMIN', text="")
        col.operator("anim.keying_set_remove", icon='ZOOMOUT', text="")

        ks = scene.active_keying_set
        if ks and ks.absolute:
            row = layout.row()

            col = row.column()
            col.prop(ks, "name")

            subcol = col.column()
            subcol.operator_context = 'INVOKE_DEFAULT'
            op = subcol.operator("anim.keying_set_export", text="Export to File")
            op.path = "keyingset.py"

            if wide_ui:
                col = row.column()
            col.label(text="Keyframing Settings:")
            col.prop(ks, "insertkey_needed", text="Needed")
            col.prop(ks, "insertkey_visual", text="Visual")
            col.prop(ks, "insertkey_xyz_to_rgb", text="XYZ to RGB")


class SCENE_PT_keying_set_paths(SceneButtonsPanel):
    bl_label = "Active Keying Set"

    def poll(self, context):
        return (context.scene.active_keying_set and context.scene.active_keying_set.absolute)

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        ks = scene.active_keying_set
        wide_ui = context.region.width > narrowui

        row = layout.row()
        row.label(text="Paths:")

        row = layout.row()

        col = row.column()
        col.template_list(ks, "paths", ks, "active_path_index", rows=2)

        col = row.column(align=True)
        col.operator("anim.keying_set_path_add", icon='ZOOMIN', text="")
        col.operator("anim.keying_set_path_remove", icon='ZOOMOUT', text="")

        ksp = ks.active_path
        if ksp:
            col = layout.column()
            col.label(text="Target:")
            col.template_any_ID(ksp, "id", "id_type")
            col.template_path_builder(ksp, "data_path", ksp.id)


            row = layout.row()

            col = row.column()
            col.label(text="Array Target:")
            col.prop(ksp, "entire_array")
            if ksp.entire_array is False:
                col.prop(ksp, "array_index")

            if wide_ui:
                col = row.column()
            col.label(text="F-Curve Grouping:")
            col.prop(ksp, "grouping")
            if ksp.grouping == 'NAMED':
                col.prop(ksp, "group")

            col.label(text="Keyframing Settings:")
            col.prop(ksp, "insertkey_needed", text="Needed")
            col.prop(ksp, "insertkey_visual", text="Visual")
            col.prop(ksp, "insertkey_xyz_to_rgb", text="XYZ to RGB")


class SCENE_PT_physics(SceneButtonsPanel):
    bl_label = "Gravity"
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw_header(self, context):
        self.layout.prop(context.scene, "use_gravity", text="")

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        wide_ui = context.region.width > narrowui

        layout.active = scene.use_gravity

        if wide_ui:
            layout.prop(scene, "gravity", text="")
        else:
            layout.column().prop(scene, "gravity", text="")


class SCENE_PT_simplify(SceneButtonsPanel):
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
        wide_ui = context.region.width > narrowui

        layout.active = rd.use_simplify

        split = layout.split()

        col = split.column()
        col.prop(rd, "simplify_subdivision", text="Subdivision")
        col.prop(rd, "simplify_child_particles", text="Child Particles")

        col.prop(rd, "simplify_triangulate")

        if wide_ui:
            col = split.column()
        col.prop(rd, "simplify_shadow_samples", text="Shadow Samples")
        col.prop(rd, "simplify_ao_sss", text="AO and SSS")


from bpy.props import *


class ANIM_OT_keying_set_export(bpy.types.Operator):
    "Export Keying Set to a python script."
    bl_idname = "anim.keying_set_export"
    bl_label = "Export Keying Set..."

    path = bpy.props.StringProperty(name="File Path", description="File path to write file to.")
    filename = bpy.props.StringProperty(name="File Name", description="Name of the file.")
    directory = bpy.props.StringProperty(name="Directory", description="Directory of the file.")
    filter_folder = bpy.props.BoolProperty(name="Filter folders", description="", default=True, options={'HIDDEN'})
    filter_text = bpy.props.BoolProperty(name="Filter text", description="", default=True, options={'HIDDEN'})
    filter_python = bpy.props.BoolProperty(name="Filter python", description="", default=True, options={'HIDDEN'})

    def execute(self, context):
        if not self.properties.path:
            raise Exception("File path not set.")

        f = open(self.properties.path, "w")
        if not f:
            raise Exception("Could not open file.")

        scene = context.scene
        ks = scene.active_keying_set


        f.write("# Keying Set: %s\n" % ks.name)

        f.write("import bpy\n\n")
        f.write("scene= bpy.data.scenes[0]\n\n")

        # Add KeyingSet and set general settings
        f.write("# Keying Set Level declarations\n")
        f.write("ks= scene.add_keying_set(name=\"%s\")\n" % ks.name)

        if ks.absolute is False:
            f.write("ks.absolute = False\n")
        f.write("\n")

        f.write("ks.insertkey_needed = %s\n" % ks.insertkey_needed)
        f.write("ks.insertkey_visual = %s\n" % ks.insertkey_visual)
        f.write("ks.insertkey_xyz_to_rgb = %s\n" % ks.insertkey_xyz_to_rgb)
        f.write("\n")


        # generate and write set of lookups for id's used in paths
        id_to_paths_cache = {} # cache for syncing ID-blocks to bpy paths + shorthands

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

            # id-block + RNA-path
            if ksp.id:
                # find the relevant shorthand from the cache
                id_bpy_path = id_to_paths_cache[ksp.id][0]
            else:
                id_bpy_path = "None" # XXX...
            f.write("%s, '%s'" % (id_bpy_path, ksp.data_path))

            # array index settings (if applicable)
            if ksp.entire_array:
                f.write(", index=-1")
            else:
                f.write(", index=%d" % ksp.array_index)

            # grouping settings (if applicable)
            # NOTE: the current default is KEYINGSET, but if this changes, change this code too
            if ksp.grouping == 'NAMED':
                f.write(", grouping_method='%s', group_name=\"%s\"" % (ksp.grouping, ksp.group))
            elif ksp.grouping != 'KEYINGSET':
                f.write(", grouping_method='%s'" % ksp.grouping)

            # finish off
            f.write(")\n")

        f.write("\n")
        f.close()

        return {'FINISHED'}

    def invoke(self, context, event):
        wm = context.manager
        wm.add_fileselect(self)
        return {'RUNNING_MODAL'}


classes = [
    SCENE_PT_scene,
    SCENE_PT_unit,
    SCENE_PT_keying_sets,
    SCENE_PT_keying_set_paths,
    SCENE_PT_physics,
    SCENE_PT_simplify,

    SCENE_PT_custom_props,

    ANIM_OT_keying_set_export]


def register():
    register = bpy.types.register
    for cls in classes:
        register(cls)


def unregister():
    unregister = bpy.types.unregister
    for cls in classes:
        unregister(cls)

if __name__ == "__main__":
    register()
