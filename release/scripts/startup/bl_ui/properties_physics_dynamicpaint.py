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
from bpy.types import Panel, UIList

from bl_ui.properties_physics_common import (point_cache_ui,
                                             effector_weights_ui,
                                             )


class PHYSICS_UL_dynapaint_surfaces(UIList):
    def draw_item(self, context, layout, data, item, icon, active_data, active_propname, index):
        # assert(isinstance(item, bpy.types.DynamicPaintSurface)
        surf = item
        sticon = layout.enum_item_icon(surf, "surface_type", surf.surface_type)
        if self.layout_type in {'DEFAULT', 'COMPACT'}:
            row = layout.row(align=True)
            row.label(text="", icon_value=icon)
            row.prop(surf, "name", text="", emboss=False, icon_value=sticon)
            row = layout.row(align=True)
            if surf.use_color_preview:
                row.prop(surf, "show_preview", text="", emboss=False,
                         icon='RESTRICT_VIEW_OFF' if surf.show_preview else 'RESTRICT_VIEW_ON')
            row.prop(surf, "is_active", text="")
        elif self.layout_type in {'GRID'}:
            layout.alignment = 'CENTER'
            row = layout.row(align=True)
            row.label(text="", icon_value=icon)
            row.label(text="", icon_value=sticon)


class PhysicButtonsPanel():
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "physics"

    @classmethod
    def poll(cls, context):
        ob = context.object
        rd = context.scene.render
        return (ob and ob.type == 'MESH') and (not rd.use_game_engine) and context.dynamic_paint


class PHYSICS_PT_dynamic_paint(PhysicButtonsPanel, Panel):
    bl_label = "Dynamic Paint"

    def draw(self, context):
        layout = self.layout

        md = context.dynamic_paint

        layout.prop(md, "ui_type", expand=True)

        if md.ui_type == 'CANVAS':
            canvas = md.canvas_settings

            if canvas is None:
                layout.operator("dpaint.type_toggle", text="Add Canvas").type = 'CANVAS'
            else:
                layout.operator("dpaint.type_toggle", text="Remove Canvas", icon='X').type = 'CANVAS'

                surface = canvas.canvas_surfaces.active

                row = layout.row()
                row.template_list("PHYSICS_UL_dynapaint_surfaces", "", canvas, "canvas_surfaces",
                                  canvas.canvas_surfaces, "active_index", rows=1)

                col = row.column(align=True)
                col.operator("dpaint.surface_slot_add", icon='ZOOMIN', text="")
                col.operator("dpaint.surface_slot_remove", icon='ZOOMOUT', text="")

                if surface:
                    layout.prop(surface, "surface_format")

                    col = layout.column()
                    if surface.surface_format != 'VERTEX':
                        col.label(text="Quality:")
                        col.prop(surface, "image_resolution")
                    col.prop(surface, "use_antialiasing")

                    col = layout.column()
                    col.label(text="Frames:")
                    split = col.split()

                    col = split.column(align=True)
                    col.prop(surface, "frame_start", text="Start")
                    col.prop(surface, "frame_end", text="End")

                    split.prop(surface, "frame_substeps")

        elif md.ui_type == 'BRUSH':
            brush = md.brush_settings
            use_shading_nodes = context.scene.render.use_shading_nodes

            if brush is None:
                layout.operator("dpaint.type_toggle", text="Add Brush").type = 'BRUSH'
            else:
                layout.operator("dpaint.type_toggle", text="Remove Brush", icon='X').type = 'BRUSH'

                split = layout.split()

                col = split.column()
                col.prop(brush, "use_absolute_alpha")
                col.prop(brush, "use_paint_erase")
                col.prop(brush, "paint_wetness", text="Wetness")

                col = split.column()
                if not use_shading_nodes:
                    sub = col.column()
                    sub.active = (brush.paint_source != 'PARTICLE_SYSTEM')
                    sub.prop(brush, "use_material")
                if brush.use_material and brush.paint_source != 'PARTICLE_SYSTEM' and not use_shading_nodes:
                    col.prop(brush, "material", text="")
                    col.prop(brush, "paint_alpha", text="Alpha Factor")
                else:
                    col.prop(brush, "paint_color", text="")
                    col.prop(brush, "paint_alpha", text="Alpha")


