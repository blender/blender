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
from bpy.types import Menu, Panel, UIList
from bl_ui.properties_grease_pencil_common import (
    GreasePencilSculptOptionsPanel,
    GreasePencilDisplayPanel,
)
from bl_ui.properties_paint_common import (
    UnifiedPaintPanel,
    BrushSelectPanel,
    ClonePanel,
    TextureMaskPanel,
    ColorPalettePanel,
    StrokePanel,
    SmoothStrokePanel,
    FalloffPanel,
    DisplayPanel,
    brush_texture_settings,
    brush_mask_texture_settings,
    brush_settings,
    brush_settings_advanced,
    draw_color_settings,
)
from bl_ui.utils import PresetPanel


class VIEW3D_MT_brush_context_menu(Menu):
    bl_label = "Brush Specials"

    def draw(self, context):
        layout = self.layout

        settings = UnifiedPaintPanel.paint_settings(context)
        brush = getattr(settings, "brush", None)

        # skip if no active brush
        if not brush:
            layout.label(text="No Brushes currently available", icon='INFO')
            return

        # brush paint modes
        layout.menu("VIEW3D_MT_brush_paint_modes")

        # brush tool

        if context.image_paint_object:
            layout.prop_menu_enum(brush, "image_tool")
        elif context.vertex_paint_object:
            layout.prop_menu_enum(brush, "vertex_tool")
        elif context.weight_paint_object:
            layout.prop_menu_enum(brush, "weight_tool")
        elif context.sculpt_object:
            layout.prop_menu_enum(brush, "sculpt_tool")
            layout.operator("brush.reset")


class VIEW3D_MT_brush_context_menu_paint_modes(Menu):
    bl_label = "Enabled Modes"

    def draw(self, context):
        layout = self.layout

        settings = UnifiedPaintPanel.paint_settings(context)
        brush = settings.brush

        layout.prop(brush, "use_paint_sculpt", text="Sculpt")
        layout.prop(brush, "use_paint_uv_sculpt", text="UV Sculpt")
        layout.prop(brush, "use_paint_vertex", text="Vertex Paint")
        layout.prop(brush, "use_paint_weight", text="Weight Paint")
        layout.prop(brush, "use_paint_image", text="Texture Paint")


class View3DPanel:
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'


# **************** standard tool clusters ******************

# Used by vertex & weight paint
def draw_vpaint_symmetry(layout, vpaint):

    split = layout.split()

    col = split.column()
    col.alignment = 'RIGHT'
    col.label(text="Mirror")

    col = split.column()
    row = col.row(align=True)
    row.prop(vpaint, "use_symmetry_x", text="X", toggle=True)
    row.prop(vpaint, "use_symmetry_y", text="Y", toggle=True)
    row.prop(vpaint, "use_symmetry_z", text="Z", toggle=True)

    col = layout.column()
    col.use_property_split = True
    col.use_property_decorate = False
    col.prop(vpaint, "radial_symmetry", text="Radial")


# Most of these panels should not be visible in GP edit modes
def is_not_gpencil_edit_mode(context):
    is_gpmode = (
        context.active_object and
        context.active_object.mode in {'EDIT_GPENCIL', 'PAINT_GPENCIL', 'SCULPT_GPENCIL', 'WEIGHT_GPENCIL'}
    )
    return not is_gpmode


# ********** default tools for object mode ****************


class VIEW3D_PT_tools_object_options(View3DPanel, Panel):
    bl_category = "Tool"
    bl_context = ".objectmode"  # dot on purpose (access from topbar)
    bl_label = "Options"

    def draw(self, context):
        # layout = self.layout
        pass


class VIEW3D_PT_tools_object_options_transform(View3DPanel, Panel):
    bl_category = "Tool"
    bl_context = ".objectmode"  # dot on purpose (access from topbar)
    bl_label = "Transform"
    bl_parent_id = "VIEW3D_PT_tools_object_options"

    def draw(self, context):
        layout = self.layout

        layout.use_property_split = True
        layout.use_property_decorate = False

        tool_settings = context.tool_settings

        layout.label(text="Affect Only")
        layout.prop(tool_settings, "use_transform_data_origin", text="Origins")
        layout.prop(tool_settings, "use_transform_pivot_point_align", text="Locations")
        layout.prop(tool_settings, "use_transform_skip_children", text="Parents")


# ********** default tools for editmode_mesh ****************


class VIEW3D_PT_tools_meshedit_options(View3DPanel, Panel):
    bl_category = "Tool"
    bl_context = ".mesh_edit"  # dot on purpose (access from topbar)
    bl_label = "Options"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        return context.active_object

    def draw(self, context):
        layout = self.layout

        layout.use_property_split = True
        layout.use_property_decorate = False

        ob = context.active_object
        mesh = ob.data

        split = layout.split()

        col = split.column()
        col.alignment = 'RIGHT'
        col.label(text="Mirror")

        col = split.column()

        row = col.row(align=True)
        row.prop(mesh, "use_mirror_x", text="X", toggle=True)
        row.prop(mesh, "use_mirror_y", text="Y", toggle=True)
        row.prop(mesh, "use_mirror_z", text="Z", toggle=True)

        row = layout.row(align=True)
        row.active = ob.data.use_mirror_x or ob.data.use_mirror_y or ob.data.use_mirror_z
        row.prop(mesh, "use_mirror_topology")


class VIEW3D_PT_tools_meshedit_options_automerge(View3DPanel, Panel):
    bl_category = "Tool"
    bl_context = ".mesh_edit"  # dot on purpose (access from topbar)
    bl_label = "Auto Merge"
    bl_parent_id = "VIEW3D_PT_tools_meshedit_options"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        return context.active_object

    def draw_header(self, context):
        tool_settings = context.tool_settings

        self.layout.prop(tool_settings, "use_mesh_automerge", text="", toggle=False)

    def draw(self, context):
        layout = self.layout

        tool_settings = context.tool_settings

        layout.use_property_split = True
        layout.use_property_decorate = False

        col = layout.column(align=True)
        col.active = tool_settings.use_mesh_automerge
        col.prop(tool_settings, "use_mesh_automerge_and_split", toggle=False)
        col.prop(tool_settings, "double_threshold", text="Threshold")

# ********** default tools for editmode_curve ****************


class VIEW3D_PT_tools_curveedit_options_stroke(View3DPanel, Panel):
    bl_category = "Tool"
    bl_context = ".curve_edit"  # dot on purpose (access from topbar)
    bl_label = "Curve Stroke"

    def draw(self, context):
        layout = self.layout

        tool_settings = context.tool_settings
        cps = tool_settings.curve_paint_settings

        col = layout.column()

        col.prop(cps, "curve_type")

        if cps.curve_type == 'BEZIER':
            col.label(text="Bezier Options:")
            col.prop(cps, "error_threshold")
            col.prop(cps, "fit_method")
            col.prop(cps, "use_corners_detect")

            col = layout.column()
            col.active = cps.use_corners_detect
            col.prop(cps, "corner_angle")

        col.label(text="Pressure Radius:")
        row = layout.row(align=True)
        rowsub = row.row(align=True)
        rowsub.prop(cps, "radius_min", text="Min")
        rowsub.prop(cps, "radius_max", text="Max")

        row.prop(cps, "use_pressure_radius", text="", icon_only=True)

        col = layout.column()
        col.label(text="Taper Radius:")
        row = layout.row(align=True)
        row.prop(cps, "radius_taper_start", text="Start")
        row.prop(cps, "radius_taper_end", text="End")

        col = layout.column()
        col.label(text="Projection Depth:")
        row = layout.row(align=True)
        row.prop(cps, "depth_mode", expand=True)

        col = layout.column()
        if cps.depth_mode == 'SURFACE':
            col.prop(cps, "surface_offset")
            col.prop(cps, "use_offset_absolute")
            col.prop(cps, "use_stroke_endpoints")
            if cps.use_stroke_endpoints:
                colsub = layout.column(align=True)
                colsub.prop(cps, "surface_plane", expand=True)


