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
from bpy.types import Panel

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

        md = context.soft_body
        ob = context.object

        softbody = md.settings

        # General
        split = layout.split()
        split.enabled = softbody_panel_enabled(md)

        col = split.column()
        col.label(text="Object:")
        col.prop(softbody, "friction")
        col.prop(softbody, "mass")
        col.prop_search(softbody, "vertex_group_mass", ob, "vertex_groups", text="Mass")

        col = split.column()
        col.label(text="Simulation:")
        col.prop(softbody, "speed")

        layout.prop(softbody, "collision_group")


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

        md = context.soft_body
        softbody = md.settings
        ob = context.object

        layout.active = softbody.use_goal and softbody_panel_enabled(md)

        split = layout.split()

        # Goal
        split = layout.split()

        col = split.column()
        col.label(text="Goal Strengths:")
        col.prop(softbody, "goal_default", text="Default")
        sub = col.column(align=True)
        sub.prop(softbody, "goal_min", text="Minimum")
        sub.prop(softbody, "goal_max", text="Maximum")

        col = split.column()
        col.label(text="Goal Settings:")
        col.prop(softbody, "goal_spring", text="Stiffness")
        col.prop(softbody, "goal_friction", text="Damping")

        layout.prop_search(softbody, "vertex_group_goal", ob, "vertex_groups", text="Vertex Group")


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

        md = context.soft_body
        softbody = md.settings
        ob = context.object

        layout.active = softbody.use_edges and softbody_panel_enabled(md)

        split = layout.split()

        col = split.column()
        col.label(text="Springs:")
        col.prop(softbody, "pull")
        col.prop(softbody, "push")
        col.prop(softbody, "damping")
        col.prop(softbody, "plastic")
        col.prop(softbody, "bend")
        col.prop(softbody, "spring_length", text="Length")
        col.prop_search(softbody, "vertex_group_spring", ob, "vertex_groups", text="Springs")

        col = split.column()
        col.prop(softbody, "use_stiff_quads")
        sub = col.column()
        sub.active = softbody.use_stiff_quads
        sub.prop(softbody, "shear")

        col.label(text="Aerodynamics:")
        col.row().prop(softbody, "aerodynamics_type", expand=True)
        col.prop(softbody, "aero", text="Factor")

        #sub = col.column()
        #sub.enabled = softbody.aero > 0

        col.label(text="Collision:")
        col.prop(softbody, "use_edge_collision", text="Edge")
        col.prop(softbody, "use_face_collision", text="Face")


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

        md = context.soft_body
        softbody = md.settings

        layout.active = softbody.use_self_collision and softbody_panel_enabled(md)

        layout.label(text="Collision Ball Size Calculation:")
        layout.row().prop(softbody, "collision_type", expand=True)

        col = layout.column(align=True)
        col.label(text="Ball:")
        col.prop(softbody, "ball_size", text="Size")
        col.prop(softbody, "ball_stiff", text="Stiffness")
        col.prop(softbody, "ball_damp", text="Dampening")


class PHYSICS_PT_softbody_solver(PhysicButtonsPanel, Panel):
    bl_label = "Solver"
    bl_parent_id = 'PHYSICS_PT_softbody'
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    def draw(self, context):
        layout = self.layout

        md = context.soft_body
        softbody = md.settings

        layout.active = softbody_panel_enabled(md)

        # Solver
        split = layout.split()

        col = split.column(align=True)
        col.label(text="Step Size:")
        col.prop(softbody, "step_min")
        col.prop(softbody, "step_max")
        col.prop(softbody, "use_auto_step", text="Auto-Step")

        col = split.column()
        col.prop(softbody, "error_threshold")
        col.label(text="Helpers:")
        col.prop(softbody, "choke")
        col.prop(softbody, "fuzzy")

        layout.label(text="Diagnostics:")
        layout.prop(softbody, "use_diagnose")
        layout.prop(softbody, "use_estimate_matrix")


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
    PHYSICS_PT_softbody_cache,
    PHYSICS_PT_softbody_goal,
    PHYSICS_PT_softbody_edge,
    PHYSICS_PT_softbody_collision,
    PHYSICS_PT_softbody_solver,
    PHYSICS_PT_softbody_field_weights,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