class PHYSICS_PT_dp_advanced_canvas(PhysicButtonsPanel, Panel):
    bl_label = "Dynamic Paint Advanced"

    @classmethod
    def poll(cls, context):
        md = context.dynamic_paint
        rd = context.scene.render
        return md and md.ui_type == 'CANVAS' and md.canvas_settings and md.canvas_settings.canvas_surfaces.active and (not rd.use_game_engine)

    def draw(self, context):
        layout = self.layout

        canvas = context.dynamic_paint.canvas_settings
        surface = canvas.canvas_surfaces.active

        surface_type = surface.surface_type

        layout.prop(surface, "surface_type")
        layout.separator()

        # dissolve
        if surface_type == 'PAINT':
            split = layout.split(percentage=0.35)
            split.prop(surface, "use_drying", text="Dry:")

            col = split.column()
            col.active = surface.use_drying
            split = col.split(percentage=0.7)
            col = split.column(align=True)
            col.prop(surface, "dry_speed", text="Time")
            col.prop(surface, "color_dry_threshold")
            split.prop(surface, "use_dry_log", text="Slow")

        if surface_type != 'WAVE':
            split = layout.split(percentage=0.35)
            col = split.column()
            if surface_type == 'WEIGHT':
                col.prop(surface, "use_dissolve", text="Fade:")
            else:
                col.prop(surface, "use_dissolve", text="Dissolve:")
            col = split.column()
            col.active = surface.use_dissolve
            split = col.split(percentage=0.7)
            split.prop(surface, "dissolve_speed", text="Time")
            split.prop(surface, "use_dissolve_log", text="Slow")

        # per type settings
        if surface_type == 'DISPLACE':
            layout.prop(surface, "use_incremental_displace")
            if surface.surface_format == 'VERTEX':
                row = layout.row()
                row.prop(surface, "depth_clamp")
                row.prop(surface, "displace_factor")

        elif surface_type == 'WAVE':
            layout.prop(surface, "use_wave_open_border")

            split = layout.split()

            col = split.column(align=True)
            col.prop(surface, "wave_timescale")
            col.prop(surface, "wave_speed")

            col = split.column(align=True)
            col.prop(surface, "wave_damping")
            col.prop(surface, "wave_spring")
            col.prop(surface, "wave_smoothness")

        layout.separator()
        layout.prop(surface, "brush_group")
        row = layout.row()
        row.prop(surface, "brush_influence_scale")
        row.prop(surface, "brush_radius_scale")


class PHYSICS_PT_dp_canvas_output(PhysicButtonsPanel, Panel):
    bl_label = "Dynamic Paint Output"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        md = context.dynamic_paint
        rd = context.scene.render
        if not (md and md.ui_type == 'CANVAS' and md.canvas_settings):
            return 0
        surface = context.dynamic_paint.canvas_settings.canvas_surfaces.active
        return (surface and
                (not (surface.surface_format == 'VERTEX' and (surface.surface_type in {'DISPLACE', 'WAVE'}))) and
                (not rd.use_game_engine))

    def draw(self, context):
        layout = self.layout

        canvas = context.dynamic_paint.canvas_settings
        surface = canvas.canvas_surfaces.active
        ob = context.object

        surface_type = surface.surface_type

        # vertex format outputs
        if surface.surface_format == 'VERTEX':
            if surface_type == 'PAINT':
                # toggle active preview
                layout.prop(surface, "preview_id")

                # paint-map output
                row = layout.row()
                row.prop_search(surface, "output_name_a", ob.data, "vertex_colors", text="Paintmap layer")
                if surface.output_exists(object=ob, index=0):
                    ic = 'ZOOMOUT'
                else:
                    ic = 'ZOOMIN'

                row.operator("dpaint.output_toggle", icon=ic, text="").output = 'A'

                # wet-map output
                row = layout.row()
                row.prop_search(surface, "output_name_b", ob.data, "vertex_colors", text="Wetmap layer")
                if surface.output_exists(object=ob, index=1):
                    ic = 'ZOOMOUT'
                else:
                    ic = 'ZOOMIN'

                row.operator("dpaint.output_toggle", icon=ic, text="").output = 'B'

            elif surface_type == 'WEIGHT':
                row = layout.row()
                row.prop_search(surface, "output_name_a", ob, "vertex_groups", text="Vertex Group")
                if surface.output_exists(object=ob, index=0):
                    ic = 'ZOOMOUT'
                else:
                    ic = 'ZOOMIN'

                row.operator("dpaint.output_toggle", icon=ic, text="").output = 'A'

        # image format outputs
        if surface.surface_format == 'IMAGE':
            layout.operator("dpaint.bake", text="Bake Image Sequence", icon='MOD_DYNAMICPAINT')
            layout.prop_search(surface, "uv_layer", ob.data, "uv_textures", text="UV Map")
            layout.separator()

            layout.prop(surface, "image_output_path", text="")
            row = layout.row()
            row.prop(surface, "image_fileformat", text="")
            row.prop(surface, "use_premultiply", text="Premultiply alpha")

            if surface_type == 'PAINT':
                split = layout.split(percentage=0.4)
                split.prop(surface, "use_output_a", text="Paintmaps:")
                sub = split.row()
                sub.active = surface.use_output_a
                sub.prop(surface, "output_name_a", text="")

                split = layout.split(percentage=0.4)
                split.prop(surface, "use_output_b", text="Wetmaps:")
                sub = split.row()
                sub.active = surface.use_output_b
                sub.prop(surface, "output_name_b", text="")
            else:
                col = layout.column()
                col.prop(surface, "output_name_a", text="Filename:")
                if surface_type == 'DISPLACE':
                    col.prop(surface, "displace_type", text="Displace Type")
                    col.prop(surface, "depth_clamp")
                elif surface_type == 'WAVE':
                    col.prop(surface, "depth_clamp", text="Wave Clamp")


