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
from .properties_physics_common import (
    point_cache_ui,
    effector_weights_ui,
)

COMPAT_OB_TYPES = {'MESH', 'LATTICE', 'CURVE', 'SURFACE', 'FONT'}


def softbody_panel_enabled(md):
    return (md.point_cache.is_baked is False)


class PhysicButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "physics"

    @classmethod
    def poll(cls, context):
        ob = context.object
        return ob and ob.type in COMPAT_OB_TYPES and context.engine in cls.COMPAT_ENGINES and context.soft_body


class PHYSICS_PT_softbody(PhysicButtonsPanel, Panel):
    bl_label = "Soft Body"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        md = context.soft_body
        softbody = md.settings

        layout.prop(softbody, "collision_group")


class PHYSICS_PT_softbody_object(PhysicButtonsPanel, Panel):
    bl_label = "Object"
    bl_parent_id = 'PHYSICS_PT_softbody'
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        md = context.soft_body
        softbody = md.settings
        ob = context.object

        layout.enabled = softbody_panel_enabled(md)
        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=True)

        col = flow.column()
        col.prop(softbody, "friction")

        col.separator()

        col = flow.column()
        col.prop(softbody, "mass")

        # Note: TODO prop_search doesn't align on the right.
        row = col.row(align=True)
        row.prop_search(softbody, "vertex_group_mass", ob, "vertex_groups", text="Control Point")
        row.label(text="", icon='BLANK1')


class PHYSICS_PT_softbody_simulation(PhysicButtonsPanel, Panel):
    bl_label = "Simulation"
    bl_parent_id = 'PHYSICS_PT_softbody'
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        md = context.soft_body
        softbody = md.settings

        layout.enabled = softbody_panel_enabled(md)

        layout.prop(softbody, "speed")


class PHYSICS_PT_softbody_cache(PhysicButtonsPanel, Panel):
    bl_label = "Cache"
    bl_parent_id = 'PHYSICS_PT_softbody'
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    def draw(self, context):
        md = context.soft_body
        point_cache_ui(self, context, md.point_cache, softbody_panel_enabled(md), 'SOFTBODY')


class PHYSICS_PT_softbody_goal(PhysicButtonsPanel, Panel):
    bl_label = "Goal"
    bl_parent_id = 'PHYSICS_PT_softbody'
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    def draw_header(self, context):
        softbody = context.soft_body.settings

        self.layout.active = softbody_panel_enabled(context.soft_body)
        self.layout.prop(softbody, "use_goal", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        md = context.soft_body
        softbody = md.settings
        ob = context.object

        layout.active = softbody.use_goal and softbody_panel_enabled(md)

        # Note: TODO prop_search doesn't align on the right.
        row = layout.row(align=True)
        row.prop_search(softbody, "vertex_group_goal", ob, "vertex_groups", text="Vertex Group")
        row.label(text="", icon='BLANK1')


class PHYSICS_PT_softbody_goal_strenghts(PhysicButtonsPanel, Panel):
    bl_label = "Strengths"
    bl_parent_id = 'PHYSICS_PT_softbody_goal'
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        md = context.soft_body
        softbody = md.settings

        layout.active = softbody.use_goal and softbody_panel_enabled(md)
        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=True)

        col = flow.column()
        col.prop(softbody, "goal_default", text="Default")

        col.separator()

        col = flow.column(align=True)
        col.prop(softbody, "goal_min", text="Min")
        col.prop(softbody, "goal_max", text="Max")


class PHYSICS_PT_softbody_goal_settings(PhysicButtonsPanel, Panel):
    bl_label = "Settings"
    bl_parent_id = 'PHYSICS_PT_softbody_goal'
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        md = context.soft_body
        softbody = md.settings

        layout.active = softbody.use_goal and softbody_panel_enabled(md)
        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=True)

        col = flow.column()
        col.prop(softbody, "goal_spring", text="Stiffness")

        col = flow.column()
        col.prop(softbody, "goal_friction", text="Damping")


class PHYSICS_PT_softbody_edge(PhysicButtonsPanel, Panel):
    bl_label = "Edges"
    bl_parent_id = 'PHYSICS_PT_softbody'
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    def draw_header(self, context):
        softbody = context.soft_body.settings

        self.layout.active = softbody_panel_enabled(context.soft_body)
        self.layout.prop(softbody, "use_edges", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        md = context.soft_body
        softbody = md.settings
        ob = context.object

        layout.active = softbody.use_edges and softbody_panel_enabled(md)
        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=True)

        col = flow.column()

        # Note: TODO prop_search doesn't align on the right.
        row = col.row(align=True)
        row.prop_search(softbody, "vertex_group_spring", ob, "vertex_groups", text="Springs")
        row.label(text="", icon='BLANK1')

        col.separator()

        col.prop(softbody, "pull")
        col.prop(softbody, "push")

        col.separator()

        col = flow.column()
        col.prop(softbody, "damping")
        col.prop(softbody, "plastic")
        col.prop(softbody, "bend")

        col.separator()

        col = flow.column()
        col.prop(softbody, "spring_length", text="Length")
        col.prop(softbody, "use_edge_collision", text="Collision Edge")
        col.prop(softbody, "use_face_collision", text="Face")


