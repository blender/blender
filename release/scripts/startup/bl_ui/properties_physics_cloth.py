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
from bpy.types import Menu, Panel

from bl_ui.properties_physics_common import (
    point_cache_ui,
    effector_weights_ui,
    )


def cloth_panel_enabled(md):
    return md.point_cache.is_baked is False


class CLOTH_MT_presets(Menu):
    bl_label = "Cloth Presets"
    preset_subdir = "cloth"
    preset_operator = "script.execute_preset"
    draw = Menu.draw_preset


class PhysicButtonsPanel():
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "physics"

    @classmethod
    def poll(cls, context):
        ob = context.object
        rd = context.scene.render
        return (ob and ob.type == 'MESH') and (not rd.use_game_engine) and (context.cloth)


class PHYSICS_PT_cloth(PhysicButtonsPanel, Panel):
    bl_label = "Cloth"

    def draw(self, context):
        layout = self.layout

        md = context.cloth
        ob = context.object
        cloth = md.settings

        split = layout.split()

        split.active = cloth_panel_enabled(md)

        col = split.column()

        col.label(text="Presets:")
        sub = col.row(align=True)
        sub.menu("CLOTH_MT_presets", text=bpy.types.CLOTH_MT_presets.bl_label)
        sub.operator("cloth.preset_add", text="", icon='ZOOMIN')
        sub.operator("cloth.preset_add", text="", icon='ZOOMOUT').remove_active = True

        col.label(text="Quality:")
        col.prop(cloth, "quality", text="Steps", slider=True)

        col.label(text="Material:")
        col.prop(cloth, "mass")
        col.prop(cloth, "structural_stiffness", text="Structural")
        col.prop(cloth, "bending_stiffness", text="Bending")

        col = split.column()

        col.label(text="Damping:")
        col.prop(cloth, "spring_damping", text="Spring")
        col.prop(cloth, "air_damping", text="Air")
        col.prop(cloth, "vel_damping", text="Velocity")

        col.prop(cloth, "use_pin_cloth", text="Pinning")
        sub = col.column()
        sub.active = cloth.use_pin_cloth
        sub.prop_search(cloth, "vertex_group_mass", ob, "vertex_groups", text="")
        sub.prop(cloth, "pin_stiffness", text="Stiffness")

        col.label(text="Pre roll:")
        col.prop(cloth, "pre_roll", text="Frame")

        # Disabled for now
        """
        if cloth.vertex_group_mass:
            layout.label(text="Goal:")

            col = layout.column_flow()
            col.prop(cloth, "goal_default", text="Default")
            col.prop(cloth, "goal_spring", text="Stiffness")
            col.prop(cloth, "goal_friction", text="Friction")
        """

        key = ob.data.shape_keys

        if key:
            col.label(text="Rest Shape Key:")
            col.prop_search(cloth, "rest_shape_key", key, "key_blocks", text="")


class PHYSICS_PT_cloth_cache(PhysicButtonsPanel, Panel):
    bl_label = "Cloth Cache"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        return context.cloth

    def draw(self, context):
        md = context.cloth
        point_cache_ui(self, context, md.point_cache, cloth_panel_enabled(md), 'CLOTH')


class PHYSICS_PT_cloth_collision(PhysicButtonsPanel, Panel):
    bl_label = "Cloth Collision"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        return context.cloth

    def draw_header(self, context):
        cloth = context.cloth.collision_settings

        self.layout.active = cloth_panel_enabled(context.cloth)
        self.layout.prop(cloth, "use_collision", text="")

    def draw(self, context):
        layout = self.layout

        cloth = context.cloth.collision_settings
        md = context.cloth

        layout.active = cloth.use_collision and cloth_panel_enabled(md)

        split = layout.split()

        col = split.column()
        col.prop(cloth, "collision_quality", slider=True, text="Quality")
        col.prop(cloth, "distance_min", slider=True, text="Distance")
        col.prop(cloth, "repel_force", slider=True, text="Repel")
        col.prop(cloth, "distance_repel", slider=True, text="Repel Distance")
        col.prop(cloth, "friction")

        col = split.column()
        col.prop(cloth, "use_self_collision", text="Self Collision")
        sub = col.column()
        sub.active = cloth.use_self_collision
        sub.prop(cloth, "self_collision_quality", slider=True, text="Quality")
        sub.prop(cloth, "self_distance_min", slider=True, text="Distance")

        layout.prop(cloth, "group")


class PHYSICS_PT_cloth_stiffness(PhysicButtonsPanel, Panel):
    bl_label = "Cloth Stiffness Scaling"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        return context.cloth

    def draw_header(self, context):
        cloth = context.cloth.settings

        self.layout.active = cloth_panel_enabled(context.cloth)
        self.layout.prop(cloth, "use_stiffness_scale", text="")

    def draw(self, context):
        layout = self.layout

        md = context.cloth
        ob = context.object
        cloth = context.cloth.settings

        layout.active = cloth.use_stiffness_scale	and cloth_panel_enabled(md)

        split = layout.split()

        col = split.column()
        col.label(text="Structural Stiffness:")
        col.prop_search(cloth, "vertex_group_structural_stiffness", ob, "vertex_groups", text="")
        col.prop(cloth, "structural_stiffness_max", text="Max")

        col = split.column()
        col.label(text="Bending Stiffness:")
        col.prop_search(cloth, "vertex_group_bending", ob, "vertex_groups", text="")
        col.prop(cloth, "bending_stiffness_max", text="Max")


class PHYSICS_PT_cloth_field_weights(PhysicButtonsPanel, Panel):
    bl_label = "Cloth Field Weights"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        return (context.cloth)

    def draw(self, context):
        cloth = context.cloth.settings
        effector_weights_ui(self, context, cloth.effector_weights)

if __name__ == "__main__":  # only for live edit.
    bpy.utils.register_module(__name__)