class PHYSICS_PT_dp_canvas_initial_color(PhysicButtonsPanel, Panel):
    bl_label = "Dynamic Paint Initial Color"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        md = context.dynamic_paint
        rd = context.scene.render
        if not (md and md.ui_type == 'CANVAS' and md.canvas_settings):
            return 0
        surface = context.dynamic_paint.canvas_settings.canvas_surfaces.active
        return (surface and surface.surface_type == 'PAINT') and (not rd.use_game_engine)

    def draw(self, context):
        layout = self.layout

        canvas = context.dynamic_paint.canvas_settings
        surface = canvas.canvas_surfaces.active
        ob = context.object

        layout.prop(surface, "init_color_type", expand=False)
        if surface.init_color_type != 'NONE':
            layout.separator()

        # dissolve
        if surface.init_color_type == 'COLOR':
            layout.prop(surface, "init_color")

        elif surface.init_color_type == 'TEXTURE':
            layout.prop(surface, "init_texture")
            layout.prop_search(surface, "init_layername", ob.data, "uv_textures", text="UV Map")

        elif surface.init_color_type == 'VERTEX_COLOR':
            layout.prop_search(surface, "init_layername", ob.data, "vertex_colors", text="Color Layer")


class PHYSICS_PT_dp_effects(PhysicButtonsPanel, Panel):
    bl_label = "Dynamic Paint Effects"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        md = context.dynamic_paint
        rd = context.scene.render
        if not (md and md.ui_type == 'CANVAS' and md.canvas_settings):
            return False
        surface = context.dynamic_paint.canvas_settings.canvas_surfaces.active
        return (surface and surface.surface_type == 'PAINT') and (not rd.use_game_engine)

    def draw(self, context):
        layout = self.layout

        canvas = context.dynamic_paint.canvas_settings
        surface = canvas.canvas_surfaces.active

        layout.prop(surface, "effect_ui", expand=True)

        if surface.effect_ui == 'SPREAD':
            layout.prop(surface, "use_spread")

            row = layout.row()
            row.active = surface.use_spread
            row.prop(surface, "spread_speed")
            row.prop(surface, "color_spread_speed")

        elif surface.effect_ui == 'DRIP':
            layout.prop(surface, "use_drip")

            col = layout.column()
            col.active = surface.use_drip
            effector_weights_ui(self, context, surface.effector_weights, 'DYNAMIC_PAINT')

            layout.label(text="Surface Movement:")
            row = layout.row()
            row.prop(surface, "drip_velocity", slider=True)
            row.prop(surface, "drip_acceleration", slider=True)

        elif surface.effect_ui == 'SHRINK':
            layout.prop(surface, "use_shrink")

            row = layout.row()
            row.active = surface.use_shrink
            row.prop(surface, "shrink_speed")


