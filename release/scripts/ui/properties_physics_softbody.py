
import bpy

from properties_physics_common import point_cache_ui
from properties_physics_common import effector_weights_ui

def softbody_panel_enabled(md):
    return md.point_cache.baked==False

class PhysicButtonsPanel(bpy.types.Panel):
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "physics"

    def poll(self, context):
        ob = context.object
        rd = context.scene.render_data
        return (ob and ob.type == 'MESH') and (not rd.use_game_engine)

class PHYSICS_PT_softbody(PhysicButtonsPanel):
    bl_label = "Soft Body"

    def draw(self, context):
        layout = self.layout

        md = context.soft_body
        ob = context.object

        split = layout.split()
        split.operator_context = "EXEC_DEFAULT"

        if md:
            # remove modifier + settings
            split.set_context_pointer("modifier", md)
            split.itemO("object.modifier_remove", text="Remove")

            row = split.row(align=True)
            row.itemR(md, "render", text="")
            row.itemR(md, "realtime", text="")
        else:
            # add modifier
            split.item_enumO("object.modifier_add", "type", 'SOFT_BODY', text="Add")
            split.itemL("")

        if md:
            softbody = md.settings

            # General
            split = layout.split()
            split.enabled = softbody_panel_enabled(md)

            col = split.column()
            col.itemL(text="Object:")
            col.itemR(softbody, "mass")
            col.itemR(softbody, "friction")

            col = split.column()
            col.itemL(text="Simulation:")
            col.itemR(softbody, "speed")

class PHYSICS_PT_softbody_cache(PhysicButtonsPanel):
    bl_label = "Soft Body Cache"
    bl_default_closed = True

    def poll(self, context):
        return context.soft_body

    def draw(self, context):
        md = context.soft_body
        point_cache_ui(self, md.point_cache, softbody_panel_enabled(md), 0, 0)

class PHYSICS_PT_softbody_goal(PhysicButtonsPanel):
    bl_label = "Soft Body Goal"
    bl_default_closed = True

    def poll(self, context):
        return context.soft_body

    def draw_header(self, context):
        softbody = context.soft_body.settings

        self.layout.active = softbody_panel_enabled(context.soft_body)
        self.layout.itemR(softbody, "use_goal", text="")

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
        col.itemL(text="Goal Strengths:")
        col.itemR(softbody, "goal_default", text="Default")
        sub = col.column(align=True)
        sub.itemR(softbody, "goal_min", text="Minimum")
        sub.itemR(softbody, "goal_max", text="Maximum")

        col = split.column()
        col.itemL(text="Goal Settings:")
        col.itemR(softbody, "goal_spring", text="Stiffness")
        col.itemR(softbody, "goal_friction", text="Damping")

        layout.item_pointerR(softbody, "goal_vertex_group", ob, "vertex_groups", text="Vertex Group")

class PHYSICS_PT_softbody_edge(PhysicButtonsPanel):
    bl_label = "Soft Body Edges"
    bl_default_closed = True

    def poll(self, context):
        return context.soft_body

    def draw_header(self, context):
        softbody = context.soft_body.settings

        self.layout.active = softbody_panel_enabled(context.soft_body)
        self.layout.itemR(softbody, "use_edges", text="")

    def draw(self, context):
        layout = self.layout

        md = context.soft_body
        softbody = md.settings
        ob = context.object

        layout.active = softbody.use_edges and softbody_panel_enabled(md)

        split = layout.split()

        col = split.column()
        col.itemL(text="Springs:")
        col.itemR(softbody, "pull")
        col.itemR(softbody, "push")
        col.itemR(softbody, "damp")
        col.itemR(softbody, "plastic")
        col.itemR(softbody, "bending")
        col.itemR(softbody, "spring_length", text="Length")

        col = split.column()
        col.itemR(softbody, "stiff_quads")
        sub = col.column()
        sub.active = softbody.stiff_quads
        sub.itemR(softbody, "shear")

        col.itemR(softbody, "new_aero", text="Aero")
        sub = col.column()
        sub.enabled = softbody.new_aero
        sub.itemR(softbody, "aero", text="Factor")

        col.itemL(text="Collision:")
        col.itemR(softbody, "edge_collision", text="Edge")
        col.itemR(softbody, "face_collision", text="Face")

class PHYSICS_PT_softbody_collision(PhysicButtonsPanel):
    bl_label = "Soft Body Collision"
    bl_default_closed = True

    def poll(self, context):
        return context.soft_body

    def draw_header(self, context):
        softbody = context.soft_body.settings

        self.layout.active = softbody_panel_enabled(context.soft_body)
        self.layout.itemR(softbody, "self_collision", text="")

    def draw(self, context):
        layout = self.layout

        md = context.soft_body
        softbody = md.settings
        ob = context.object

        layout.active = softbody.self_collision and softbody_panel_enabled(md)

        layout.itemL(text="Collision Type:")
        layout.itemR(softbody, "collision_type", expand=True)

        col = layout.column(align=True)
        col.itemL(text="Ball:")
        col.itemR(softbody, "ball_size", text="Size")
        col.itemR(softbody, "ball_stiff", text="Stiffness")
        col.itemR(softbody, "ball_damp", text="Dampening")

class PHYSICS_PT_softbody_solver(PhysicButtonsPanel):
    bl_label = "Soft Body Solver"
    bl_default_closed = True

    def poll(self, context):
        return context.soft_body

    def draw(self, context):
        layout = self.layout

        md = context.soft_body
        softbody = md.settings
        ob = context.object

        layout.active = softbody_panel_enabled(md)

        # Solver
        split = layout.split()

        col = split.column(align=True)
        col.itemL(text="Step Size:")
        col.itemR(softbody, "minstep")
        col.itemR(softbody, "maxstep")
        col.itemR(softbody, "auto_step", text="Auto-Step")

        col = split.column()
        col.itemR(softbody, "error_limit")
        col.itemL(text="Helpers:")
        col.itemR(softbody, "choke")
        col.itemR(softbody, "fuzzy")

        layout.itemL(text="Diagnostics:")
        layout.itemR(softbody, "diagnose")

class PHYSICS_PT_softbody_field_weights(PhysicButtonsPanel):
    bl_label = "Soft Body Field Weights"
    bl_default_closed = True

    def poll(self, context):
        return (context.soft_body)

    def draw(self, context):
        md = context.soft_body
        softbody = md.settings
        effector_weights_ui(self, softbody.effector_weights)

bpy.types.register(PHYSICS_PT_softbody)
bpy.types.register(PHYSICS_PT_softbody_cache)
bpy.types.register(PHYSICS_PT_softbody_goal)
bpy.types.register(PHYSICS_PT_softbody_edge)
bpy.types.register(PHYSICS_PT_softbody_collision)
bpy.types.register(PHYSICS_PT_softbody_solver)
bpy.types.register(PHYSICS_PT_softbody_field_weights)
