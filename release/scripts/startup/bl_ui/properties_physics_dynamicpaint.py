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

from bpy.types import (
    Panel,
    UIList,
)
from .properties_physics_common import (
    point_cache_ui,
    effector_weights_ui,
)


class PHYSICS_UL_dynapaint_surfaces(UIList):
    def draw_item(self, _context, layout, _data, item, icon, _active_data, _active_propname, _index):
        # assert(isinstance(item, bpy.types.DynamicPaintSurface)
        surf = item
        sticon = layout.enum_item_icon(surf, "surface_type", surf.surface_type)

        if self.layout_type in {'DEFAULT', 'COMPACT'}:
            row = layout.row(align=True)
            row.label(text="", icon_value=icon)
            row.prop(surf, "name", text="", emboss=False, icon_value=sticon)
            row = layout.row(align=True)
            row.prop(surf, "is_active", text="")

        elif self.layout_type == 'GRID':
            layout.alignment = 'CENTER'
            row = layout.row(align=True)
            row.label(text="", icon_value=icon)
            row.label(text="", icon_value=sticon)


class PhysicButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "physics"

    @staticmethod
    def poll_dyn_paint(context):
        ob = context.object
        return (ob and ob.type == 'MESH') and context.dynamic_paint

    @staticmethod
    def poll_dyn_canvas(context):
        if not PhysicButtonsPanel.poll_dyn_paint(context):
            return False

        md = context.dynamic_paint
        return (md and md.ui_type == 'CANVAS' and md.canvas_settings and md.canvas_settings.canvas_surfaces.active)

    @staticmethod
    def poll_dyn_canvas_paint(context):
        if not PhysicButtonsPanel.poll_dyn_canvas(context):
            return False

        surface = context.dynamic_paint.canvas_settings.canvas_surfaces.active
        return (surface.surface_type == 'PAINT')

    @staticmethod
    def poll_dyn_canvas_brush(context):
        if not PhysicButtonsPanel.poll_dyn_paint(context):
            return False

        md = context.dynamic_paint
        return (md and md.ui_type == 'BRUSH' and md.brush_settings)

    @staticmethod
    def poll_dyn_output(context):
        if not PhysicButtonsPanel.poll_dyn_canvas(context):
            return False

        surface = context.dynamic_paint.canvas_settings.canvas_surfaces.active
        return (not (surface.surface_format == 'VERTEX' and (surface.surface_type in {'DISPLACE', 'WAVE'})))

    @staticmethod
    def poll_dyn_output_maps(context):
        if not PhysicButtonsPanel.poll_dyn_output(context):
            return False

        surface = context.dynamic_paint.canvas_settings.canvas_surfaces.active
        return (surface.surface_format == 'IMAGE' and surface.surface_type == 'PAINT')


