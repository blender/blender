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


from properties_physics_common import point_cache_ui
from properties_physics_common import effector_weights_ui


def softbody_panel_enabled(md):
    return (md.point_cache.baked is False)


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
        return (ob and (ob.type == 'MESH' or ob.type == 'LATTICE'or ob.type == 'CURVE')) and (not rd.use_game_engine)


class PHYSICS_PT_softbody(PhysicButtonsPanel, bpy.types.Panel):
    bl_label = "Soft Body"

    def draw(self, context):
        layout = self.layout

        md = context.soft_body
        ob = context.object

        split = layout.split()

        if md:
            # remove modifier + settings
            split.set_context_pointer("modifier", md)
            split.operator("object.modifier_remove", text="Remove")

            row = split.row(align=True)
            row.prop(md, "render", text="")
            row.prop(md, "realtime", text="")
        else:
            # add modifier
            split.operator("object.modifier_add", text="Add").type = 'SOFT_BODY'
            split.column()

        if md:
            softbody = md.settings

            # General
            split = layout.split()
            split.enabled = softbody_panel_enabled(md)

            col = split.column()
            col.label(text="Object:")
            col.prop(softbody, "friction")
            col.prop(softbody, "mass")
            col.prop_object(softbody, "mass_vertex_group", ob, "vertex_groups", text="Mass:")

            col = split.column()
            col.label(text="Simulation:")
            col.prop(softbody, "speed")


class PHYSICS_PT_softbody_cache(PhysicButtonsPanel, bpy.types.Panel):
    bl_label = "Soft Body Cache"
    bl_default_closed = True

    @classmethod
    def poll(cls, context):
        return context.soft_body

    def draw(self, context):
        md = context.soft_body
        point_cache_ui(self, context, md.point_cache, softbody_panel_enabled(md), 'SOFTBODY')


class PHYSICS_PT_softbody_goal(PhysicButtonsPanel, bpy.types.Panel):
    bl_label = "Soft Body Goal"
    bl_default_closed = True

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
        col.label(text="Goal Strengths:")
        col.prop(softbody, "goal_default", text="Default")
        sub = col.column(align=True)
        sub.prop(softbody, "goal_min", text="Minimum")
        sub.prop(softbody, "goal_max", text="Maximum")

        col = split.column()
        col.label(text="Goal Settings:")
        col.prop(softbody, "goal_spring", text="Stiffness")
        col.prop(softbody, "goal_friction", text="Damping")

        layout.prop_object(softbody, "goal_vertex_group", ob, "vertex_groups", text="Vertex Group")


class PHYSICS_PT_softbody_edge(PhysicButtonsPanel, bpy.types.Panel):
    bl_label = "Soft Body Edges"
    bl_default_closed = True

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
        col.label(text="Springs:")
        col.prop(softbody, "pull")
        col.prop(softbody, "push")
        col.prop(softbody, "damp")
        col.prop(softbody, "plastic")
        col.prop(softbody, "bending")
        col.prop(softbody, "spring_length", text="Length")
        col.prop_object(softbody, "spring_vertex_group", ob, "vertex_groups", text="Springs:")

        col = split.column()
        col.prop(softbody, "stiff_quads")
        sub = col.column()
        sub.active = softbody.stiff_quads
        sub.prop(softbody, "shear")

        col.label(text="Aerodynamics:")
        col.row().prop(softbody, "aerodynamics_type", expand=True)
        col.prop(softbody, "aero", text="Factor")

        #sub = col.column()
        #sub.enabled = softbody.aero > 0


        col.label(text="Collision:")
        col.prop(softbody, "edge_collision", text="Edge")
        col.prop(softbody, "face_collision", text="Face")


class PHYSICS_PT_softbody_collision(PhysicButtonsPanel, bpy.types.Panel):
    bl_label = "Soft Body Self Collision"
    bl_default_closed = True

    @classmethod
    def poll(cls, context):
        return context.soft_body

    def draw_header(self, context):
        softbody = context.soft_body.settings

        self.layout.active = softbody_panel_enabled(context.soft_body)
        self.layout.prop(softbody, "self_collision", text="")

    def draw(self, context):
        layout = self.layout

        md = context.soft_body
        softbody = md.settings

        layout.active = softbody.self_collision and softbody_panel_enabled(md)

        layout.label(text="Collision Ball Size Calculation:")
        layout.prop(softbody, "collision_type", expand=True)

        col = layout.column(align=True)
        col.label(text="Ball:")
        col.prop(softbody, "ball_size", text="Size")
        col.prop(softbody, "ball_stiff", text="Stiffness")
        col.prop(softbody, "ball_damp", text="Dampening")


class PHYSICS_PT_softbody_solver(PhysicButtonsPanel, bpy.types.Panel):
    bl_label = "Soft Body Solver"
    bl_default_closed = True

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
        col.label(text="Step Size:")
        col.prop(softbody, "minstep")
        col.prop(softbody, "maxstep")
        col.prop(softbody, "auto_step", text="Auto-Step")

        col = split.column()
        col.prop(softbody, "error_limit")
        col.label(text="Helpers:")
        col.prop(softbody, "choke")
        col.prop(softbody, "fuzzy")

        layout.label(text="Diagnostics:")
        layout.prop(softbody, "diagnose")
        layout.prop(softbody, "estimate_matrix")


class PHYSICS_PT_softbody_field_weights(PhysicButtonsPanel, bpy.types.Panel):
    bl_label = "Soft Body Field Weights"
    bl_default_closed = True

    @classmethod
    def poll(cls, context):
        return (context.soft_body)

    def draw(self, context):
        md = context.soft_body
        softbody = md.settings

        effector_weights_ui(self, context, softbody.effector_weights)


def register():
    pass


def unregister():
    pass

if __name__ == "__main__":
    register()