# ********** default tools for editmode_armature ****************


class VIEW3D_PT_tools_armatureedit_options(View3DPanel, Panel):
    bl_category = "Tool"
    bl_context = ".armature_edit"  # dot on purpose (access from topbar)
    bl_label = "Options"

    def draw(self, context):
        arm = context.active_object.data

        self.layout.prop(arm, "use_mirror_x")


# ********** default tools for pose-mode ****************

class VIEW3D_PT_tools_posemode_options(View3DPanel, Panel):
    bl_category = "Tool"
    bl_context = ".posemode"  # dot on purpose (access from topbar)
    bl_label = "Pose Options"

    def draw(self, context):
        pose = context.active_object.pose
        layout = self.layout

        tool_settings = context.tool_settings

        layout.prop(pose, "use_auto_ik")
        layout.prop(pose, "use_mirror_x")
        col = layout.column()
        col.active = pose.use_mirror_x and not pose.use_auto_ik
        col.prop(pose, "use_mirror_relative")

        layout.label(text="Affect Only")
        layout.prop(tool_settings, "use_transform_pivot_point_align", text="Locations")

# ********** default tools for paint modes ****************


class TEXTURE_UL_texpaintslots(UIList):
    def draw_item(self, _context, layout, _data, item, icon, _active_data, _active_propname, _index):
        # mat = data

        if self.layout_type in {'DEFAULT', 'COMPACT'}:
            layout.prop(item, "name", text="", emboss=False, icon_value=icon)
        elif self.layout_type == 'GRID':
            layout.alignment = 'CENTER'
            layout.label(text="")


class View3DPaintPanel(View3DPanel, UnifiedPaintPanel):
    bl_category = "Tool"


class View3DPaintBrushPanel(View3DPaintPanel):
    @classmethod
    def poll(cls, context):
        mode = cls.get_brush_mode(context)
        return mode is not None


class VIEW3D_PT_tools_particlemode(Panel, View3DPaintPanel):
    bl_context = ".paint_common"  # dot on purpose (access from topbar)
    bl_label = "Particle Tool"
    bl_options = {'HIDE_HEADER'}

    @classmethod
    def poll(cls, context):
        settings = context.tool_settings.particle_edit
        return (settings and settings.brush and context.particle_edit_object)

    def draw(self, context):
        layout = self.layout

        settings = context.tool_settings.particle_edit
        brush = settings.brush
        tool = settings.tool

        layout.use_property_split = True
        layout.use_property_decorate = False  # No animation.

        if tool is not None:
            col = layout.column()
            col.prop(brush, "size", slider=True)
            if tool == 'ADD':
                col.prop(brush, "count")

                col = layout.column()
                col.prop(settings, "use_default_interpolate")
                col.prop(brush, "steps", slider=True)
                col.prop(settings, "default_key_count", slider=True)
            else:
                col.prop(brush, "strength", slider=True)

                if tool == 'LENGTH':
                    layout.row().prop(brush, "length_mode", expand=True)
                elif tool == 'PUFF':
                    layout.row().prop(brush, "puff_mode", expand=True)
                    layout.prop(brush, "use_puff_volume")
                elif tool == 'COMB':
                    layout.prop(settings, "use_emitter_deflect", text="Deflect Emitter")
                    col = layout.column()
                    col.active = settings.use_emitter_deflect
                    col.prop(settings, "emitter_distance", text="Distance")


# TODO, move to space_view3d.py
class VIEW3D_PT_tools_brush_select(Panel, View3DPaintBrushPanel, BrushSelectPanel):
    bl_context = ".paint_common"
    bl_label = "Brushes"


# TODO, move to space_view3d.py
class VIEW3D_PT_tools_brush_settings(Panel, View3DPaintBrushPanel):
    bl_context = ".paint_common"
    bl_label = "Brush Settings"

    @classmethod
    def poll(cls, context):
        settings = cls.paint_settings(context)
        return settings and settings.brush is not None

    def draw(self, context):
        layout = self.layout

        layout.use_property_split = True
        layout.use_property_decorate = False  # No animation.

        settings = self.paint_settings(context)
        brush = settings.brush

        brush_settings(layout.column(), context, brush, popover=self.is_popover)


class VIEW3D_PT_tools_brush_settings_advanced(Panel, View3DPaintBrushPanel):
    bl_context = ".paint_common"
    bl_parent_id = "VIEW3D_PT_tools_brush_settings"
    bl_label = "Advanced"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout

        layout.use_property_split = True
        layout.use_property_decorate = False  # No animation.

        settings = UnifiedPaintPanel.paint_settings(context)
        brush = settings.brush

        brush_settings_advanced(layout.column(), context, brush, self.is_popover)


class VIEW3D_PT_tools_brush_color(Panel, View3DPaintPanel):
    bl_context = ".paint_common"  # dot on purpose (access from topbar)
    bl_parent_id = "VIEW3D_PT_tools_brush_settings"
    bl_label = "Color Picker"

    @classmethod
    def poll(cls, context):
        settings = cls.paint_settings(context)
        brush = settings.brush

        if context.image_paint_object:
            capabilities = brush.image_paint_capabilities
            return capabilities.has_color
        elif context.vertex_paint_object:
            capabilities = brush.vertex_paint_capabilities
            return capabilities.has_color

        return False

    def draw(self, context):
        layout = self.layout
        settings = self.paint_settings(context)
        brush = settings.brush

        draw_color_settings(context, layout, brush, color_type=not context.vertex_paint_object)


class VIEW3D_PT_tools_brush_swatches(Panel, View3DPaintPanel, ColorPalettePanel):
    bl_context = ".paint_common"
    bl_parent_id = "VIEW3D_PT_tools_brush_settings"
    bl_label = "Color Palette"
    bl_options = {'DEFAULT_CLOSED'}


class VIEW3D_PT_tools_brush_clone(Panel, View3DPaintPanel, ClonePanel):
    bl_context = ".paint_common"
    bl_parent_id = "VIEW3D_PT_tools_brush_settings"
    bl_label = "Clone from Paint Slot"
    bl_options = {'DEFAULT_CLOSED'}


class VIEW3D_MT_tools_projectpaint_uvlayer(Menu):
    bl_label = "Clone Layer"

    def draw(self, context):
        layout = self.layout

        for i, uv_layer in enumerate(context.active_object.data.uv_layers):
            props = layout.operator("wm.context_set_int", text=uv_layer.name, translate=False)
            props.data_path = "active_object.data.uv_layers.active_index"
            props.value = i


