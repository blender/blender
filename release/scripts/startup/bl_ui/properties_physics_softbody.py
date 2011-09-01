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
from blf import gettext as _

from bl_ui.properties_physics_common import (
    point_cache_ui,
    effector_weights_ui,
    )


def softbody_panel_enabled(md):
    return (md.point_cache.is_baked is False)


class PhysicButtonsPanel():
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "physics"

    @classmethod
    def poll(cls, context):
        ob = context.object
        rd = context.scene.render
#        return (ob and ob.type == 'MESH') and (not rd.use_game_engine)
# i really hate touching things i do not understand completely .. but i think this should read (bjornmose)
        return (ob and (ob.type == 'MESH' or ob.type == 'LATTICE'or ob.type == 'CURVE')) and (not rd.use_game_engine) and (context.soft_body)


class PHYSICS_PT_softbody(PhysicButtonsPanel, Panel):
    bl_label = _("Soft Body")

    def draw(self, context):
        layout = self.layout

        md = context.soft_body
        ob = context.object

        if md:
            softbody = md.settings

            # General
            split = layout.split()
            split.enabled = softbody_panel_enabled(md)

            col = split.column()
            col.label(text=_("Object:"))
            col.prop(softbody, "friction")
            col.prop(softbody, "mass")
            col.prop_search(softbody, "vertex_group_mass", ob, "vertex_groups", text=_("Mass:"))

            col = split.column()
            col.label(text=_("Simulation:"))
            col.prop(softbody, "speed")


class PHYSICS_PT_softbody_cache(PhysicButtonsPanel, Panel):
    bl_label = _("Soft Body Cache")
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        return context.soft_body

    def draw(self, context):
        md = context.soft_body
        point_cache_ui(self, context, md.point_cache, softbody_panel_enabled(md), 'SOFTBODY')


class PHYSICS_PT_softbody_goal(PhysicButtonsPanel, Panel):
    bl_label = _("Soft Body Goal")
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        return context.soft_body

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
        col.label(text=_("Goal Strengths:"))
        col.prop(softbody, "goal_default", text=_("Default"))
        sub = col.column(align=True)
        sub.prop(softbody, "goal_min", text=_("Minimum"))
        sub.prop(softbody, "goal_max", text=_("Maximum"))

        col = split.column()
        col.label(text=_("Goal Settings:"))
        col.prop(softbody, "goal_spring", text=_("Stiffness"))
        col.prop(softbody, "goal_friction", text=_("Damping"))

        layout.prop_search(softbody, "vertex_group_goal", ob, "vertex_groups", text=_("Vertex Group"))


class PHYSICS_PT_softbody_edge(PhysicButtonsPanel, Panel):
    bl_label = _("Soft Body Edges")
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        return context.soft_body

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
        col.label(text=_("Springs:"))
        col.prop(softbody, "pull")
        col.prop(softbody, "push")
        col.prop(softbody, "damping")
        col.prop(softbody, "plastic")
        col.prop(softbody, "bend")
        col.prop(softbody, "spring_length", text=_("Length"))
        col.prop_search(softbody, "vertex_group_spring", ob, "vertex_groups", text=_("Springs:"))

        col = split.column()
        col.prop(softbody, "use_stiff_quads")
        sub = col.column()
        sub.active = softbody.use_stiff_quads
        sub.prop(softbody, "shear")

        col.label(text=_("Aerodynamics:"))
        col.row().prop(softbody, "aerodynamics_type", expand=True)
        col.prop(softbody, "aero", text=_("Factor"))

        #sub = col.column()
        #sub.enabled = softbody.aero > 0

        col.label(text=_("Collision:"))
        col.prop(softbody, "use_edge_collision", text=_("Edge"))
        col.prop(softbody, "use_face_collision", text=_("Face"))


class PHYSICS_PT_softbody_collision(PhysicButtonsPanel, Panel):
    bl_label = _("Soft Body Self Collision")
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        return context.soft_body

    def draw_header(self, context):
        softbody = context.soft_body.settings

        self.layout.active = softbody_panel_enabled(context.soft_body)
        self.layout.prop(softbody, "use_self_collision", text="")

    def draw(self, context):
        layout = self.layout

        md = context.soft_body
        softbody = md.settings

        layout.active = softbody.use_self_collision and softbody_panel_enabled(md)

        layout.label(text=_("Collision Ball Size Calculation:"))
        layout.prop(softbody, "collision_type", expand=True)

        col = layout.column(align=True)
        col.label(text=_("Ball:"))
        col.prop(softbody, "ball_size", text=_("Size"))
        col.prop(softbody, "ball_stiff", text=_("Stiffness"))
        col.prop(softbody, "ball_damp", text=_("Dampening"))


class PHYSICS_PT_softbody_solver(PhysicButtonsPanel, Panel):
    bl_label = _("Soft Body Solver")
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        return context.soft_body

    def draw(self, context):
        layout = self.layout

        md = context.soft_body
        softbody = md.settings

        layout.active = softbody_panel_enabled(md)

        # Solver
        split = layout.split()

        col = split.column(align=True)
        col.label(text=_("Step Size:"))
        col.prop(softbody, "step_min")
        col.prop(softbody, "step_max")
        col.prop(softbody, "use_auto_step", text=_("Auto-Step"))

        col = split.column()
        col.prop(softbody, "error_threshold")
        col.label(text=_("Helpers:"))
        col.prop(softbody, "choke")
        col.prop(softbody, "fuzzy")

        layout.label(text=_("Diagnostics:"))
        layout.prop(softbody, "use_diagnose")
        layout.prop(softbody, "use_estimate_matrix")


class PHYSICS_PT_softbody_field_weights(PhysicButtonsPanel, Panel):
    bl_label = _("Soft Body Field Weights")
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        return (context.soft_body)

    def draw(self, context):
        md = context.soft_body
        softbody = md.settings

        effector_weights_ui(self, context, softbody.effector_weights)

if __name__ == "__main__":  # only for live edit.
    bpy.utils.register_module(__name__)