class PHYSICS_PT_dynamic_paint(PhysicButtonsPanel, Panel):
    bl_label = "Dynamic Paint"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    @classmethod
    def poll(cls, context):
        if not PhysicButtonsPanel.poll_dyn_paint(context):
            return False

        return (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        md = context.dynamic_paint

        layout.prop(md, "ui_type")


class PHYSICS_PT_dynamic_paint_settings(PhysicButtonsPanel, Panel):
    bl_label = "Settings"
    bl_parent_id = 'PHYSICS_PT_dynamic_paint'
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    @classmethod
    def poll(cls, context):
        if not PhysicButtonsPanel.poll_dyn_paint(context):
            return False

        return (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout

        md = context.dynamic_paint

        if md.ui_type == 'CANVAS':
            canvas = md.canvas_settings

            if canvas is None:
                layout.operator("dpaint.type_toggle", text="Add Canvas").type = 'CANVAS'
                return  # do nothing.

            layout.operator("dpaint.type_toggle", text="Remove Canvas", icon='X').type = 'CANVAS'

            surface = canvas.canvas_surfaces.active

            row = layout.row()
            row.template_list(
                "PHYSICS_UL_dynapaint_surfaces", "", canvas, "canvas_surfaces",
                canvas.canvas_surfaces, "active_index", rows=1
            )

            col = row.column(align=True)
            col.operator("dpaint.surface_slot_add", icon='ADD', text="")
            col.operator("dpaint.surface_slot_remove", icon='REMOVE', text="")

            layout.separator()

            layout.use_property_split = True

            if surface:
                flow = layout.grid_flow(
                    row_major=True, columns=0, even_columns=True, even_rows=False, align=False
                )
                col = flow.column()

                col.prop(surface, "surface_format")

                if surface.surface_format != 'VERTEX':
                    col.prop(surface, "image_resolution")
                col.prop(surface, "use_antialiasing")

                col = flow.column(align=True)
                col.prop(surface, "frame_start", text="Frame Start")
                col.prop(surface, "frame_end", text="End")

                col.prop(surface, "frame_substeps")

        elif md.ui_type == 'BRUSH':
            brush = md.brush_settings

            if brush is None:
                layout.operator("dpaint.type_toggle", text="Add Brush").type = 'BRUSH'
                return  # do nothing.

            layout.operator("dpaint.type_toggle", text="Remove Brush", icon='X').type = 'BRUSH'

            layout.use_property_split = True

            flow = layout.grid_flow(
                row_major=True, columns=0, even_columns=True, even_rows=False, align=False
            )
            col = flow.column()
            col.prop(brush, "paint_color")
            col.prop(brush, "paint_alpha", text="Alpha", slider=True)

            col = flow.column()
            col.prop(brush, "paint_wetness", text="Wetness", slider=True)
            col.prop(brush, "use_absolute_alpha")
            col.prop(brush, "use_paint_erase")


class PHYSICS_PT_dp_surface_canvas(PhysicButtonsPanel, Panel):
    bl_label = "Surface"
    bl_parent_id = "PHYSICS_PT_dynamic_paint"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    @classmethod
    def poll(cls, context):
        if not PhysicButtonsPanel.poll_dyn_canvas(context):
            return False

        return (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        canvas = context.dynamic_paint.canvas_settings
        surface = canvas.canvas_surfaces.active
        surface_type = surface.surface_type

        layout.prop(surface, "surface_type")

        layout.separator()

        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=True)

        # per type settings
        if surface_type == 'DISPLACE':
            col = flow.column()

            if surface.surface_format == 'VERTEX':
                col.prop(surface, "depth_clamp")
                col.prop(surface, "displace_factor")

            col.prop(surface, "use_incremental_displace")
            col.separator()

        elif surface_type == 'WAVE':
            col = flow.column()
            col.prop(surface, "use_wave_open_border")
            col.prop(surface, "wave_timescale")
            col.prop(surface, "wave_speed")

            col.separator()

            col = flow.column()
            col.prop(surface, "wave_damping")
            col.prop(surface, "wave_spring")
            col.prop(surface, "wave_smoothness")

            col.separator()

        col = flow.column()
        col.prop(surface, "brush_collection")

        if surface_type not in {'DISPLACE', 'WAVE'}:
            col = flow.column()  # flow the layout otherwise.

        col.prop(surface, "brush_influence_scale", text="Scale Influence")
        col.prop(surface, "brush_radius_scale", text="Radius")


class PHYSICS_PT_dp_surface_canvas_paint_dry(PhysicButtonsPanel, Panel):
    bl_label = "Dry"
    bl_parent_id = "PHYSICS_PT_dp_surface_canvas"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    @classmethod
    def poll(cls, context):
        if not PhysicButtonsPanel.poll_dyn_canvas_paint(context):
            return False

        return (context.engine in cls.COMPAT_ENGINES)

    def draw_header(self, context):
        canvas = context.dynamic_paint.canvas_settings
        surface = canvas.canvas_surfaces.active
        self.layout.prop(surface, "use_drying", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=False)

        canvas = context.dynamic_paint.canvas_settings
        surface = canvas.canvas_surfaces.active

        flow.active = surface.use_drying

        col = flow.column()
        col.prop(surface, "dry_speed", text="Time")

        col = flow.column()
        col.prop(surface, "color_dry_threshold", text="Color")
        col.prop(surface, "use_dry_log", text="Slow")


class PHYSICS_PT_dp_surface_canvas_paint_dissolve(PhysicButtonsPanel, Panel):
    bl_label = "Dissolve"
    bl_parent_id = "PHYSICS_PT_dp_surface_canvas"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    @classmethod
    def poll(cls, context):
        if not PhysicButtonsPanel.poll_dyn_canvas(context):
            return False

        surface = context.dynamic_paint.canvas_settings.canvas_surfaces.active

        return (surface.surface_type != 'WAVE' and context.engine in cls.COMPAT_ENGINES)

    def draw_header(self, context):
        canvas = context.dynamic_paint.canvas_settings
        surface = canvas.canvas_surfaces.active
        self.layout.prop(surface, "use_dissolve", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=False)

        canvas = context.dynamic_paint.canvas_settings
        surface = canvas.canvas_surfaces.active

        flow.active = surface.use_dissolve

        col = flow.column()
        col.prop(surface, "dissolve_speed", text="Time")

        col = flow.column()
        col.prop(surface, "use_dissolve_log", text="Slow")


class PHYSICS_PT_dp_canvas_output(PhysicButtonsPanel, Panel):
    bl_label = "Output"
    bl_parent_id = "PHYSICS_PT_dynamic_paint"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    @classmethod
    def poll(cls, context):
        if not PhysicButtonsPanel.poll_dyn_output(context):
            return False

        return (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        canvas = context.dynamic_paint.canvas_settings
        surface = canvas.canvas_surfaces.active
        ob = context.object

        surface_type = surface.surface_type

        # vertex format outputs.
        if surface.surface_format == 'VERTEX':
            if surface_type == 'PAINT':
                # paint-map output.
                row = layout.row()
                row.prop_search(surface, "output_name_a", ob.data, "vertex_colors", text="Paintmap Layer")

                icons = 'REMOVE' if surface.output_exists(object=ob, index=0) else 'ADD'
                row.operator("dpaint.output_toggle", icon=icons, text="").output = 'A'

                # wet-map output.
                row = layout.row()
                row.prop_search(surface, "output_name_b", ob.data, "vertex_colors", text="Wetmap Layer")

                icons = 'REMOVE' if surface.output_exists(object=ob, index=1) else 'ADD'
                row.operator("dpaint.output_toggle", icon=icons, text="").output = 'B'

            elif surface_type == 'WEIGHT':
                row = layout.row()
                row.prop_search(surface, "output_name_a", ob, "vertex_groups", text="Vertex Group")

                icons = 'REMOVE' if surface.output_exists(object=ob, index=0) else 'ADD'
                row.operator("dpaint.output_toggle", icon=icons, text="").output = 'A'

        # image format outputs.
        if surface.surface_format == 'IMAGE':

            layout.operator("dpaint.bake", text="Bake Image Sequence", icon='MOD_DYNAMICPAINT')

            layout.prop(surface, "image_output_path", text="Cache Path")

            flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=False)

            col = flow.column()

            col.prop_search(surface, "uv_layer", ob.data, "uv_layers", text="UV Map")

            col = flow.column()
            col.prop(surface, "image_fileformat")
            col.prop(surface, "use_premultiply", text="Premultiply Alpha")

            if surface_type != 'PAINT':
                col = col.column()
                col.prop(surface, "output_name_a", text="Filename")

                if surface_type == 'DISPLACE':
                    col.prop(surface, "displace_type", text="Displace Type")
                    col.prop(surface, "depth_clamp")

                elif surface_type == 'WAVE':
                    col.prop(surface, "depth_clamp", text="Wave Clamp")


class PHYSICS_PT_dp_canvas_output_paintmaps(PhysicButtonsPanel, Panel):
    bl_label = "Paintmaps"
    bl_parent_id = "PHYSICS_PT_dp_canvas_output"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    @classmethod
    def poll(cls, context):
        if not PhysicButtonsPanel.poll_dyn_output_maps(context):
            return False

        return (context.engine in cls.COMPAT_ENGINES)

    def draw_header(self, context):
        canvas = context.dynamic_paint.canvas_settings
        surface = canvas.canvas_surfaces.active
        self.layout.prop(surface, "use_output_a", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        canvas = context.dynamic_paint.canvas_settings
        surface = canvas.canvas_surfaces.active

        sub = layout.column()
        sub.active = surface.use_output_a
        sub.prop(surface, "output_name_a", text="Name")


class PHYSICS_PT_dp_canvas_output_wetmaps(PhysicButtonsPanel, Panel):
    bl_label = "Wetmaps"
    bl_parent_id = "PHYSICS_PT_dp_canvas_output"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    @classmethod
    def poll(cls, context):
        if not PhysicButtonsPanel.poll_dyn_output_maps(context):
            return False

        return (context.engine in cls.COMPAT_ENGINES)

    def draw_header(self, context):
        canvas = context.dynamic_paint.canvas_settings
        surface = canvas.canvas_surfaces.active
        self.layout.prop(surface, "use_output_b", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        canvas = context.dynamic_paint.canvas_settings
        surface = canvas.canvas_surfaces.active

        sub = layout.column()
        sub.active = surface.use_output_b
        sub.prop(surface, "output_name_b", text="Name")


class PHYSICS_PT_dp_canvas_initial_color(PhysicButtonsPanel, Panel):
    bl_label = "Initial Color"
    bl_parent_id = "PHYSICS_PT_dynamic_paint"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    @classmethod
    def poll(cls, context):
        if not PhysicButtonsPanel.poll_dyn_canvas_paint(context):
            return False

        return (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout

        canvas = context.dynamic_paint.canvas_settings
        surface = canvas.canvas_surfaces.active
        ob = context.object

        layout.use_property_split = True

        col = layout.column()
        col.prop(surface, "init_color_type", text="Type", expand=False)

        if surface.init_color_type != 'NONE':
            col.separator()

        # dissolve
        if surface.init_color_type == 'COLOR':
            layout.prop(surface, "init_color")

        elif surface.init_color_type == 'TEXTURE':
            col.prop(surface, "init_texture")
            col.prop_search(surface, "init_layername", ob.data, "uv_layers", text="UV Map")

        elif surface.init_color_type == 'VERTEX_COLOR':
            col.prop_search(surface, "init_layername", ob.data, "vertex_colors", text="Color Layer")


class PHYSICS_PT_dp_effects(PhysicButtonsPanel, Panel):
    bl_label = "Effects"
    bl_parent_id = 'PHYSICS_PT_dynamic_paint'
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    @classmethod
    def poll(cls, context):
        if not PhysicButtonsPanel.poll_dyn_canvas_paint(context):
            return False

        return (context.engine in cls.COMPAT_ENGINES)

    def draw(self, _context):
        return  # do nothing.


class PHYSICS_PT_dp_effects_spread(PhysicButtonsPanel, Panel):
    bl_label = "Spread"
    bl_parent_id = "PHYSICS_PT_dp_effects"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    @classmethod
    def poll(cls, context):
        if not PhysicButtonsPanel.poll_dyn_paint(context):
            return False

        return (context.engine in cls.COMPAT_ENGINES)

    def draw_header(self, context):
        canvas = context.dynamic_paint.canvas_settings
        surface = canvas.canvas_surfaces.active

        self.layout.prop(surface, "use_spread", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=False)

        canvas = context.dynamic_paint.canvas_settings
        surface = canvas.canvas_surfaces.active
        layout.active = surface.use_spread

        col = flow.column()
        col.prop(surface, "spread_speed", text="Speed")

        col = flow.column()
        col.prop(surface, "color_spread_speed", text="Color")


class PHYSICS_PT_dp_effects_drip(PhysicButtonsPanel, Panel):
    bl_label = "Drip"
    bl_parent_id = "PHYSICS_PT_dp_effects"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    @classmethod
    def poll(cls, context):
        if not PhysicButtonsPanel.poll_dyn_paint(context):
            return False

        return (context.engine in cls.COMPAT_ENGINES)

    def draw_header(self, context):
        canvas = context.dynamic_paint.canvas_settings
        surface = canvas.canvas_surfaces.active

        self.layout.prop(surface, "use_drip", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=False)

        canvas = context.dynamic_paint.canvas_settings
        surface = canvas.canvas_surfaces.active

        flow.active = surface.use_drip

        col = flow.column()
        col.prop(surface, "drip_velocity", slider=True)

        col = flow.column()
        col.prop(surface, "drip_acceleration", slider=True)


class PHYSICS_PT_dp_effects_drip_weights(PhysicButtonsPanel, Panel):
    bl_label = "Weights"
    bl_parent_id = "PHYSICS_PT_dp_effects_drip"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    @classmethod
    def poll(cls, context):
        if not PhysicButtonsPanel.poll_dyn_paint(context):
            return False

        return (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout

        canvas = context.dynamic_paint.canvas_settings
        surface = canvas.canvas_surfaces.active

        layout.active = surface.use_drip

        effector_weights_ui(self, surface.effector_weights, 'DYNAMIC_PAINT')


class PHYSICS_PT_dp_effects_shrink(PhysicButtonsPanel, Panel):
    bl_label = "Shrink"
    bl_parent_id = "PHYSICS_PT_dp_effects"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    @classmethod
    def poll(cls, context):
        if not PhysicButtonsPanel.poll_dyn_paint(context):
            return False

        return (context.engine in cls.COMPAT_ENGINES)

    def draw_header(self, context):
        canvas = context.dynamic_paint.canvas_settings
        surface = canvas.canvas_surfaces.active

        self.layout.prop(surface, "use_shrink", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        canvas = context.dynamic_paint.canvas_settings
        surface = canvas.canvas_surfaces.active
        layout.active = surface.use_shrink

        layout.prop(surface, "shrink_speed", text="Speed")


class PHYSICS_PT_dp_cache(PhysicButtonsPanel, Panel):
    bl_label = "Cache"
    bl_parent_id = "PHYSICS_PT_dynamic_paint"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    @classmethod
    def poll(cls, context):
        if not PhysicButtonsPanel.poll_dyn_canvas(context):
            return False

        surface = context.dynamic_paint.canvas_settings.canvas_surfaces.active
        return (surface.is_cache_user and (context.engine in cls.COMPAT_ENGINES))

    def draw(self, context):
        surface = context.dynamic_paint.canvas_settings.canvas_surfaces.active
        cache = surface.point_cache

        point_cache_ui(self, cache, (cache.is_baked is False), 'DYNAMIC_PAINT')


class PHYSICS_PT_dp_brush_source(PhysicButtonsPanel, Panel):
    bl_label = "Source"
    bl_parent_id = "PHYSICS_PT_dynamic_paint"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    @classmethod
    def poll(cls, context):
        if not PhysicButtonsPanel.poll_dyn_canvas_brush(context):
            return False

        return (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        brush = context.dynamic_paint.brush_settings
        ob = context.object

        layout.prop(brush, "paint_source", text="Paint")

        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=False)

        if brush.paint_source == 'PARTICLE_SYSTEM':
            col = flow.column()

            col.separator()

            col.prop_search(brush, "particle_system", ob, "particle_systems")

            if brush.particle_system:
                col = flow.column()

                sub = col.column()
                sub.active = not brush.use_particle_radius
                sub.prop(brush, "solid_radius", text="Effect Solid Radius")

                col.prop(brush, "use_particle_radius", text="Use Particle's Radius")
                col.prop(brush, "smooth_radius", text="Smooth Radius")

        if brush.paint_source in {'DISTANCE', 'VOLUME_DISTANCE', 'POINT'}:
            col = flow.column()

            col.separator()

            col.prop(brush, "paint_distance", text="Distance")
            col.prop(brush, "proximity_falloff")

            if brush.paint_source == 'VOLUME_DISTANCE':
                col.prop(brush, "invert_proximity")

                col = flow.column()
                col.prop(brush, "use_negative_volume")

            if brush.paint_source in {'DISTANCE', 'VOLUME_DISTANCE'}:
                col = flow.column() if brush.paint_source != 'VOLUME_DISTANCE' else col.column()
                col.prop(brush, "use_proximity_project")

                sub = col.column()
                sub.active = brush.use_proximity_project
                sub.prop(brush, "ray_direction")


class PHYSICS_PT_dp_brush_source_color_ramp(PhysicButtonsPanel, Panel):
    bl_label = "Falloff Ramp"
    bl_parent_id = "PHYSICS_PT_dp_brush_source"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    @classmethod
    def poll(cls, context):
        if not PhysicButtonsPanel.poll_dyn_canvas_brush(context):
            return False

        brush = context.dynamic_paint.brush_settings
        return ((brush.paint_source in {'DISTANCE', 'VOLUME_DISTANCE', 'POINT'})
                and (brush.proximity_falloff == 'RAMP')
                and (context.engine in cls.COMPAT_ENGINES))

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        brush = context.dynamic_paint.brush_settings
        layout.prop(brush, "use_proximity_ramp_alpha", text="Only Use Alpha")

        layout.use_property_split = False
        layout.template_color_ramp(brush, "paint_ramp", expand=True)


class PHYSICS_PT_dp_brush_velocity(PhysicButtonsPanel, Panel):
    bl_label = "Velocity"
    bl_parent_id = "PHYSICS_PT_dynamic_paint"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    @classmethod
    def poll(cls, context):
        if not PhysicButtonsPanel.poll_dyn_canvas_brush(context):
            return False

        return (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=False)

        brush = context.dynamic_paint.brush_settings

        col = flow.column()
        col.prop(brush, "use_velocity_alpha")
        col.prop(brush, "use_velocity_color")

        col = flow.column()
        col.prop(brush, "use_velocity_depth")
        sub = col.column()
        sub.active = (brush.use_velocity_alpha or brush.use_velocity_color or brush.use_velocity_depth)
        sub.prop(brush, "velocity_max")


class PHYSICS_PT_dp_brush_velocity_color_ramp(PhysicButtonsPanel, Panel):
    bl_label = "Ramp"
    bl_parent_id = "PHYSICS_PT_dp_brush_velocity"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    @classmethod
    def poll(cls, context):
        if not PhysicButtonsPanel.poll_dyn_canvas_brush(context):
            return False

        return (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout

        brush = context.dynamic_paint.brush_settings

        layout.template_color_ramp(brush, "velocity_ramp", expand=True)


class PHYSICS_PT_dp_brush_velocity_smudge(PhysicButtonsPanel, Panel):
    bl_label = "Smudge"
    bl_parent_id = "PHYSICS_PT_dp_brush_velocity"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    @classmethod
    def poll(cls, context):
        if not PhysicButtonsPanel.poll_dyn_canvas_brush(context):
            return False

        return (context.engine in cls.COMPAT_ENGINES)

    def draw_header(self, context):
        brush = context.dynamic_paint.brush_settings

        self.layout.prop(brush, "use_smudge", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        brush = context.dynamic_paint.brush_settings

        layout.active = brush.use_smudge
        layout.prop(brush, "smudge_strength", text="Strength", slider=True)


class PHYSICS_PT_dp_brush_wave(PhysicButtonsPanel, Panel):
    bl_label = "Waves"
    bl_parent_id = "PHYSICS_PT_dynamic_paint"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    @classmethod
    def poll(cls, context):
        if not PhysicButtonsPanel.poll_dyn_canvas_brush(context):
            return False

        return (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        brush = context.dynamic_paint.brush_settings

        layout.prop(brush, "wave_type", text="Type")

        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=True)

        if brush.wave_type != 'REFLECT':
            col = flow.column()
            col.prop(brush, "wave_factor")

            col = flow.column()
            col.prop(brush, "wave_clamp")


classes = (
    PHYSICS_UL_dynapaint_surfaces,
    PHYSICS_PT_dynamic_paint,
    PHYSICS_PT_dynamic_paint_settings,
    PHYSICS_PT_dp_surface_canvas,
    PHYSICS_PT_dp_surface_canvas_paint_dissolve,
    PHYSICS_PT_dp_surface_canvas_paint_dry,
    PHYSICS_PT_dp_cache,
    PHYSICS_PT_dp_effects,
    PHYSICS_PT_dp_effects_spread,
    PHYSICS_PT_dp_effects_drip,
    PHYSICS_PT_dp_effects_drip_weights,
    PHYSICS_PT_dp_effects_shrink,
    PHYSICS_PT_dp_canvas_initial_color,
    PHYSICS_PT_dp_brush_source,
    PHYSICS_PT_dp_brush_source_color_ramp,
    PHYSICS_PT_dp_brush_velocity,
    PHYSICS_PT_dp_brush_velocity_color_ramp,
    PHYSICS_PT_dp_brush_velocity_smudge,
    PHYSICS_PT_dp_brush_wave,
    PHYSICS_PT_dp_canvas_output,
    PHYSICS_PT_dp_canvas_output_paintmaps,
    PHYSICS_PT_dp_canvas_output_wetmaps,
)


if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