class VIEW3D_PT_slots_projectpaint(View3DPanel, Panel):
    bl_category = "Tool"
    bl_context = ".imagepaint"  # dot on purpose (access from topbar)
    bl_label = "Texture Slots"

    @classmethod
    def poll(cls, context):
        brush = context.tool_settings.image_paint.brush
        return (brush is not None and context.active_object is not None)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        settings = context.tool_settings.image_paint

        ob = context.active_object

        layout.prop(settings, "mode", text="Mode")
        layout.separator()

        if settings.mode == 'MATERIAL':
            if len(ob.material_slots) > 1:
                layout.template_list("MATERIAL_UL_matslots", "layers",
                                     ob, "material_slots",
                                     ob, "active_material_index", rows=2)
            mat = ob.active_material
            if mat and mat.texture_paint_images:
                row = layout.row()
                row.template_list("TEXTURE_UL_texpaintslots", "",
                                  mat, "texture_paint_images",
                                  mat, "paint_active_slot", rows=2)

                if mat.texture_paint_slots:
                    slot = mat.texture_paint_slots[mat.paint_active_slot]
                else:
                    slot = None

                have_image = slot is not None
            else:
                row = layout.row()

                box = row.box()
                box.label(text="No Textures")
                have_image = False

            sub = row.column(align=True)
            sub.operator_menu_enum("paint.add_texture_paint_slot", "type", icon='ADD', text="")

        elif settings.mode == 'IMAGE':
            mesh = ob.data
            uv_text = mesh.uv_layers.active.name if mesh.uv_layers.active else ""
            layout.template_ID(settings, "canvas", new="image.new", open="image.open")
            if settings.missing_uvs:
                layout.operator("paint.add_simple_uvs", icon='ADD', text="Add UVs")
            else:
                layout.menu("VIEW3D_MT_tools_projectpaint_uvlayer", text=uv_text, translate=False)
            have_image = settings.canvas is not None

            layout.prop(settings, "interpolation", text="")

        if settings.missing_uvs:
            layout.separator()
            split = layout.split()
            split.label(text="UV Map Needed", icon='INFO')
            split.operator("paint.add_simple_uvs", icon='ADD', text="Add Simple UVs")
        elif have_image:
            layout.separator()
            layout.operator("image.save_all_modified", text="Save All Images", icon='FILE_TICK')


class VIEW3D_PT_mask(View3DPanel, Panel):
    bl_category = "Tool"
    bl_context = ".imagepaint"  # dot on purpose (access from topbar)
    bl_label = "Masking"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        pass


# TODO, move to space_view3d.py
class VIEW3D_PT_stencil_projectpaint(View3DPanel, Panel):
    bl_category = "Tool"
    bl_context = ".imagepaint"  # dot on purpose (access from topbar)
    bl_label = "Stencil Mask"
    bl_options = {'DEFAULT_CLOSED'}
    bl_parent_id = "VIEW3D_PT_mask"
    bl_ui_units_x = 14

    @classmethod
    def poll(cls, context):
        brush = context.tool_settings.image_paint.brush
        ob = context.active_object
        return (brush is not None and ob is not None)

    def draw_header(self, context):
        ipaint = context.tool_settings.image_paint
        self.layout.prop(ipaint, "use_stencil_layer", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        tool_settings = context.tool_settings
        ipaint = tool_settings.image_paint
        ob = context.active_object
        mesh = ob.data

        col = layout.column()
        col.active = ipaint.use_stencil_layer

        col.label(text="Stencil Image")
        col.template_ID(ipaint, "stencil_image", new="image.new", open="image.open")

        stencil_text = mesh.uv_layer_stencil.name if mesh.uv_layer_stencil else ""

        col.separator()

        split = col.split()
        colsub = split.column()
        colsub.alignment = 'RIGHT'
        colsub.label(text="UV Layer")
        split.column().menu("VIEW3D_MT_tools_projectpaint_stencil", text=stencil_text, translate=False)

        col.separator()

        row = col.row(align=True)
        row.prop(ipaint, "stencil_color", text="Display Color")
        row.prop(ipaint, "invert_stencil", text="", icon='IMAGE_ALPHA')


# TODO, move to space_view3d.py
class VIEW3D_PT_tools_brush_display(Panel, View3DPaintBrushPanel, DisplayPanel):
    bl_context = ".paint_common"
    bl_parent_id = "VIEW3D_PT_tools_brush_settings"
    bl_label = "Cursor"
    bl_options = {'DEFAULT_CLOSED'}


# TODO, move to space_view3d.py
class VIEW3D_PT_tools_brush_texture(Panel, View3DPaintPanel):
    bl_context = ".paint_common"
    bl_parent_id = "VIEW3D_PT_tools_brush_settings"
    bl_label = "Texture"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        settings = cls.paint_settings(context)
        return (settings and settings.brush and
                (context.sculpt_object or context.image_paint_object or context.vertex_paint_object))

    def draw(self, context):
        layout = self.layout

        settings = self.paint_settings(context)
        brush = settings.brush

        col = layout.column()

        col.template_ID_preview(brush, "texture", new="texture.new", rows=3, cols=8)

        brush_texture_settings(col, brush, context.sculpt_object)


# TODO, move to space_view3d.py
class VIEW3D_PT_tools_mask_texture(Panel, View3DPaintPanel, TextureMaskPanel):
    bl_category = "Tool"
    bl_context = ".imagepaint"  # dot on purpose (access from topbar)
    bl_parent_id = "VIEW3D_PT_tools_brush_settings"
    bl_label = "Texture Mask"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        settings = cls.paint_settings(context)
        return (settings and settings.brush and context.image_paint_object)

    def draw(self, context):
        layout = self.layout

        brush = context.tool_settings.image_paint.brush

        col = layout.column()

        col.template_ID_preview(brush, "mask_texture", new="texture.new", rows=3, cols=8)

        brush_mask_texture_settings(col, brush)


# TODO, move to space_view3d.py
class VIEW3D_PT_tools_brush_stroke(Panel, View3DPaintPanel, StrokePanel):
    bl_context = ".paint_common"  # dot on purpose (access from topbar)
    bl_label = "Stroke"
    bl_parent_id = "VIEW3D_PT_tools_brush_settings"
    bl_options = {'DEFAULT_CLOSED'}


class VIEW3D_PT_tools_brush_stroke_smooth_stroke(Panel, View3DPaintPanel, SmoothStrokePanel):
    bl_context = ".paint_common"  # dot on purpose (access from topbar)
    bl_label = "Stabilize Stroke"
    bl_parent_id = "VIEW3D_PT_tools_brush_stroke"
    bl_options = {'DEFAULT_CLOSED'}


# TODO, move to space_view3d.py
class VIEW3D_PT_tools_brush_falloff(Panel, View3DPaintPanel, FalloffPanel):
    bl_context = ".paint_common"  # dot on purpose (access from topbar)
    bl_parent_id = "VIEW3D_PT_tools_brush_settings"
    bl_label = "Falloff"
    bl_options = {'DEFAULT_CLOSED'}


class VIEW3D_PT_tools_brush_falloff_frontface(View3DPaintPanel, Panel):
    bl_context = ".imagepaint"  # dot on purpose (access from topbar)
    bl_label = "Front-face Falloff"
    bl_parent_id = "VIEW3D_PT_tools_brush_falloff"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        return (context.weight_paint_object or context.vertex_paint_object)

    def draw_header(self, context):
        settings = self.paint_settings(context)
        brush = settings.brush

        self.layout.prop(brush, "use_frontface_falloff", text="")

    def draw(self, context):
        settings = self.paint_settings(context)
        brush = settings.brush

        layout = self.layout

        layout.use_property_split = True
        layout.use_property_decorate = False

        layout.active = brush.use_frontface_falloff
        layout.prop(brush, "falloff_angle", text="Angle")


class VIEW3D_PT_tools_brush_falloff_normal(View3DPaintPanel, Panel):
    bl_context = ".imagepaint"  # dot on purpose (access from topbar)
    bl_label = "Normal Falloff"
    bl_parent_id = "VIEW3D_PT_tools_brush_falloff"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        return context.image_paint_object

    def draw_header(self, context):
        tool_settings = context.tool_settings
        ipaint = tool_settings.image_paint

        self.layout.prop(ipaint, "use_normal_falloff", text="")

    def draw(self, context):
        tool_settings = context.tool_settings
        ipaint = tool_settings.image_paint

        layout = self.layout

        layout.use_property_split = True
        layout.use_property_decorate = False

        layout.active = ipaint.use_normal_falloff
        layout.prop(ipaint, "normal_angle", text="Angle")


# TODO, move to space_view3d.py
class VIEW3D_PT_sculpt_dyntopo(Panel, View3DPaintPanel):
    bl_context = ".sculpt_mode"  # dot on purpose (access from topbar)
    bl_label = "Dyntopo"
    bl_options = {'DEFAULT_CLOSED'}
    bl_ui_units_x = 12

    @classmethod
    def poll(cls, context):
        return (context.sculpt_object and context.tool_settings.sculpt)

    def draw_header(self, context):
        is_popover = self.is_popover
        layout = self.layout
        layout.operator(
            "sculpt.dynamic_topology_toggle",
            icon='CHECKBOX_HLT' if context.sculpt_object.use_dynamic_topology_sculpting else 'CHECKBOX_DEHLT',
            text="",
            emboss=is_popover,
        )

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        tool_settings = context.tool_settings
        sculpt = tool_settings.sculpt
        settings = self.paint_settings(context)
        brush = settings.brush

        col = layout.column()
        col.active = context.sculpt_object.use_dynamic_topology_sculpting

        sub = col.column()
        sub.active = (brush and brush.sculpt_tool != 'MASK')
        if sculpt.detail_type_method in {'CONSTANT', 'MANUAL'}:
            row = sub.row(align=True)
            row.prop(sculpt, "constant_detail_resolution")
            props = row.operator("sculpt.sample_detail_size", text="", icon='EYEDROPPER')
            props.mode = 'DYNTOPO'
        elif (sculpt.detail_type_method == 'BRUSH'):
            sub.prop(sculpt, "detail_percent")
        else:
            sub.prop(sculpt, "detail_size")
        sub.prop(sculpt, "detail_refine_method", text="Refine Method")
        sub.prop(sculpt, "detail_type_method", text="Detailing")

        if sculpt.detail_type_method in {'CONSTANT', 'MANUAL'}:
            col.operator("sculpt.detail_flood_fill")

        col.prop(sculpt, "use_smooth_shading")






class VIEW3D_PT_sculpt_voxel_remesh(Panel, View3DPaintPanel):
    bl_context = ".sculpt_mode"  # dot on purpose (access from topbar)
    bl_label = "Remesh"
    bl_options = {'DEFAULT_CLOSED'}
    bl_ui_units_x = 12

    @classmethod
    def poll(cls, context):
        return (context.sculpt_object and context.tool_settings.sculpt)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        col = layout.column()
        mesh = context.active_object.data
        row = col.row(align=True)
        row.prop(mesh, "remesh_voxel_size")
        props = row.operator("sculpt.sample_detail_size", text="", icon='EYEDROPPER')
        props.mode = 'VOXEL'
        col.prop(mesh, "remesh_voxel_adaptivity")
        col.prop(mesh, "use_remesh_fix_poles")
        col.prop(mesh, "use_remesh_smooth_normals")
        col.prop(mesh, "use_remesh_preserve_volume")
        col.prop(mesh, "use_remesh_preserve_paint_mask")
        col.prop(mesh, "use_remesh_preserve_sculpt_face_sets")
        col.operator("object.voxel_remesh", text="Remesh")


# TODO, move to space_view3d.py
class VIEW3D_PT_sculpt_options(Panel, View3DPaintPanel):
    bl_context = ".sculpt_mode"  # dot on purpose (access from topbar)
    bl_label = "Options"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        return (context.sculpt_object and context.tool_settings.sculpt)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        tool_settings = context.tool_settings
        sculpt = tool_settings.sculpt

        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=False)

        col = flow.column()
        col.prop(sculpt, "use_threaded", text="Threaded Sculpt")
        col = flow.column()
        col.prop(sculpt, "show_low_resolution")
        col = flow.column()
        col.prop(sculpt, "use_deform_only")


