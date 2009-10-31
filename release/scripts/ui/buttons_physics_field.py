
import bpy

from buttons_physics_common import basic_force_field_settings_ui
from buttons_physics_common import basic_force_field_falloff_ui

class PhysicButtonsPanel(bpy.types.Panel):
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "physics"

    def poll(self, context):
        rd = context.scene.render_data
        return (context.object) and (not rd.use_game_engine)

class PHYSICS_PT_field(PhysicButtonsPanel):
    bl_label = "Force Fields"

    def draw(self, context):
        layout = self.layout

        ob = context.object
        field = ob.field

        split = layout.split(percentage=0.2)
        split.itemL(text="Type:")
        split.itemR(field, "type",text="")

        if field.type not in ('NONE', 'GUIDE', 'TEXTURE'):
            split = layout.split(percentage=0.2)
            #split = layout.row()
            split.itemL(text="Shape:")
            split.itemR(field, "shape", text="")

        split = layout.split()

        if field.type == 'NONE':
            return # nothing to draw
        elif field.type == 'GUIDE':
            col = split.column()
            col.itemR(field, "guide_minimum")
            col.itemR(field, "guide_free")
            col.itemR(field, "falloff_power")
            col.itemR(field, "guide_path_add")

            col = split.column()
            col.itemL(text="Clumping:")
            col.itemR(field, "guide_clump_amount")
            col.itemR(field, "guide_clump_shape")

            row = layout.row()
            row.itemR(field, "use_max_distance")
            sub = row.row()
            sub.active = field.use_max_distance
            sub.itemR(field, "maximum_distance")

            layout.itemS()

            layout.itemR(field, "guide_kink_type")
            if (field.guide_kink_type != "NONE"):
                layout.itemR(field, "guide_kink_axis")

                flow = layout.column_flow()
                flow.itemR(field, "guide_kink_frequency")
                flow.itemR(field, "guide_kink_shape")
                flow.itemR(field, "guide_kink_amplitude")

        elif field.type == 'TEXTURE':
            col = split.column()
            col.itemR(field, "strength")
            col.itemR(field, "texture", text="")
            col.itemR(field, "texture_mode", text="")
            col.itemR(field, "texture_nabla")

            col = split.column()
            col.itemR(field, "use_coordinates")
            col.itemR(field, "root_coordinates")
            col.itemR(field, "force_2d")

        else :
            basic_force_field_settings_ui(self, field)

        if field.type not in ('NONE', 'GUIDE'):

            layout.itemL(text="Falloff:")
            layout.itemR(field, "falloff_type", expand=True)

            basic_force_field_falloff_ui(self, field)

            if field.falloff_type == 'CONE':
                layout.itemS()

                split = layout.split(percentage=0.35)

                col = split.column()
                col.itemL(text="Angular:")
                col.itemR(field, "use_radial_min", text="Use Minimum")
                col.itemR(field, "use_radial_max", text="Use Maximum")

                col = split.column()
                col.itemR(field, "radial_falloff", text="Power")

                sub = col.column()
                sub.active = field.use_radial_min
                sub.itemR(field, "radial_minimum", text="Angle")

                sub = col.column()
                sub.active = field.use_radial_max
                sub.itemR(field, "radial_maximum", text="Angle")

            elif field.falloff_type == 'TUBE':
                layout.itemS()

                split = layout.split(percentage=0.35)

                col = split.column()
                col.itemL(text="Radial:")
                col.itemR(field, "use_radial_min", text="Use Minimum")
                col.itemR(field, "use_radial_max", text="Use Maximum")

                col = split.column()
                col.itemR(field, "radial_falloff", text="Power")

                sub = col.column()
                sub.active = field.use_radial_min
                sub.itemR(field, "radial_minimum", text="Distance")

                sub = col.column()
                sub.active = field.use_radial_max
                sub.itemR(field, "radial_maximum", text="Distance")

class PHYSICS_PT_collision(PhysicButtonsPanel):
    bl_label = "Collision"
    #bl_default_closed = True

    def poll(self, context):
        ob = context.object
        rd = context.scene.render_data
        return (ob and ob.type == 'MESH') and (not rd.use_game_engine)

    def draw(self, context):
        layout = self.layout

        md = context.collision

        split = layout.split()
        split.operator_context = 'EXEC_DEFAULT'

        if md:
            # remove modifier + settings
            split.set_context_pointer("modifier", md)
            split.itemO("object.modifier_remove", text="Remove")
            col = split.column()

            #row = split.row(align=True)
            #row.itemR(md, "render", text="")
            #row.itemR(md, "realtime", text="")

            coll = md.settings

        else:
            # add modifier
            split.item_enumO("object.modifier_add", "type", 'COLLISION', text="Add")
            split.itemL()

            coll = None

        if coll:
            settings = context.object.collision

            layout.active = settings.enabled

            split = layout.split()

            col = split.column()
            col.itemL(text="Particle:")
            col.itemR(settings, "permeability", slider=True)
            col.itemL(text="Particle Damping:")
            sub = col.column(align=True)
            sub.itemR(settings, "damping_factor", text="Factor", slider=True)
            sub.itemR(settings, "random_damping", text="Random", slider=True)

            col.itemL(text="Soft Body and Cloth:")
            sub = col.column(align=True)
            sub.itemR(settings, "outer_thickness", text="Outer", slider=True)
            sub.itemR(settings, "inner_thickness", text="Inner", slider=True)

            layout.itemL(text="Force Fields:")
            layout.itemR(settings, "absorption", text="Absorption")

            col = split.column()
            col.itemL(text="")
            col.itemR(settings, "kill_particles")
            col.itemL(text="Particle Friction:")
            sub = col.column(align=True)
            sub.itemR(settings, "friction_factor", text="Factor", slider=True)
            sub.itemR(settings, "random_friction", text="Random", slider=True)
            col.itemL(text="Soft Body Damping:")
            col.itemR(settings, "damping", text="Factor", slider=True)

bpy.types.register(PHYSICS_PT_field)
bpy.types.register(PHYSICS_PT_collision)
