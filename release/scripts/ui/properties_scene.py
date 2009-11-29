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
#  Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>
import bpy

narrowui = 180


class SceneButtonsPanel(bpy.types.Panel):
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "scene"

    def poll(self, context):
        return context.scene


class SCENE_PT_scene(SceneButtonsPanel):
    bl_label = "Scene"
    COMPAT_ENGINES = set(['BLENDER_RENDER'])

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


class SCENE_PT_unit(SceneButtonsPanel):
    bl_label = "Units"
    COMPAT_ENGINES = set(['BLENDER_RENDER'])

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
        col.operator("anim.keying_set_add", icon='ICON_ZOOMIN', text="")
        col.operator("anim.keying_set_remove", icon='ICON_ZOOMOUT', text="")

        ks = scene.active_keying_set
        if ks:
            row = layout.row()

            col = row.column()
            col.prop(ks, "name")
            col.prop(ks, "absolute")

            if wide_ui:
                col = row.column()
            col.label(text="Keyframing Settings:")
            col.prop(ks, "insertkey_needed", text="Needed")
            col.prop(ks, "insertkey_visual", text="Visual")


class SCENE_PT_keying_set_paths(SceneButtonsPanel):
    bl_label = "Active Keying Set"

    def poll(self, context):
        return (context.scene.active_keying_set is not None)

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
        col.operator("anim.keying_set_path_add", icon='ICON_ZOOMIN', text="")
        col.operator("anim.keying_set_path_remove", icon='ICON_ZOOMOUT', text="")

        ksp = ks.active_path
        if ksp:
            col = layout.column()
            col.label(text="Target:")
            col.template_any_ID(ksp, "id", "id_type")
            col.template_path_builder(ksp, "rna_path", ksp.id)


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


class SCENE_PT_physics(SceneButtonsPanel):
    bl_label = "Gravity"
    COMPAT_ENGINES = set(['BLENDER_RENDER'])

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

bpy.types.register(SCENE_PT_scene)
bpy.types.register(SCENE_PT_unit)
bpy.types.register(SCENE_PT_keying_sets)
bpy.types.register(SCENE_PT_keying_set_paths)
bpy.types.register(SCENE_PT_physics)
