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


class PhysicButtonsPanel():
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "physics"

    @classmethod
    def poll(cls, context):
        ob = context.object
        rd = context.scene.render
        return (ob and ob.type == 'MESH') and (not rd.use_game_engine) and (context.dynamic_paint)


class PHYSICS_PT_dynamic_paint(PhysicButtonsPanel, bpy.types.Panel):
    bl_label = "Dynamic Paint"

    def draw(self, context):
        layout = self.layout

        md = context.dynamic_paint
        ob = context.object

        if md:
            layout.prop(md, "dynamicpaint_type", expand=True)

            if md.dynamicpaint_type == 'CANVAS':
                canvas = md.canvas_settings
                
                layout.operator("dpaint.bake", text="Bake Dynamic Paint", icon='MOD_FLUIDSIM')
                if len(canvas.ui_info) != 0:
                    layout.label(text=canvas.ui_info)

                col = layout.column()
                col.label(text="Quality:")
                col.prop(canvas, "resolution")
                col.prop(canvas, "use_anti_aliasing")
                
                col = layout.column()
                col.label(text="Frames:")
                split = col.split()
                
                col = split.column(align=True)
                col.prop(canvas, "start_frame", text="Start")
                col.prop(canvas, "end_frame", text="End")
                
                col = split.column()
                col.prop(canvas, "substeps")
                

            elif md.dynamicpaint_type == 'PAINT':
                paint = md.paint_settings
                
                layout.prop(paint, "do_paint")
                
                split = layout.split()

                col = split.column()
                col.active = paint.do_paint
                col.prop(paint, "absolute_alpha")
                col.prop(paint, "paint_erase")
                col.prop(paint, "paint_wetness", text="Wetness")
                
                col = split.column()
                col.active = paint.do_paint
                sub = col.column()
                sub.active = (paint.paint_source != "PSYS");
                sub.prop(paint, "use_material")
                if paint.use_material and paint.paint_source != "PSYS":
                    col.prop(paint, "material", text="")
                else:
                    col.prop(paint, "paint_color", text="")
                col.prop(paint, "paint_alpha", text="Alpha")
                
                layout.label()
                layout.prop(paint, "do_displace")


class PHYSICS_PT_dp_output(PhysicButtonsPanel, bpy.types.Panel):
    bl_label = "Dynamic Paint: Output"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        md = context.dynamic_paint
        return md and (md.dynamicpaint_type == 'CANVAS')

    def draw(self, context):
        layout = self.layout

        canvas = context.dynamic_paint.canvas_settings

        col = layout.column()
        col.prop(canvas, "output_paint")
        sub = col.column()
        sub.active = canvas.output_paint
        sub.prop(canvas, "paint_output_path", text="")
        sub.prop(canvas, "premultiply", text="Premultiply alpha")
        
        layout.separator()

        col = layout.column()
        col.prop(canvas, "output_wet")
        sub = col.column()
        sub.active = canvas.output_wet
        sub.prop(canvas, "wet_output_path", text="")
        
        layout.separator()

        col = layout.column()
        col.prop(canvas, "output_disp")
        sub = col.column()
        sub.active = canvas.output_disp
        sub.prop(canvas, "displace_output_path", text="")
        sub.prop(canvas, "displacement", text="Strength")

        split = sub.split()
        sub = split.column()
        sub.prop(canvas, "disp_type", text="Type")
        sub = split.column()
        sub.prop(canvas, "disp_format", text="Format")


class PHYSICS_PT_dp_advanced_canvas(PhysicButtonsPanel, bpy.types.Panel):
    bl_label = "Dynamic Paint: Advanced"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        md = context.dynamic_paint
        return md and (md.dynamicpaint_type == 'CANVAS')

    def draw(self, context):
        layout = self.layout

        canvas = context.dynamic_paint.canvas_settings
        ob = context.object

        col = layout.column()
        split = col.split(percentage=0.7)
        split.prop(canvas, "dry_speed", text="Dry Time")
        split.prop(canvas, "use_dry_log", text="Slow")

        col = layout.column()
        col.prop(canvas, "use_dissolve_paint")
        sub = col.column()
        sub.active = canvas.use_dissolve_paint
        sub.prop(canvas, "dissolve_speed", text="Time")

        col = layout.column()
        col.prop(canvas, "use_flatten_disp", text="Flatten Displace")
        sub = col.column()
        sub.active = canvas.use_flatten_disp
        sub.prop(canvas, "flatten_speed", text="Time")
        
        layout.separator()
		
        layout.prop_search(canvas, "uv_layer", ob.data, "uv_textures")

class PHYSICS_PT_dp_effects(PhysicButtonsPanel, bpy.types.Panel):
    bl_label = "Dynamic Paint: Effects"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        md = context.dynamic_paint
        return md and (md.dynamicpaint_type == 'CANVAS')

    def draw(self, context):
        layout = self.layout

        canvas = context.dynamic_paint.canvas_settings

        layout.prop(canvas, "effect_ui", expand=True)

        if canvas.effect_ui == "SPREAD":
            layout.prop(canvas, "use_spread")
            col = layout.column()
            col.active = canvas.use_spread
            col.prop(canvas, "spread_speed")

        elif canvas.effect_ui == "DRIP":
            layout.prop(canvas, "use_drip")
            col = layout.column()
            col.active = canvas.use_drip
            col.prop(canvas, "drip_speed")

        elif canvas.effect_ui == "SHRINK":
            layout.prop(canvas, "use_shrink")
            col = layout.column()
            col.active = canvas.use_shrink
            col.prop(canvas, "shrink_speed")

class PHYSICS_PT_dp_advanced_paint(PhysicButtonsPanel, bpy.types.Panel):
    bl_label = "Dynamic Paint: Advanced"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        md = context.dynamic_paint
        return md and (md.dynamicpaint_type == 'PAINT')

    def draw(self, context):
        layout = self.layout

        paint = context.dynamic_paint.paint_settings
        ob = context.object
		
        split = layout.split()
        col = split.column()
        col.prop(paint, "paint_source")

        if paint.paint_source == "PSYS":
            col.prop_search(paint, "psys", ob, "particle_systems", text="")
            if paint.psys:
                col.label(text="Particle effect:")
                sub = col.column()
                sub.active = not paint.use_part_radius
                sub.prop(paint, "solid_radius", text="Solid Radius")
                col.prop(paint, "use_part_radius", text="Use Particle's Radius")
                col.prop(paint, "smooth_radius", text="Smooth radius")

        elif paint.paint_source == "DISTANCE" or paint.paint_source == "VOLDIST":
            col.prop(paint, "paint_distance", text="Paint Distance")
            split = layout.row().split()
            sub = split.column()
            sub.prop(paint, "prox_facealigned", text="Face Aligned")
            sub = split.column()
            sub.prop(paint, "prox_falloff", text="Falloff")
            if paint.prox_falloff == "RAMP":
                col = layout.row().column()
                col.label(text="Falloff Ramp:")
                col.prop(paint, "prox_ramp_alpha", text="Only Use Alpha")
                col.template_color_ramp(paint, "paint_ramp", expand=True)

def register():
    bpy.utils.register_module(__name__)


def unregister():
    bpy.utils.register_module(__name__)

if __name__ == "__main__":
    register()