class VIEW3D_PT_sculpt_options_gravity(Panel, View3DPaintPanel):
    bl_context = ".sculpt_mode"  # dot on purpose (access from topbar)
    bl_parent_id = "VIEW3D_PT_sculpt_options"
    bl_label = "Gravity"

    @classmethod
    def poll(cls, context):
        return (context.sculpt_object and context.tool_settings.sculpt)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        tool_settings = context.tool_settings
        sculpt = tool_settings.sculpt
        capabilities = sculpt.brush.sculpt_capabilities

        col = layout.column()
        col.active = capabilities.has_gravity
        col.prop(sculpt, "gravity", slider=True, text="Factor")
        col.prop(sculpt, "gravity_object")


# TODO, move to space_view3d.py
class VIEW3D_PT_sculpt_symmetry(Panel, View3DPaintPanel):
    bl_context = ".sculpt_mode"  # dot on purpose (access from topbar)
    bl_label = "Symmetry"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        return (
            (context.sculpt_object and context.tool_settings.sculpt) and
            # When used in the tool header, this is explicitly included next to the XYZ symmetry buttons.
            (context.region.type != 'TOOL_HEADER')
        )

    def draw(self, context):
        layout = self.layout

        sculpt = context.tool_settings.sculpt

        split = layout.split()

        col = split.column()
        col.alignment = 'RIGHT'
        col.label(text="Mirror")

        col = split.column()

        row = col.row(align=True)
        row.prop(sculpt, "use_symmetry_x", text="X", toggle=True)
        row.prop(sculpt, "use_symmetry_y", text="Y", toggle=True)
        row.prop(sculpt, "use_symmetry_z", text="Z", toggle=True)

        split = layout.split()

        col = split.column()
        col.alignment = 'RIGHT'
        col.label(text="Lock")

        col = split.column()

        row = col.row(align=True)
        row.prop(sculpt, "lock_x", text="X", toggle=True)
        row.prop(sculpt, "lock_y", text="Y", toggle=True)
        row.prop(sculpt, "lock_z", text="Z", toggle=True)

        split = layout.split()

        col = split.column()
        col.alignment = 'RIGHT'
        col.label(text="Tiling")

        col = split.column()

        row = col.row(align=True)
        row.prop(sculpt, "tile_x", text="X", toggle=True)
        row.prop(sculpt, "tile_y", text="Y", toggle=True)
        row.prop(sculpt, "tile_z", text="Z", toggle=True)

        layout.use_property_split = True
        layout.use_property_decorate = False

        layout.prop(sculpt, "use_symmetry_feather", text="Feather")
        layout.column().prop(sculpt, "radial_symmetry", text="Radial")
        layout.column().prop(sculpt, "tile_offset", text="Tile Offset")

        layout.separator()

        col = layout.column()

        col.prop(sculpt, "symmetrize_direction")
        col.operator("sculpt.symmetrize")


class VIEW3D_PT_sculpt_symmetry_for_topbar(Panel):
    bl_space_type = 'TOPBAR'
    bl_region_type = 'HEADER'
    bl_label = "Symmetry"

    draw = VIEW3D_PT_sculpt_symmetry.draw


# ********** default tools for weight-paint ****************


# TODO, move to space_view3d.py
class VIEW3D_PT_tools_weightpaint_symmetry(Panel, View3DPaintPanel):
    bl_context = ".weightpaint"
    bl_options = {'DEFAULT_CLOSED'}
    bl_label = "Symmetry"

    @classmethod
    def poll(cls, context):
        # When used in the tool header, this is explicitly included next to the XYZ symmetry buttons.
        return (context.region.type != 'TOOL_HEADER')

    def draw(self, context):
        layout = self.layout
        tool_settings = context.tool_settings
        wpaint = tool_settings.weight_paint
        draw_vpaint_symmetry(layout, wpaint)