class PHYSICS_PT_dp_cache(PhysicButtonsPanel, Panel):
    bl_label = "Dynamic Paint Cache"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        md = context.dynamic_paint
        rd = context.scene.render
        return (md and
                md.ui_type == 'CANVAS' and
                md.canvas_settings and
                md.canvas_settings.canvas_surfaces.active and
                md.canvas_settings.canvas_surfaces.active.is_cache_user and
                (not rd.use_game_engine))

    def draw(self, context):
        surface = context.dynamic_paint.canvas_settings.canvas_surfaces.active
        cache = surface.point_cache

        point_cache_ui(self, context, cache, (cache.is_baked is False), 'DYNAMIC_PAINT')


class PHYSICS_PT_dp_brush_source(PhysicButtonsPanel, Panel):
    bl_label = "Dynamic Paint Source"

    @classmethod
    def poll(cls, context):
        md = context.dynamic_paint
        rd = context.scene.render
        return md and md.ui_type == 'BRUSH' and md.brush_settings and (not rd.use_game_engine)

    def draw(self, context):
        layout = self.layout

        brush = context.dynamic_paint.brush_settings
        ob = context.object

        split = layout.split()
        col = split.column()
        col.prop(brush, "paint_source")

        if brush.paint_source == 'PARTICLE_SYSTEM':
            col.prop_search(brush, "particle_system", ob, "particle_systems", text="")
            if brush.particle_system:
                col.label(text="Particle effect:")
                sub = col.column()
                sub.active = not brush.use_particle_radius
                sub.prop(brush, "solid_radius", text="Solid Radius")
                col.prop(brush, "use_particle_radius", text="Use Particle's Radius")
                col.prop(brush, "smooth_radius", text="Smooth radius")

        if brush.paint_source in {'DISTANCE', 'VOLUME_DISTANCE', 'POINT'}:
            col.prop(brush, "paint_distance", text="Paint Distance")
            split = layout.row().split(percentage=0.4)
            sub = split.column()
            if brush.paint_source in {'DISTANCE', 'VOLUME_DISTANCE'}:
                sub.prop(brush, "use_proximity_project")
            if brush.paint_source == 'VOLUME_DISTANCE':
                sub.prop(brush, "invert_proximity")
                sub.prop(brush, "use_negative_volume")

            sub = split.column()
            if brush.paint_source in {'DISTANCE', 'VOLUME_DISTANCE'}:
                column = sub.column()
                column.active = brush.use_proximity_project
                column.prop(brush, "ray_direction")
            sub.prop(brush, "proximity_falloff")
            if brush.proximity_falloff == 'RAMP':
                col = layout.row().column()
                col.separator()
                col.prop(brush, "use_proximity_ramp_alpha", text="Only Use Alpha")
                col.template_color_ramp(brush, "paint_ramp", expand=True)


class PHYSICS_PT_dp_brush_velocity(PhysicButtonsPanel, Panel):
    bl_label = "Dynamic Paint Velocity"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        md = context.dynamic_paint
        rd = context.scene.render
        return md and md.ui_type == 'BRUSH' and md.brush_settings and (not rd.use_game_engine)

    def draw(self, context):
        layout = self.layout

        brush = context.dynamic_paint.brush_settings

        split = layout.split()

        col = split.column()
        col.prop(brush, "use_velocity_alpha")
        col.prop(brush, "use_velocity_color")

        split.prop(brush, "use_velocity_depth")

        col = layout.column()
        col.active = (brush.use_velocity_alpha or brush.use_velocity_color or brush.use_velocity_depth)
        col.prop(brush, "velocity_max")
        col.template_color_ramp(brush, "velocity_ramp", expand=True)
        layout.separator()

        row = layout.row()
        row.prop(brush, "use_smudge")
        sub = row.row()
        sub.active = brush.use_smudge
        sub.prop(brush, "smudge_strength")


class PHYSICS_PT_dp_brush_wave(PhysicButtonsPanel, Panel):
    bl_label = "Dynamic Paint Waves"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        md = context.dynamic_paint
        rd = context.scene.render
        return md and md.ui_type == 'BRUSH' and md.brush_settings and (not rd.use_game_engine)

    def draw(self, context):
        layout = self.layout

        brush = context.dynamic_paint.brush_settings

        layout.prop(brush, "wave_type")
        if brush.wave_type != 'REFLECT':
            row = layout.row()
            row.prop(brush, "wave_factor")
            row.prop(brush, "wave_clamp")


def register():
    bpy.utils.register_module(__name__)


def unregister():
    bpy.utils.register_module(__name__)

if __name__ == "__main__":
    register()
