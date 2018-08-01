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
from bpy.types import (
    Panel,
)
from bl_operators.presets import PresetMenu

from .properties_physics_common import (
    point_cache_ui,
    effector_weights_ui,
)


def cloth_panel_enabled(md):
    return md.point_cache.is_baked is False


class CLOTH_PT_presets(PresetMenu):
    bl_label = "Cloth Presets"
    preset_subdir = "cloth"
    preset_operator = "script.execute_preset"
    preset_add_operator = "cloth.preset_add"


class PhysicButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "physics"

    @classmethod
    def poll(cls, context):
        ob = context.object
        return (ob and ob.type == 'MESH') and (context.engine in cls.COMPAT_ENGINES) and (context.cloth)


class PHYSICS_PT_cloth(PhysicButtonsPanel, Panel):
    bl_label = "Cloth"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    def draw_header_preset(self, context):
        CLOTH_PT_presets.draw_panel_header(self.layout)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        md = context.cloth
        ob = context.object
        cloth = md.settings

        layout.active = cloth_panel_enabled(md)

        flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=True)

        col = flow.column()
        col.prop(cloth, "quality", text="Quality Steps")
        col.prop(cloth, "time_scale", text="Speed Multiplier")

        col.separator()

        col = flow.column()
        col.prop(cloth, "mass", text="Material Mass")
        col.prop(cloth, "structural_stiffness", text="Structural")
        col.prop(cloth, "bending_stiffness", text="Bending")

        col.separator()

        col = flow.column()
        col.prop(cloth, "spring_damping", text="Damping Spring")
        col.prop(cloth, "air_damping", text="Air")
        col.prop(cloth, "vel_damping", text="Velocity")

        col = flow.column()
        col.prop(cloth, "use_dynamic_mesh", text="Dynamic Mesh")

        key = ob.data.shape_keys

        if key:
            # Note: TODO prop_search doesn't align on the right.
            row = col.row(align=True)
            row.active = not cloth.use_dynamic_mesh
            row.prop_search(cloth, "rest_shape_key", key, "key_blocks", text="Rest Shape Key")
            row.label(text="", icon='BLANK1')


class PHYSICS_PT_cloth_pinning(PhysicButtonsPanel, Panel):
    bl_label = "Pinning"
    bl_parent_id = 'PHYSICS_PT_cloth'
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    def draw_header(self, context):
        md = context.cloth
        cloth = md.settings

        self.layout.active = cloth_panel_enabled(md) and cloth.use_pin_cloth
        self.layout.prop(cloth, "use_pin_cloth", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        md = context.cloth
        ob = context.object
        cloth = md.settings

        layout.active = cloth_panel_enabled(md) and cloth.use_pin_cloth

        flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=True)

        col = flow.column()

        # Note: TODO prop_search doesn't align on the right.
        row = col.row(align=True)
        row.prop_search(cloth, "vertex_group_mass", ob, "vertex_groups", text="Mass Group")
        row.label(text="", icon='BLANK1')

        col = flow.column()
        col.prop(cloth, "pin_stiffness", text="Stiffness")

        # Disabled for now.
        """
        if cloth.vertex_group_mass:
            col = flow.column()
            col.prop(cloth, "goal_default", text="Goal Default")
            col.prop(cloth, "goal_spring", text="Stiffness")
            col.prop(cloth, "goal_friction", text="Friction")
        """


class PHYSICS_PT_cloth_cache(PhysicButtonsPanel, Panel):
    bl_label = "Cache"
    bl_parent_id = 'PHYSICS_PT_cloth'
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    def draw(self, context):
        md = context.cloth
        point_cache_ui(self, context, md.point_cache, cloth_panel_enabled(md), 'CLOTH')


class PHYSICS_PT_cloth_collision(PhysicButtonsPanel, Panel):
    bl_label = "Collision"
    bl_parent_id = 'PHYSICS_PT_cloth'
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    def draw_header(self, context):
        cloth = context.cloth.collision_settings

        self.layout.active = cloth_panel_enabled(context.cloth)
        self.layout.prop(cloth, "use_collision", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        cloth = context.cloth.collision_settings
        md = context.cloth

        layout.active = cloth.use_collision and cloth_panel_enabled(md)

        flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=True)

        col = flow.column()
        col.prop(cloth, "collision_quality", text="Quality")
        col.prop(cloth, "distance_min", slider=True, text="Distance")
        col.prop(cloth, "repel_force", slider=True, text="Repel")

        col = flow.column()
        col.prop(cloth, "distance_repel", slider=True, text="Repel Distance")
        col.prop(cloth, "friction")
        col.prop(cloth, "group")