class VIEW3D_PT_tools_weightpaint_symmetry_for_topbar(Panel):
    bl_space_type = 'TOPBAR'
    bl_region_type = 'HEADER'
    bl_label = "Symmetry"

    draw = VIEW3D_PT_tools_weightpaint_symmetry.draw


# TODO, move to space_view3d.py
class VIEW3D_PT_tools_weightpaint_options(Panel, View3DPaintPanel):
    bl_context = ".weightpaint"
    bl_label = "Options"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout

        layout.use_property_split = True
        layout.use_property_decorate = False

        tool_settings = context.tool_settings
        wpaint = tool_settings.weight_paint

        col = layout.column()

        col.prop(tool_settings, "use_auto_normalize", text="Auto Normalize")
        col.prop(tool_settings, "use_multipaint", text="Multi-Paint")

        col.prop(wpaint, "use_group_restrict")

        obj = context.weight_paint_object
        if obj.type == 'MESH':
            mesh = obj.data
            col.prop(mesh, "use_mirror_x")
            row = col.row()
            row.active = mesh.use_mirror_x
            row.prop(mesh, "use_mirror_topology")


# ********** default tools for vertex-paint ****************


# TODO, move to space_view3d.py
class VIEW3D_PT_tools_vertexpaint_options(Panel, View3DPaintPanel):
    bl_context = ".vertexpaint"  # dot on purpose (access from topbar)
    bl_label = "Options"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(self, _context):
        # This is currently unused, since there aren't any Vertex Paint mode specific options.
        return False

    def draw(self, _context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False


# TODO, move to space_view3d.py
class VIEW3D_PT_tools_vertexpaint_symmetry(Panel, View3DPaintPanel):
    bl_context = ".vertexpaint"  # dot on purpose (access from topbar)
    bl_options = {'DEFAULT_CLOSED'}
    bl_label = "Symmetry"

    @classmethod
    def poll(cls, context):
        # When used in the tool header, this is explicitly included next to the XYZ symmetry buttons.
        return (context.region.type != 'TOOL_HEADER')

    def draw(self, context):
        layout = self.layout
        tool_settings = context.tool_settings
        vpaint = tool_settings.vertex_paint
        draw_vpaint_symmetry(layout, vpaint)


class VIEW3D_PT_tools_vertexpaint_symmetry_for_topbar(Panel):
    bl_space_type = 'TOPBAR'
    bl_region_type = 'HEADER'
    bl_label = "Symmetry"

    draw = VIEW3D_PT_tools_vertexpaint_symmetry.draw


# ********** default tools for texture-paint ****************


# TODO, move to space_view3d.py
class VIEW3D_PT_tools_imagepaint_options_external(Panel, View3DPaintPanel):
    bl_context = ".imagepaint"  # dot on purpose (access from topbar)
    bl_label = "External"
    bl_parent_id = "VIEW3D_PT_tools_imagepaint_options"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        tool_settings = context.tool_settings
        ipaint = tool_settings.image_paint

        layout.prop(ipaint, "screen_grab_size", text="Screen Grab Size")

        layout.separator()

        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=False)
        col = flow.column()
        col.operator("image.project_edit", text="Quick Edit")
        col = flow.column()
        col.operator("image.project_apply", text="Apply")
        col = flow.column()
        col.operator("paint.project_image", text="Apply Camera Image")


# TODO, move to space_view3d.py
class VIEW3D_PT_tools_imagepaint_symmetry(Panel, View3DPaintPanel):
    bl_context = ".imagepaint"  # dot on purpose (access from topbar)
    bl_label = "Symmetry"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        # When used in the tool header, this is explicitly included next to the XYZ symmetry buttons.
        return (context.region.type != 'TOOL_HEADER')

    def draw(self, context):
        layout = self.layout

        tool_settings = context.tool_settings
        ipaint = tool_settings.image_paint

        split = layout.split()

        col = split.column()
        col.alignment = 'RIGHT'
        col.label(text="Mirror")

        col = split.column()

        row = col.row(align=True)
        row.prop(ipaint, "use_symmetry_x", text="X", toggle=True)
        row.prop(ipaint, "use_symmetry_y", text="Y", toggle=True)
        row.prop(ipaint, "use_symmetry_z", text="Z", toggle=True)


# TODO, move to space_view3d.py
class VIEW3D_PT_tools_imagepaint_options(View3DPaintPanel, Panel):
    bl_context = ".imagepaint"  # dot on purpose (access from topbar)
    bl_label = "Options"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        brush = context.tool_settings.image_paint.brush
        return (brush is not None)

    def draw(self, context):
        layout = self.layout

        layout.use_property_split = True
        layout.use_property_decorate = False

        tool_settings = context.tool_settings
        ipaint = tool_settings.image_paint

        layout.prop(ipaint, "seam_bleed")
        layout.prop(ipaint, "dither", slider=True)

        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=False)

        col = flow.column()
        col.prop(ipaint, "use_occlude")

        col = flow.column()
        col.prop(ipaint, "use_backface_culling", text="Backface Culling")


class VIEW3D_PT_tools_imagepaint_options_cavity(View3DPaintPanel, Panel):
    bl_context = ".imagepaint"  # dot on purpose (access from topbar)
    bl_label = "Cavity Mask"
    bl_parent_id = "VIEW3D_PT_mask"
    bl_options = {'DEFAULT_CLOSED'}

    def draw_header(self, context):
        tool_settings = context.tool_settings
        ipaint = tool_settings.image_paint

        self.layout.prop(ipaint, "use_cavity", text="")

    def draw(self, context):
        layout = self.layout

        tool_settings = context.tool_settings
        ipaint = tool_settings.image_paint

        layout.active = ipaint.use_cavity

        layout.template_curve_mapping(ipaint, "cavity_curve", brush=True,
                                      use_negative_slope=True)


