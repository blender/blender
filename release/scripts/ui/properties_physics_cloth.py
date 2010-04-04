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

narrowui = 180


from properties_physics_common import point_cache_ui
from properties_physics_common import effector_weights_ui


def cloth_panel_enabled(md):
    return md.point_cache.baked is False


class CLOTH_MT_presets(bpy.types.Menu):
    '''
    Creates the menu items by scanning scripts/templates
    '''
    bl_label = "Cloth Presets"
    preset_subdir = "cloth"
    preset_operator = "script.python_file_run"
    draw = bpy.types.Menu.draw_preset


class PhysicButtonsPanel(bpy.types.Panel):
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "physics"

    def poll(self, context):
        ob = context.object
        rd = context.scene.render
        return (ob and ob.type == 'MESH') and (not rd.use_game_engine)


class PHYSICS_PT_cloth(PhysicButtonsPanel):
    bl_label = "Cloth"

    def draw(self, context):
        layout = self.layout

        md = context.cloth
        ob = context.object
        wide_ui = context.region.width > narrowui

        split = layout.split()
        split.operator_context = 'EXEC_DEFAULT'

        if md:
            # remove modifier + settings
            split.set_context_pointer("modifier", md)
            split.operator("object.modifier_remove", text="Remove")

            row = split.row(align=True)
            row.prop(md, "render", text="")
            row.prop(md, "realtime", text="")
        else:
            # add modifier
            split.operator("object.modifier_add", text="Add").type = 'CLOTH'
            if wide_ui:
                split.column()

        split.operator_context = 'INVOKE_DEFAULT'

        if md:
            cloth = md.settings

            split = layout.split()

            split.active = cloth_panel_enabled(md)

            col = split.column()

            col.label(text="Presets:")
            sub = col.row(align=True).split(percentage=0.75)
            sub.menu("CLOTH_MT_presets", text="Presets")
            sub.operator("cloth.preset_add", text="Add")

            col.label(text="Quality:")
            col.prop(cloth, "quality", text="Steps", slider=True)

            col.label(text="Material:")
            col.prop(cloth, "mass")
            col.prop(cloth, "structural_stiffness", text="Structural")
            col.prop(cloth, "bending_stiffness", text="Bending")

            if wide_ui:
                col = split.column()

            col.label(text="Damping:")
            col.prop(cloth, "spring_damping", text="Spring")
            col.prop(cloth, "air_damping", text="Air")

            col.prop(cloth, "pin_cloth", text="Pinning")
            sub = col.column()
            sub.active = cloth.pin_cloth
            sub.prop_object(cloth, "mass_vertex_group", ob, "vertex_groups", text="")
            sub.prop(cloth, "pin_stiffness", text="Stiffness")

            col.label(text="Pre roll:")
            col.prop(cloth, "pre_roll", text="Frame")

            # Disabled for now
            """
            if cloth.mass_vertex_group:
                layout.label(text="Goal:")

                col = layout.column_flow()
                col.prop(cloth, "goal_default", text="Default")
                col.prop(cloth, "goal_spring", text="Stiffness")
                col.prop(cloth, "goal_friction", text="Friction")
            """

            key = ob.data.shape_keys

            if key:
                col.label(text="Rest Shape Key:")
                col.prop_object(cloth, "rest_shape_key", key, "keys", text="")


class PHYSICS_PT_cloth_cache(PhysicButtonsPanel):
    bl_label = "Cloth Cache"
    bl_default_closed = True

    def poll(self, context):
        return context.cloth

    def draw(self, context):
        md = context.cloth
        point_cache_ui(self, context, md.point_cache, cloth_panel_enabled(md), 0, 0)


class PHYSICS_PT_cloth_collision(PhysicButtonsPanel):
    bl_label = "Cloth Collision"
    bl_default_closed = True

    def poll(self, context):
        return context.cloth

    def draw_header(self, context):
        cloth = context.cloth.collision_settings

        self.layout.active = cloth_panel_enabled(context.cloth)
        self.layout.prop(cloth, "enable_collision", text="")

    def draw(self, context):
        layout = self.layout

        cloth = context.cloth.collision_settings
        md = context.cloth
        wide_ui = context.region.width > narrowui

        layout.active = cloth.enable_collision and cloth_panel_enabled(md)

        split = layout.split()

        col = split.column()
        col.prop(cloth, "collision_quality", slider=True, text="Quality")
        col.prop(cloth, "min_distance", slider=True, text="Distance")
        col.prop(cloth, "friction")

        if wide_ui:
            col = split.column()
        col.prop(cloth, "enable_self_collision", text="Self Collision")
        sub = col.column()
        sub.active = cloth.enable_self_collision
        sub.prop(cloth, "self_collision_quality", slider=True, text="Quality")
        sub.prop(cloth, "self_min_distance", slider=True, text="Distance")

        layout.prop(cloth, "group")


class PHYSICS_PT_cloth_stiffness(PhysicButtonsPanel):
    bl_label = "Cloth Stiffness Scaling"
    bl_default_closed = True

    def poll(self, context):
        return context.cloth

    def draw_header(self, context):
        cloth = context.cloth.settings

        self.layout.active = cloth_panel_enabled(context.cloth)
        self.layout.prop(cloth, "stiffness_scaling", text="")

    def draw(self, context):
        layout = self.layout

        md = context.cloth
        ob = context.object
        cloth = context.cloth.settings
        wide_ui = context.region.width > narrowui

        layout.active = cloth.stiffness_scaling	and cloth_panel_enabled(md)

        split = layout.split()

        col = split.column()
        col.label(text="Structural Stiffness:")
        col.prop_object(cloth, "structural_stiffness_vertex_group", ob, "vertex_groups", text="")
        col.prop(cloth, "structural_stiffness_max", text="Max")

        if wide_ui:
            col = split.column()
        col.label(text="Bending Stiffness:")
        col.prop_object(cloth, "bending_vertex_group", ob, "vertex_groups", text="")
        col.prop(cloth, "bending_stiffness_max", text="Max")


class PHYSICS_PT_cloth_field_weights(PhysicButtonsPanel):
    bl_label = "Cloth Field Weights"
    bl_default_closed = True

    def poll(self, context):
        return (context.cloth)

    def draw(self, context):
        cloth = context.cloth.settings
        effector_weights_ui(self, context, cloth.effector_weights)


classes = [
    CLOTH_MT_presets,

    PHYSICS_PT_cloth,
    PHYSICS_PT_cloth_cache,
    PHYSICS_PT_cloth_collision,
    PHYSICS_PT_cloth_stiffness,
    PHYSICS_PT_cloth_field_weights]


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