class PHYSICS_PT_softbody_edge_aerodynamics(PhysicButtonsPanel, Panel):
    bl_label = "Aerodynamics"
    bl_parent_id = 'PHYSICS_PT_softbody_edge'
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=True)

        md = context.soft_body
        softbody = md.settings

        flow.active = softbody.use_edges and softbody_panel_enabled(md)

        col = flow.column()
        col.prop(softbody, "aerodynamics_type", text="Type")

        col = flow.column()
        col.prop(softbody, "aero", text="Factor")


class PHYSICS_PT_softbody_edge_stiffness(PhysicButtonsPanel, Panel):
    bl_label = "Stiffness"
    bl_parent_id = 'PHYSICS_PT_softbody_edge'
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    def draw_header(self, context):
        softbody = context.soft_body.settings

        self.layout.active = softbody_panel_enabled(context.soft_body)
        self.layout.prop(softbody, "use_stiff_quads", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        md = context.soft_body
        softbody = md.settings

        layout.active = softbody.use_edges and softbody.use_stiff_quads and softbody_panel_enabled(md)

        layout.prop(softbody, "shear")


class PHYSICS_PT_softbody_collision(PhysicButtonsPanel, Panel):
    bl_label = "Self Collision"
    bl_parent_id = 'PHYSICS_PT_softbody'
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    def draw_header(self, context):
        softbody = context.soft_body.settings

        self.layout.active = softbody_panel_enabled(context.soft_body)
        self.layout.prop(softbody, "use_self_collision", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        md = context.soft_body
        softbody = md.settings

        layout.active = softbody.use_self_collision and softbody_panel_enabled(md)

        layout.prop(softbody, "collision_type", text="Calculation Type")

        layout.separator()

        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=True)

        col = flow.column(align=True)
        col.prop(softbody, "ball_size", text="Ball Size")

        col = flow.column(align=True)
        col.prop(softbody, "ball_stiff", text="Stiffness")
        col.prop(softbody, "ball_damp", text="Dampening")


class PHYSICS_PT_softbody_solver(PhysicButtonsPanel, Panel):
    bl_label = "Solver"
    bl_parent_id = 'PHYSICS_PT_softbody'
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        md = context.soft_body
        softbody = md.settings

        layout.active = softbody_panel_enabled(md)
        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=True)

        col = flow.column(align=True)
        col.prop(softbody, "step_min", text="Step Size Min")
        col.prop(softbody, "step_max", text="Max")

        col = flow.column()
        col.prop(softbody, "use_auto_step", text="Auto-Step")
        col.prop(softbody, "error_threshold")


class PHYSICS_PT_softbody_solver_diagnostics(PhysicButtonsPanel, Panel):
    bl_label = "Diagnostics"
    bl_parent_id = 'PHYSICS_PT_softbody_solver'
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        md = context.soft_body
        softbody = md.settings

        layout.active = softbody_panel_enabled(md)

        layout.prop(softbody, "use_diagnose")
        layout.prop(softbody, "use_estimate_matrix")


class PHYSICS_PT_softbody_solver_helpers(PhysicButtonsPanel, Panel):
    bl_label = "Helpers"
    bl_parent_id = 'PHYSICS_PT_softbody_solver'
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        md = context.soft_body
        softbody = md.settings

        layout.active = softbody_panel_enabled(md)
        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=True)

        col = flow.column()
        col.prop(softbody, "choke")

        col = flow.column()
        col.prop(softbody, "fuzzy")


class PHYSICS_PT_softbody_field_weights(PhysicButtonsPanel, Panel):
    bl_label = "Field Weights"
    bl_parent_id = 'PHYSICS_PT_softbody'
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    def draw(self, context):
        md = context.soft_body
        softbody = md.settings

        effector_weights_ui(self, context, softbody.effector_weights, 'SOFTBODY')


classes = (
    PHYSICS_PT_softbody,
    PHYSICS_PT_softbody_object,
    PHYSICS_PT_softbody_simulation,
    PHYSICS_PT_softbody_cache,
    PHYSICS_PT_softbody_goal,
    PHYSICS_PT_softbody_goal_settings,
    PHYSICS_PT_softbody_goal_strenghts,
    PHYSICS_PT_softbody_edge,
    PHYSICS_PT_softbody_edge_aerodynamics,
    PHYSICS_PT_softbody_edge_stiffness,
    PHYSICS_PT_softbody_collision,
    PHYSICS_PT_softbody_solver,
    PHYSICS_PT_softbody_solver_diagnostics,
    PHYSICS_PT_softbody_solver_helpers,
    PHYSICS_PT_softbody_field_weights,
)


if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
