
import bpy

from properties_physics_common import point_cache_ui
from properties_physics_common import effector_weights_ui

def cloth_panel_enabled(md):
    return md.point_cache.baked==False

class PhysicButtonsPanel(bpy.types.Panel):
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "physics"

    def poll(self, context):
        ob = context.object
        rd = context.scene.render_data
        return (ob and ob.type == 'MESH') and (not rd.use_game_engine)

class PHYSICS_PT_cloth(PhysicButtonsPanel):
    bl_label = "Cloth"

    def draw(self, context):
        layout = self.layout

        md = context.cloth
        ob = context.object

        split = layout.split()
        split.operator_context = 'EXEC_DEFAULT'

        if md:
            # remove modifier + settings
            split.set_context_pointer("modifier", md)
            split.itemO("object.modifier_remove", text="Remove")

            row = split.row(align=True)
            row.itemR(md, "render", text="")
            row.itemR(md, "realtime", text="")
        else:
            # add modifier
            split.item_enumO("object.modifier_add", "type", 'CLOTH', text="Add")
            split.itemL()

        if md:
            cloth = md.settings

            layout.active = cloth_panel_enabled(md)

            split = layout.split()

            col = split.column()

            col.itemL(text="Quality:")
            col.itemR(cloth, "quality", text="Steps",slider=True)

            col.itemL(text="Material:")
            sub = col.column(align=True)
            sub.itemR(cloth, "mass")
            sub.itemR(cloth, "structural_stiffness", text="Structural")
            sub.itemR(cloth, "bending_stiffness", text="Bending")

            col = split.column()

            col.itemL(text="Presets:")
            col.itemL(text="TODO!")

            col.itemL(text="Damping:")
            sub = col.column(align=True)
            sub.itemR(cloth, "spring_damping", text="Spring")
            sub.itemR(cloth, "air_damping", text="Air")

            col.itemR(cloth, "pin_cloth", text="Pin")
            sub = col.column(align=True)
            sub.active = cloth.pin_cloth
            sub.itemR(cloth, "pin_stiffness", text="Stiffness")
            sub.item_pointerR(cloth, "mass_vertex_group", ob, "vertex_groups", text="")

            # Disabled for now
            """
            if cloth.mass_vertex_group:
                layout.itemL(text="Goal:")

                col = layout.column_flow()
                col.itemR(cloth, "goal_default", text="Default")
                col.itemR(cloth, "goal_spring", text="Stiffness")
                col.itemR(cloth, "goal_friction", text="Friction")
            """

class PHYSICS_PT_cloth_cache(PhysicButtonsPanel):
    bl_label = "Cloth Cache"
    bl_default_closed = True

    def poll(self, context):
        return context.cloth

    def draw(self, context):
        md = context.cloth
        point_cache_ui(self, md.point_cache, cloth_panel_enabled(md), 0, 0)

class PHYSICS_PT_cloth_collision(PhysicButtonsPanel):
    bl_label = "Cloth Collision"
    bl_default_closed = True

    def poll(self, context):
        return context.cloth

    def draw_header(self, context):
        cloth = context.cloth.collision_settings

        self.layout.active = cloth_panel_enabled(context.cloth)
        self.layout.itemR(cloth, "enable_collision", text="")

    def draw(self, context):
        layout = self.layout

        cloth = context.cloth.collision_settings
        md = context.cloth

        layout.active = cloth.enable_collision and cloth_panel_enabled(md)

        split = layout.split()

        col = split.column()
        col.itemR(cloth, "collision_quality", slider=True, text="Quality")
        col.itemR(cloth, "min_distance", slider=True, text="Distance")
        col.itemR(cloth, "friction")

        col = split.column()
        col.itemR(cloth, "enable_self_collision", text="Self Collision")
        sub = col.column()
        sub.active = cloth.enable_self_collision
        sub.itemR(cloth, "self_collision_quality", slider=True, text="Quality")
        sub.itemR(cloth, "self_min_distance", slider=True, text="Distance")

class PHYSICS_PT_cloth_stiffness(PhysicButtonsPanel):
    bl_label = "Cloth Stiffness Scaling"
    bl_default_closed = True

    def poll(self, context):
        return context.cloth

    def draw_header(self, context):
        cloth = context.cloth.settings

        self.layout.active = cloth_panel_enabled(context.cloth)
        self.layout.itemR(cloth, "stiffness_scaling", text="")

    def draw(self, context):
        layout = self.layout

        md = context.cloth
        ob = context.object
        cloth = context.cloth.settings

        layout.active = cloth.stiffness_scaling	and cloth_panel_enabled(md)

        split = layout.split()

        col = split.column()
        col.itemL(text="Structural Stiffness:")
        sub = col.column(align=True)
        sub.itemR(cloth, "structural_stiffness_max", text="Max")
        sub.item_pointerR(cloth, "structural_stiffness_vertex_group", ob, "vertex_groups", text="")

        col = split.column()
        col.itemL(text="Bending Stiffness:")
        sub = col.column(align=True)
        sub.itemR(cloth, "bending_stiffness_max", text="Max")
        sub.item_pointerR(cloth, "bending_vertex_group", ob, "vertex_groups", text="")

class PHYSICS_PT_cloth_field_weights(PhysicButtonsPanel):
    bl_label = "Cloth Field Weights"
    bl_default_closed = True

    def poll(self, context):
        return (context.cloth)

    def draw(self, context):
        cloth = context.cloth.settings
        effector_weights_ui(self, cloth.effector_weights)

bpy.types.register(PHYSICS_PT_cloth)
bpy.types.register(PHYSICS_PT_cloth_cache)
bpy.types.register(PHYSICS_PT_cloth_collision)
bpy.types.register(PHYSICS_PT_cloth_stiffness)
bpy.types.register(PHYSICS_PT_cloth_field_weights)
