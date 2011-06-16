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

from bl_ui.properties_physics_common import (
    point_cache_ui,
    )

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
                surface = canvas.active_surface
                row = layout.row()
                row.template_list(canvas, "canvas_surfaces", canvas, "active_index", rows=2)

                col = row.column(align=True)
                col.operator("dpaint.surface_slot_add", icon='ZOOMIN', text="")
                col.operator("dpaint.surface_slot_remove", icon='ZOOMOUT', text="")
                
                if surface:
                    layout.prop(surface, "name")
                    layout.prop(surface, "surface_format", expand=False)
                    
                    if surface.surface_format != "VERTEX":
                        col = layout.column()
                        col.label(text="Quality:")
                        col.prop(surface, "image_resolution")
                        col.prop(surface, "use_anti_aliasing")
                
                    col = layout.column()
                    col.label(text="Frames:")
                    split = col.split()
                
                    col = split.column(align=True)
                    col.prop(surface, "start_frame", text="Start")
                    col.prop(surface, "end_frame", text="End")
                
                    col = split.column()
                    col.prop(surface, "substeps")
                

            elif md.dynamicpaint_type == 'BRUSH':
                brush = md.brush_settings
                
                layout.prop(brush, "brush_settings_context", expand=True, icon_only=True)
                
                if (brush.brush_settings_context == "GENERAL"):
                    split = layout.split()

                    col = split.column()
                    col.prop(brush, "absolute_alpha")
                    col.prop(brush, "paint_erase")
                    col.prop(brush, "paint_wetness", text="Wetness")
                
                    col = split.column()
                    sub = col.column()
                    sub.active = (brush.paint_source != "PSYS");
                    sub.prop(brush, "use_material")
                    if brush.use_material and brush.paint_source != "PSYS":
                        col.prop(brush, "material", text="")
                        col.prop(brush, "paint_alpha", text="Alpha Factor")
                    else:
                        col.prop(brush, "paint_color", text="")
                        col.prop(brush, "paint_alpha", text="Alpha")
                
                if (brush.brush_settings_context != "GENERAL"):
                    layout.label(text="-WIP-")


class PHYSICS_PT_dp_advanced_canvas(PhysicButtonsPanel, bpy.types.Panel):
    bl_label = "Dynamic Paint: Advanced"

    @classmethod
    def poll(cls, context):
        md = context.dynamic_paint
        return md and (md.dynamicpaint_type == 'CANVAS') and (context.dynamic_paint.canvas_settings.active_surface)

    def draw(self, context):
        layout = self.layout

        canvas = context.dynamic_paint.canvas_settings
        surface = canvas.active_surface
        ob = context.object

        layout.prop(surface, "surface_type", expand=False)

        if (surface.surface_type == "PAINT"):
            layout.prop(surface, "initial_color", expand=False)
            col = layout.split(percentage=0.33)
            col.prop(surface, "use_dissolve", text="Dissolve:")
            sub = col.column()
            sub.active = surface.use_dissolve
            sub.prop(surface, "dissolve_speed", text="Time")

        if (surface.surface_type == "DISPLACE"):
            col = layout.split(percentage=0.33)
            col.prop(surface, "use_dissolve", text="Flatten:")
            sub = col.column()
            sub.active = surface.use_dissolve
            sub.prop(surface, "dissolve_speed", text="Time")
            
        layout.label(text="Brush Group:")
        layout.prop(surface, "brush_group", text="")


class PHYSICS_PT_dp_canvas_output(PhysicButtonsPanel, bpy.types.Panel):
    bl_label = "Dynamic Paint: Output"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        md = context.dynamic_paint
        if ((not md) or (md.dynamicpaint_type != 'CANVAS')):
            return 0
        surface = context.dynamic_paint.canvas_settings.active_surface
        return (surface and (not (surface.surface_format=="VERTEX" and surface.surface_type=="DISPLACE") ))

    def draw(self, context):
        layout = self.layout

        canvas = context.dynamic_paint.canvas_settings
        surface = canvas.active_surface
        ob = context.object
        
        # vertex format outputs
        if (surface.surface_format == "VERTEX"):
            if (surface.surface_type == "PAINT"):
                row = layout.row()
                row.prop_search(surface, "output_name", ob.data, "vertex_colors", text="Paintmap layer: ")
                #col = row.column(align=True)
                #col.operator("dpaint.output_add", icon='ZOOMIN', text="")
                
                row = layout.row()
                row.prop_search(surface, "output_name2", ob.data, "vertex_colors", text="Wetmap layer: ")
                #col = row.column(align=True)
                #col.operator("dpaint.output_add", icon='ZOOMIN', text="")

        # image format outputs
        if (surface.surface_format == "IMAGE"):
            col = layout.column()
            col.label(text="UV layer:")
            col.prop_search(surface, "uv_layer", ob.data, "uv_textures", text="")
            
            col = layout.column()
            col.prop(surface, "image_output_path", text="Output directory")
            if (surface.surface_type == "PAINT"):
                col.prop(surface, "output_name", text="Paintmap: ")
                col.prop(surface, "premultiply", text="Premultiply alpha")
                col.prop(surface, "output_name2", text="Wetmap: ")
            if (surface.surface_type == "DISPLACE"):
                col.prop(surface, "output_name", text="Filename: ")
            
            layout.separator()
            layout.operator("dpaint.bake", text="Bake Image Sequence", icon='MOD_DYNAMICPAINT')
            if len(canvas.ui_info) != 0:
                layout.label(text=canvas.ui_info)