# TODO, move to space_view3d.py
class VIEW3D_PT_imagepaint_options(View3DPaintPanel):
    bl_label = "Options"

    @classmethod
    def poll(cls, _context):
        # This is currently unused, since there aren't any Vertex Paint mode specific options.
        return False
        # return (context.image_paint_object and context.tool_settings.image_paint)

    def draw(self, _context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False


class VIEW3D_MT_tools_projectpaint_stencil(Menu):
    bl_label = "Mask Layer"

    def draw(self, context):
        layout = self.layout
        for i, uv_layer in enumerate(context.active_object.data.uv_layers):
            props = layout.operator("wm.context_set_int", text=uv_layer.name, translate=False)
            props.data_path = "active_object.data.uv_layer_stencil_index"
            props.value = i


# TODO, move to space_view3d.py
class VIEW3D_PT_tools_particlemode_options(View3DPanel, Panel):
    """Default tools for particle mode"""
    bl_category = "Tool"
    bl_context = ".particlemode"
    bl_label = "Options"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout

        layout.use_property_split = True
        layout.use_property_decorate = False  # No animation.

        pe = context.tool_settings.particle_edit
        ob = pe.object

        layout.prop(pe, "type", text="Editing Type")

        ptcache = None

        if pe.type == 'PARTICLES':
            if ob.particle_systems:
                if len(ob.particle_systems) > 1:
                    layout.template_list("UI_UL_list", "particle_systems", ob, "particle_systems",
                                         ob.particle_systems, "active_index", rows=2, maxrows=3)

                ptcache = ob.particle_systems.active.point_cache
        else:
            for md in ob.modifiers:
                if md.type == pe.type:
                    ptcache = md.point_cache

        if ptcache and len(ptcache.point_caches) > 1:
            layout.template_list("UI_UL_list", "particles_point_caches", ptcache, "point_caches",
                                 ptcache.point_caches, "active_index", rows=2, maxrows=3)

        if not pe.is_editable:
            layout.label(text="Point cache must be baked")
            layout.label(text="in memory to enable editing!")

        col = layout.column(align=True)
        col.active = pe.is_editable
        col.prop(ob.data, "use_mirror_x")
        if pe.tool == 'ADD':
          col.prop(ob.data, "use_mirror_topology")
        col.separator()
        col.prop(pe, "use_preserve_length", text="Preserve Strand Lengths")
        col.prop(pe, "use_preserve_root", text="Preserve Root Positions")
        if not pe.is_hair:
            col.prop(pe, "use_auto_velocity", text="Auto-Velocity")


class VIEW3D_PT_tools_particlemode_options_shapecut(View3DPanel, Panel):
    """Default tools for particle mode"""
    bl_category = "Tool"
    bl_parent_id = "VIEW3D_PT_tools_particlemode_options"
    bl_label = "Cut Particles to Shape"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout

        layout.use_property_split = True
        layout.use_property_decorate = False  # No animation.

        pe = context.tool_settings.particle_edit

        layout.prop(pe, "shape_object")
        layout.operator("particle.shape_cut", text="Cut")


class VIEW3D_PT_tools_particlemode_options_display(View3DPanel, Panel):
    """Default tools for particle mode"""
    bl_category = "Tool"
    bl_parent_id = "VIEW3D_PT_tools_particlemode_options"
    bl_label = "Viewport Display"

    def draw(self, context):
        layout = self.layout

        layout.use_property_split = True
        layout.use_property_decorate = False  # No animation.

        pe = context.tool_settings.particle_edit

        col = layout.column()
        col.active = pe.is_editable
        col.prop(pe, "display_step", text="Path Steps")
        if pe.is_hair:
            col.prop(pe, "show_particles", text="Children")
        else:
            if pe.type == 'PARTICLES':
                col.prop(pe, "show_particles", text="Particles")
            col.prop(pe, "use_fade_time")
            sub = col.row(align=True)
            sub.active = pe.use_fade_time
            sub.prop(pe, "fade_frames", slider=True)


# ********** grease pencil object tool panels ****************

# Grease Pencil drawing brushes


class GreasePencilPanel:
    bl_context = ".greasepencil_paint"
    bl_category = "Tool"

    @classmethod
    def poll(cls, context):
        if context.space_data.type in {'VIEW_3D', 'PROPERTIES'}:
            if context.gpencil_data is None:
                return False

            gpd = context.gpencil_data
            return bool(gpd.is_stroke_paint_mode)
        else:
            return True


class VIEW3D_PT_tools_grease_pencil_brush_select(Panel, View3DPanel, GreasePencilPanel):
    bl_label = "Brushes"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        tool_settings = context.scene.tool_settings
        gpencil_paint = tool_settings.gpencil_paint

        row = layout.row()
        row.column().template_ID_preview(gpencil_paint, "brush", new="brush.add_gpencil", rows=3, cols=8)

        col = row.column()
        col.operator("gpencil.brush_presets_create", icon='PRESET_NEW', text="")

        if context.mode == 'PAINT_GPENCIL':
            brush = tool_settings.gpencil_paint.brush
            if brush is not None:
                gp_settings = brush.gpencil_settings

                col.prop(brush, "use_custom_icon", toggle=True, icon='FILE_IMAGE', text="")

                if brush.use_custom_icon:
                    layout.row().prop(brush, "icon_filepath", text="")


class VIEW3D_PT_tools_grease_pencil_brush_settings(Panel, View3DPanel, GreasePencilPanel):
    bl_label = "Brush Settings"

    # What is the point of brush presets? Seems to serve the exact same purpose as brushes themselves??
    def draw_header_preset(self, _context):
        VIEW3D_PT_gpencil_brush_presets.draw_panel_header(self.layout)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        tool_settings = context.scene.tool_settings
        gpencil_paint = tool_settings.gpencil_paint

        brush = gpencil_paint.brush

        if brush is not None:
            gp_settings = brush.gpencil_settings

            if brush.gpencil_tool in {'DRAW', 'FILL'}:
                row = layout.row(align=True)
                row_mat = row.row()
                if gp_settings.use_material_pin:
                    row_mat.template_ID(gp_settings, "material", live_icon=True)
                else:
                    row_mat.template_ID(context.active_object, "active_material", live_icon=True)
                    row_mat.enabled = False  # will otherwise allow to change material in active slot

                row.prop(gp_settings, "use_material_pin", text="")

            if not self.is_popover:
                from bl_ui.properties_paint_common import (
                    brush_basic_gpencil_paint_settings,
                )
                brush_basic_gpencil_paint_settings(layout, context, brush, compact=False)


class VIEW3D_PT_tools_grease_pencil_brush_advanced(View3DPanel, Panel):
    bl_context = ".greasepencil_paint"
    bl_label = "Advanced"
    bl_parent_id = 'VIEW3D_PT_tools_grease_pencil_brush_settings'
    bl_category = "Tool"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        brush = context.tool_settings.gpencil_paint.brush
        return brush is not None and brush.gpencil_tool != 'ERASE'

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        tool_settings = context.scene.tool_settings
        gpencil_paint = tool_settings.gpencil_paint
        brush = gpencil_paint.brush
        gp_settings = brush.gpencil_settings

        col = layout.column(align=True)
        if brush is not None:
            if brush.gpencil_tool != 'FILL':
                col.prop(gp_settings, "input_samples")
                col.separator()

                col.prop(gp_settings, "active_smooth_factor")
                col.separator()

                col.prop(gp_settings, "angle", slider=True)
                col.prop(gp_settings, "angle_factor", text="Factor", slider=True)

                ob = context.object
                ma = None
                if ob and brush.gpencil_settings.use_material_pin is False:
                    ma = ob.active_material
                elif brush.gpencil_settings.material:
                    ma = brush.gpencil_settings.material

                col.separator()
                subcol = col.column(align=True)
                if ma and ma.grease_pencil.mode == 'LINE':
                    subcol.enabled = False
                subcol.prop(gp_settings, "gradient_factor", slider=True)
                subcol.prop(gp_settings, "gradient_shape")

            elif brush.gpencil_tool == 'FILL':
                row = col.row(align=True)
                row.prop(gp_settings, "fill_draw_mode", text="Boundary")
                row.prop(gp_settings, "show_fill_boundary", text="", icon='GRID')
                col.separator()
                col.prop(gp_settings, "fill_factor", text="Resolution")
                if gp_settings.fill_draw_mode != 'STROKE':
                    col.prop(gp_settings, "show_fill", text="Ignore Transparent Strokes")
                    col.prop(gp_settings, "fill_threshold", text="Threshold")

class VIEW3D_PT_tools_grease_pencil_brush_stroke(Panel, View3DPanel):
    bl_context = ".greasepencil_paint"
    bl_parent_id = 'VIEW3D_PT_tools_grease_pencil_brush_settings'
    bl_label = "Stroke"
    bl_category = "Tool"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        brush = context.tool_settings.gpencil_paint.brush
        return brush is not None and brush.gpencil_tool == 'DRAW'

    def draw(self, context):
        layout = self.layout


class VIEW3D_PT_tools_grease_pencil_brush_stabilizer(Panel, View3DPanel):
    bl_context = ".greasepencil_paint"
    bl_parent_id = 'VIEW3D_PT_tools_grease_pencil_brush_stroke'
    bl_label = "Stabilize Stroke"
    bl_category = "Tool"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        brush = context.tool_settings.gpencil_paint.brush
        return brush is not None and brush.gpencil_tool == 'DRAW'

    def draw_header(self, context):
        if self.is_popover:
            return

        brush = context.tool_settings.gpencil_paint.brush
        gp_settings = brush.gpencil_settings
        self.layout.prop(gp_settings, "use_settings_stabilizer", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        brush = context.tool_settings.gpencil_paint.brush
        gp_settings = brush.gpencil_settings

        if self.is_popover:
            row = layout.row()
            row.prop(gp_settings, "use_settings_stabilizer", text="")
            row.label(text=self.bl_label)

        col = layout.column()
        col.active = gp_settings.use_settings_stabilizer

        col.prop(brush, "smooth_stroke_radius", text="Radius", slider=True)
        col.prop(brush, "smooth_stroke_factor", text="Factor", slider=True)


class VIEW3D_PT_tools_grease_pencil_brush_post_processing(View3DPanel, Panel):
    bl_context = ".greasepencil_paint"
    bl_parent_id = 'VIEW3D_PT_tools_grease_pencil_brush_stroke'
    bl_label = "Post-Processing"
    bl_category = "Tool"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        brush = context.tool_settings.gpencil_paint.brush
        return brush is not None and brush.gpencil_tool not in {'ERASE', 'FILL'}

    def draw_header(self, context):
        if self.is_popover:
            return

        brush = context.tool_settings.gpencil_paint.brush
        gp_settings = brush.gpencil_settings
        self.layout.prop(gp_settings, "use_settings_postprocess", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        brush = context.tool_settings.gpencil_paint.brush
        gp_settings = brush.gpencil_settings

        if self.is_popover:
            row = layout.row()
            row.prop(gp_settings, "use_settings_postprocess", text="")
            row.label(text=self.bl_label)

        col = layout.column()
        col.active = gp_settings.use_settings_postprocess

        col1 = col.column(align=True)
        col1.prop(gp_settings, "pen_smooth_factor")
        col1.prop(gp_settings, "pen_smooth_steps")

        col1 = col.column(align=True)
        col1.prop(gp_settings, "pen_thick_smooth_factor")
        col1.prop(gp_settings, "pen_thick_smooth_steps", text="Iterations")

        col1 = col.column(align=True)
        col1.prop(gp_settings, "pen_subdivision_steps")
        col1.prop(gp_settings, "random_subdiv", text="Randomness", slider=True)

        col1 = col.column(align=True)
        col1.prop(gp_settings, "simplify_factor")

        col1 = col.column(align=True)
        col1.prop(gp_settings, "trim")


class VIEW3D_PT_tools_grease_pencil_brush_random(View3DPanel, Panel):
    bl_context = ".greasepencil_paint"
    bl_parent_id = 'VIEW3D_PT_tools_grease_pencil_brush_stroke'
    bl_label = "Randomize"
    bl_category = "Tool"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        brush = context.tool_settings.gpencil_paint.brush
        return brush is not None and brush.gpencil_tool not in {'ERASE', 'FILL'}

    def draw_header(self, context):
        if self.is_popover:
            return

        brush = context.tool_settings.gpencil_paint.brush
        gp_settings = brush.gpencil_settings
        self.layout.prop(gp_settings, "use_settings_random", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        brush = context.tool_settings.gpencil_paint.brush
        gp_settings = brush.gpencil_settings

        if self.is_popover:
            row = layout.row()
            row.prop(gp_settings, "use_settings_random", text="")
            row.label(text=self.bl_label)

        col = layout.column()
        col.active = gp_settings.use_settings_random

        col.prop(gp_settings, "random_pressure", text="Pressure", slider=True)
        col.prop(gp_settings, "random_strength", text="Strength", slider=True)
        col.prop(gp_settings, "uv_random", text="UV", slider=True)

        row = col.row(align=True)
        row.prop(gp_settings, "pen_jitter", slider=True)
        row.prop(gp_settings, "use_jitter_pressure", text="", icon='STYLUS_PRESSURE')


# Grease Pencil drawingcurves
class VIEW3D_PT_tools_grease_pencil_brushcurves(View3DPanel, Panel):
    bl_context = ".greasepencil_paint"
    bl_parent_id = 'VIEW3D_PT_tools_grease_pencil_brush_settings'
    bl_label = "Curves"
    bl_category = "Tool"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        brush = context.tool_settings.gpencil_paint.brush
        return brush is not None and brush.gpencil_tool not in {'ERASE', 'FILL'}

    def draw(self, context):
        pass


class VIEW3D_PT_tools_grease_pencil_brushcurves_sensitivity(View3DPanel, Panel):
    bl_context = ".greasepencil_paint"
    bl_label = "Sensitivity"
    bl_category = "Tool"
    bl_parent_id = "VIEW3D_PT_tools_grease_pencil_brushcurves"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        brush = context.tool_settings.gpencil_paint.brush
        gp_settings = brush.gpencil_settings

        layout.template_curve_mapping(gp_settings, "curve_sensitivity", brush=True,
                                      use_negative_slope=True)


class VIEW3D_PT_tools_grease_pencil_brushcurves_strength(View3DPanel, Panel):
    bl_context = ".greasepencil_paint"
    bl_label = "Strength"
    bl_category = "Tool"
    bl_parent_id = "VIEW3D_PT_tools_grease_pencil_brushcurves"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        brush = context.tool_settings.gpencil_paint.brush
        gp_settings = brush.gpencil_settings

        layout.template_curve_mapping(gp_settings, "curve_strength", brush=True,
                                      use_negative_slope=True)


class VIEW3D_PT_tools_grease_pencil_brushcurves_jitter(View3DPanel, Panel):
    bl_context = ".greasepencil_paint"
    bl_label = "Jitter"
    bl_category = "Tool"
    bl_parent_id = "VIEW3D_PT_tools_grease_pencil_brushcurves"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        brush = context.tool_settings.gpencil_paint.brush
        gp_settings = brush.gpencil_settings

        layout.template_curve_mapping(gp_settings, "curve_jitter", brush=True,
                                      use_negative_slope=True)


# Grease Pencil stroke interpolation tools
class VIEW3D_PT_tools_grease_pencil_interpolate(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'
    bl_label = "Interpolate"

    @classmethod
    def poll(cls, context):
        if context.gpencil_data is None:
            return False

        gpd = context.gpencil_data
        return bool(context.editable_gpencil_strokes) and bool(gpd.use_stroke_edit_mode)

    def draw(self, context):
        layout = self.layout
        settings = context.tool_settings.gpencil_interpolate

        col = layout.column(align=True)
        col.label(text="Interpolate Strokes")
        col.operator("gpencil.interpolate", text="Interpolate")
        col.operator("gpencil.interpolate_sequence", text="Sequence")
        col.operator("gpencil.interpolate_reverse", text="Remove Breakdowns")

        col = layout.column(align=True)
        col.label(text="Options:")
        col.prop(settings, "interpolate_all_layers")
        col.prop(settings, "interpolate_selected_only")

        col = layout.column(align=True)
        col.label(text="Sequence Options:")
        col.prop(settings, "type")
        if settings.type == 'CUSTOM':
            # TODO: Options for loading/saving curve presets?
            col.template_curve_mapping(settings, "interpolation_curve", brush=True,
                                       use_negative_slope=True)
        elif settings.type != 'LINEAR':
            col.prop(settings, "easing")

            if settings.type == 'BACK':
                layout.prop(settings, "back")
            elif settings.type == 'ELASTIC':
                sub = layout.column(align=True)
                sub.prop(settings, "amplitude")
                sub.prop(settings, "period")


# Grease Pencil stroke sculpting tools

class VIEW3D_PT_tools_grease_pencil_sculpt_select(Panel, View3DPanel):
    bl_context = ".greasepencil_sculpt"
    bl_label = "Brushes"
    bl_category = "Tool"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        settings = context.tool_settings.gpencil_sculpt

        layout.template_icon_view(settings, "sculpt_tool", show_labels=True)


class VIEW3D_PT_tools_grease_pencil_sculpt_settings(Panel, View3DPanel):
    bl_context = ".greasepencil_sculpt"
    bl_category = "Tool"
    bl_label = "Brush Settings"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        settings = context.tool_settings.gpencil_sculpt
        brush = settings.brush

        if not self.is_popover:
            from bl_ui.properties_paint_common import (
                brush_basic_gpencil_sculpt_settings,
            )
            brush_basic_gpencil_sculpt_settings(layout, context, brush)

# Grease Pencil weight painting tools


class VIEW3D_PT_tools_grease_pencil_weight_paint_select(View3DPanel, Panel):
    bl_context = ".greasepencil_weight"
    bl_label = "Brushes"
    bl_category = "Tool"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        settings = context.tool_settings.gpencil_sculpt

        layout.template_icon_view(settings, "weight_tool", show_labels=True)


class VIEW3D_PT_tools_grease_pencil_weight_paint_settings(Panel, View3DPanel):
    bl_context = ".greasepencil_weight"
    bl_category = "Tool"
    bl_label = "Brush Settings"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        settings = context.tool_settings.gpencil_sculpt
        brush = settings.brush

        if not self.is_popover:
            from bl_ui.properties_paint_common import (
                brush_basic_gpencil_weight_settings,
            )
            brush_basic_gpencil_weight_settings(layout, context, brush)


class VIEW3D_PT_tools_grease_pencil_sculpt_options(GreasePencilSculptOptionsPanel, Panel, View3DPanel):
    bl_context = ".greasepencil_sculpt"
    bl_parent_id = 'VIEW3D_PT_tools_grease_pencil_sculpt_settings'
    bl_category = "Tool"
    bl_label = "Sculpt Strokes"


# Grease Pencil Brush Appearance (one for each mode)
class VIEW3D_PT_tools_grease_pencil_paint_appearance(GreasePencilDisplayPanel, Panel, View3DPanel):
    bl_context = ".greasepencil_paint"
    bl_parent_id = 'VIEW3D_PT_tools_grease_pencil_brush_settings'
    bl_label = "Cursor"
    bl_category = "Tool"


class VIEW3D_PT_tools_grease_pencil_sculpt_appearance(GreasePencilDisplayPanel, Panel, View3DPanel):
    bl_context = ".greasepencil_sculpt"
    bl_parent_id = 'VIEW3D_PT_tools_grease_pencil_sculpt_settings'
    bl_label = "Cursor"
    bl_category = "Tool"


class VIEW3D_PT_tools_grease_pencil_weight_appearance(GreasePencilDisplayPanel, Panel, View3DPanel):
    bl_context = ".greasepencil_weight"
    bl_parent_id = 'VIEW3D_PT_tools_grease_pencil_weight_paint_settings'
    bl_category = "Tool"
    bl_label = "Cursor"


class VIEW3D_PT_gpencil_brush_presets(Panel, PresetPanel):
    """Brush settings"""
    bl_label = "Brush Presets"
    preset_subdir = "gpencil_brush"
    preset_operator = "script.execute_preset"
    preset_add_operator = "scene.gpencil_brush_preset_add"


classes = (
    VIEW3D_MT_brush_context_menu,
    VIEW3D_MT_brush_context_menu_paint_modes,
    VIEW3D_PT_tools_object_options,
    VIEW3D_PT_tools_object_options_transform,
    VIEW3D_PT_tools_meshedit_options,
    VIEW3D_PT_tools_meshedit_options_automerge,
    VIEW3D_PT_tools_curveedit_options_stroke,
    VIEW3D_PT_tools_armatureedit_options,
    VIEW3D_PT_tools_posemode_options,

    VIEW3D_PT_slots_projectpaint,
    VIEW3D_PT_tools_brush_select,
    VIEW3D_PT_tools_brush_settings,
    VIEW3D_PT_tools_brush_color,
    VIEW3D_PT_tools_brush_swatches,
    VIEW3D_PT_tools_brush_settings_advanced,
    VIEW3D_PT_tools_brush_clone,
    TEXTURE_UL_texpaintslots,
    VIEW3D_MT_tools_projectpaint_uvlayer,
    VIEW3D_PT_tools_brush_texture,
    VIEW3D_PT_tools_mask_texture,
    VIEW3D_PT_tools_brush_stroke,
    VIEW3D_PT_tools_brush_stroke_smooth_stroke,
    VIEW3D_PT_tools_brush_falloff,
    VIEW3D_PT_tools_brush_falloff_frontface,
    VIEW3D_PT_tools_brush_falloff_normal,
    VIEW3D_PT_tools_brush_display,

    VIEW3D_PT_sculpt_dyntopo,
    VIEW3D_PT_sculpt_voxel_remesh,
    VIEW3D_PT_sculpt_symmetry,
    VIEW3D_PT_sculpt_symmetry_for_topbar,
    VIEW3D_PT_sculpt_options,
    VIEW3D_PT_sculpt_options_gravity,

    VIEW3D_PT_tools_weightpaint_symmetry,
    VIEW3D_PT_tools_weightpaint_symmetry_for_topbar,
    VIEW3D_PT_tools_weightpaint_options,

    VIEW3D_PT_tools_vertexpaint_symmetry,
    VIEW3D_PT_tools_vertexpaint_symmetry_for_topbar,
    VIEW3D_PT_tools_vertexpaint_options,

    VIEW3D_PT_mask,
    VIEW3D_PT_stencil_projectpaint,
    VIEW3D_PT_tools_imagepaint_options_cavity,

    VIEW3D_PT_tools_imagepaint_symmetry,
    VIEW3D_PT_tools_imagepaint_options,

    VIEW3D_PT_tools_imagepaint_options_external,
    VIEW3D_MT_tools_projectpaint_stencil,

    VIEW3D_PT_tools_particlemode,
    VIEW3D_PT_tools_particlemode_options,
    VIEW3D_PT_tools_particlemode_options_shapecut,
    VIEW3D_PT_tools_particlemode_options_display,

    VIEW3D_PT_gpencil_brush_presets,
    VIEW3D_PT_tools_grease_pencil_brush_select,
    VIEW3D_PT_tools_grease_pencil_brush_settings,
    VIEW3D_PT_tools_grease_pencil_brush_advanced,
    VIEW3D_PT_tools_grease_pencil_brush_stroke,
    VIEW3D_PT_tools_grease_pencil_brush_post_processing,
    VIEW3D_PT_tools_grease_pencil_brush_random,
    VIEW3D_PT_tools_grease_pencil_brush_stabilizer,
    VIEW3D_PT_tools_grease_pencil_brushcurves,
    VIEW3D_PT_tools_grease_pencil_brushcurves_sensitivity,
    VIEW3D_PT_tools_grease_pencil_brushcurves_strength,
    VIEW3D_PT_tools_grease_pencil_brushcurves_jitter,
    VIEW3D_PT_tools_grease_pencil_paint_appearance,
    VIEW3D_PT_tools_grease_pencil_sculpt_select,
    VIEW3D_PT_tools_grease_pencil_sculpt_settings,
    VIEW3D_PT_tools_grease_pencil_sculpt_options,
    VIEW3D_PT_tools_grease_pencil_sculpt_appearance,
    VIEW3D_PT_tools_grease_pencil_weight_paint_select,
    VIEW3D_PT_tools_grease_pencil_weight_paint_settings,
    VIEW3D_PT_tools_grease_pencil_weight_appearance,
    VIEW3D_PT_tools_grease_pencil_interpolate,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