class PHYSICS_PT_cloth_self_collision(PhysicButtonsPanel, Panel):
    bl_label = "Self Collision"
    bl_parent_id = 'PHYSICS_PT_cloth_collision'
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    def draw_header(self, context):
        cloth = context.cloth.collision_settings

        self.layout.active = cloth_panel_enabled(context.cloth) and cloth.use_self_collision
        self.layout.prop(cloth, "use_self_collision", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        cloth = context.cloth.collision_settings
        md = context.cloth
        ob = context.object

        layout.active = cloth.use_collision and cloth_panel_enabled(md) and cloth.use_self_collision

        flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=True)

        col = flow.column()
        col.prop(cloth, "self_collision_quality", text="Quality")
        col.prop(cloth, "self_distance_min", slider=True, text="Distance")

        col = flow.column()
        # Note: TODO prop_search doesn't align on the right.
        row = col.row(align=True)
        row.prop_search(cloth, "vertex_group_self_collisions", ob, "vertex_groups", text="Vertex Group")
        row.label(text="", icon='BLANK1')


class PHYSICS_PT_cloth_stiffness(PhysicButtonsPanel, Panel):
    bl_label = "Stiffness Scaling"
    bl_parent_id = 'PHYSICS_PT_cloth'
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    def draw_header(self, context):
        cloth = context.cloth.settings

        self.layout.active = cloth_panel_enabled(context.cloth)
        self.layout.prop(cloth, "use_stiffness_scale", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        md = context.cloth
        ob = context.object
        cloth = context.cloth.settings

        layout.active = (cloth.use_stiffness_scale and cloth_panel_enabled(md))

        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=True)

        col = flow.column()
        # Note: TODO prop_search doesn't align on the right.
        row = col.row(align=True)
        row.prop_search(
            cloth, "vertex_group_structural_stiffness", ob, "vertex_groups",
            text="Structural Group"
        )
        row.label(text="", icon='BLANK1')
        col.prop(cloth, "structural_stiffness_max", text="Max")

        col.separator()

        col = flow.column()
        row = col.row(align=True)
        row.prop_search(
            cloth, "vertex_group_bending", ob, "vertex_groups",
            text="Bending Group"
        )
        row.label(text="", icon='BLANK1')
        col.prop(cloth, "bending_stiffness_max", text="Max")


class PHYSICS_PT_cloth_sewing(PhysicButtonsPanel, Panel):
    bl_label = "Sewing Springs"
    bl_parent_id = 'PHYSICS_PT_cloth'
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    def draw_header(self, context):
        cloth = context.cloth.settings

        self.layout.active = cloth_panel_enabled(context.cloth)
        self.layout.prop(cloth, "use_sewing_springs", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        md = context.cloth
        ob = context.object
        cloth = context.cloth.settings

        layout.active = (cloth.use_sewing_springs and cloth_panel_enabled(md))
        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=True)

        col = flow.column()
        col.prop(cloth, "sewing_force_max", text="Sewing Force")

        col.separator()

        col = col.column()
        # Note: TODO prop_search doesn't align on the right.
        row = col.row(align=True)
        row.prop_search(cloth, "vertex_group_shrink", ob, "vertex_groups", text="Shrinking Group")
        row.label(text="", icon='BLANK1')

        col = flow.column(align=True)
        col.prop(cloth, "shrink_min", text="Min")
        col.prop(cloth, "shrink_max", text="Max")


class PHYSICS_PT_cloth_field_weights(PhysicButtonsPanel, Panel):
    bl_label = "Field Weights"
    bl_parent_id = 'PHYSICS_PT_cloth'
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    def draw(self, context):
        cloth = context.cloth.settings
        effector_weights_ui(self, context, cloth.effector_weights, 'CLOTH')


classes = (
    CLOTH_PT_presets,
    PHYSICS_PT_cloth,
    PHYSICS_PT_cloth_cache,
    PHYSICS_PT_cloth_collision,
    PHYSICS_PT_cloth_self_collision,
    PHYSICS_PT_cloth_pinning,
    PHYSICS_PT_cloth_stiffness,
    PHYSICS_PT_cloth_sewing,
    PHYSICS_PT_cloth_field_weights,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