#            
#            layout.separator()
#
#            col = layout.column()
#            col.prop(surface, "output_wet")
#            sub = col.column()
#            sub.active = surface.output_wet
#            sub.prop(surface, "wet_output_path", text="")
#            
#            layout.separator()
#
#            col = layout.column()
#            col.prop(surface, "output_disp")
#            sub = col.column()
#            sub.active = surface.output_disp
#            sub.prop(surface, "displace_output_path", text="")
#            sub.prop(surface, "displacement", text="Strength")
#
#            split = sub.split()
#            sub = split.column()
#            sub.prop(surface, "disp_type", text="Type")
#            sub = split.column()
#            sub.prop(surface, "disp_format", text="Format")
		

class PHYSICS_PT_dp_effects(PhysicButtonsPanel, bpy.types.Panel):
    bl_label = "Dynamic Paint: Effects"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        md = context.dynamic_paint
        if ((not md) or (md.dynamicpaint_type != 'CANVAS')):
            return False;
        surface = context.dynamic_paint.canvas_settings.active_surface
        return surface and (surface.surface_format != "VERTEX")

    def draw(self, context):
        layout = self.layout

        canvas = context.dynamic_paint.canvas_settings
        surface = canvas.active_surface

        layout.prop(surface, "effect_ui", expand=True)

        if surface.effect_ui == "SPREAD":
            layout.prop(surface, "use_spread")
            col = layout.column()
            col.active = surface.use_spread
            col.prop(surface, "spread_speed")

        elif surface.effect_ui == "DRIP":
            layout.prop(surface, "use_drip")
            col = layout.column()
            col.active = surface.use_drip
            col.prop(surface, "drip_speed")

        elif surface.effect_ui == "SHRINK":
            layout.prop(surface, "use_shrink")
            col = layout.column()
            col.active = surface.use_shrink
            col.prop(surface, "shrink_speed")
			

class PHYSICS_PT_dp_cache(PhysicButtonsPanel, bpy.types.Panel):
    bl_label = "Dynamic Paint: Cache"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        md = context.dynamic_paint
        return md and (md.dynamicpaint_type == 'CANVAS') and \
        (md.canvas_settings.active_surface) and (md.canvas_settings.active_surface.uses_cache)

    def draw(self, context):
        layout = self.layout

        surface = context.dynamic_paint.canvas_settings.active_surface
        cache = surface.point_cache
        
        point_cache_ui(self, context, cache, (cache.is_baked is False), 'DYNAMIC_PAINT')


class PHYSICS_PT_dp_advanced_brush(PhysicButtonsPanel, bpy.types.Panel):
    bl_label = "Dynamic Paint: Advanced"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        md = context.dynamic_paint
        return md and (md.dynamicpaint_type == 'BRUSH')

    def draw(self, context):
        layout = self.layout

        brush = context.dynamic_paint.brush_settings
        ob = context.object
		
        split = layout.split()
        col = split.column()
        col.prop(brush, "paint_source")

        if brush.paint_source == "PSYS":
            col.prop_search(brush, "psys", ob, "particle_systems", text="")
            if brush.psys:
                col.label(text="Particle effect:")
                sub = col.column()
                sub.active = not brush.use_part_radius
                sub.prop(brush, "solid_radius", text="Solid Radius")
                col.prop(brush, "use_part_radius", text="Use Particle's Radius")
                col.prop(brush, "smooth_radius", text="Smooth radius")

        elif brush.paint_source == "DISTANCE" or brush.paint_source == "VOLDIST":
            col.prop(brush, "paint_distance", text="Paint Distance")
            split = layout.row().split()
            sub = split.column()
            sub.prop(brush, "prox_facealigned", text="Face Aligned")
            sub = split.column()
            sub.prop(brush, "prox_falloff", text="Falloff")
            if brush.prox_falloff == "RAMP":
                col = layout.row().column()
                col.label(text="Falloff Ramp:")
                col.prop(brush, "prox_ramp_alpha", text="Only Use Alpha")
                col.template_color_ramp(brush, "paint_ramp", expand=True)

def register():
    bpy.utils.register_module(__name__)


def unregister():
    bpy.utils.register_module(__name__)

if __name__ == "__main__":
    register()
