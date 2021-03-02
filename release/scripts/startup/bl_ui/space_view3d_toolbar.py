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
    GreasePencilBrushFalloff,
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


class VIEW3D_MT_brush_gpencil_context_menu(Menu):
    bl_label = "Brush Specials"

    def draw(self, context):
        layout = self.layout
        ts = context.tool_settings

        settings = None
        if context.mode == 'PAINT_GPENCIL':
            settings = ts.gpencil_paint
        if context.mode == 'SCULPT_GPENCIL':
            settings = ts.gpencil_sculpt_paint
        elif context.mode == 'WEIGHT_GPENCIL':
            settings = ts.gpencil_weight_paint
        elif context.mode == 'VERTEX_GPENCIL':
            settings = ts.gpencil_vertex_paint

        brush = getattr(settings, "brush", None)
        # skip if no active brush
        if not brush:
            layout.label(text="No Brushes currently available", icon='INFO')
            return

        layout.operator("gpencil.brush_reset")
        layout.operator("gpencil.brush_reset_all")


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
def draw_vpaint_symmetry(layout, vpaint, mesh):
    col = layout.column()
    row = col.row(heading="Mirror", align=True)
    row.prop(mesh, "use_mirror_x", text="X", toggle=True)
    row.prop(mesh, "use_mirror_y", text="Y", toggle=True)
    row.prop(mesh, "use_mirror_z", text="Z", toggle=True)

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

        col = layout.column(heading="Affect Only", align=True)
        col.prop(tool_settings, "use_transform_data_origin", text="Origins")
        col.prop(tool_settings, "use_transform_pivot_point_align", text="Locations")
        col.prop(tool_settings, "use_transform_skip_children", text="Parents")


# ********** default tools for editmode_mesh ****************


class VIEW3D_PT_tools_meshedit_options(View3DPanel, Panel):
    bl_category = "Tool"
    bl_context = ".mesh_edit"  # dot on purpose (access from topbar)
    bl_label = "Options"
    bl_options = {'DEFAULT_CLOSED'}
    bl_ui_units_x = 12

    @classmethod
    def poll(cls, context):
        return context.active_object

    def draw(self, context):
        layout = self.layout

        layout.use_property_split = True
        layout.use_property_decorate = False

        tool_settings = context.tool_settings
        ob = context.active_object
        mesh = ob.data

        row = layout.row(align=True, heading="Transform")
        row.prop(tool_settings, "use_transform_correct_face_attributes")

        row = layout.row(align=True)
        row.active = tool_settings.use_transform_correct_face_attributes
        row.prop(tool_settings, "use_transform_correct_keep_connected")

        row = layout.row(align=True, heading="UVs")
        row.prop(tool_settings, "use_edge_path_live_unwrap")

        row = layout.row(heading="Mirror")
        sub = row.row(align=True)
        sub.prop(mesh, "use_mirror_x", text="X", toggle=True)
        sub.prop(mesh, "use_mirror_y", text="Y", toggle=True)
        sub.prop(mesh, "use_mirror_z", text="Z", toggle=True)

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

        from bl_ui.space_toolsystem_common import ToolSelectPanelHelper
        tool_context = ToolSelectPanelHelper.tool_active_from_context(context)

        if not tool_context:
            # If there is no active tool, then there can't be an active brush.
            tool = None

        if not tool_context.has_datablock:
            # tool.has_datablock is always true for tools that use brushes.
            tool = None

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
                    col = layout.column(align=False, heading="Deflect Emitter")
                    row = col.row(align=True)
                    sub = row.row(align=True)
                    sub.prop(settings, "use_emitter_deflect", text="")
                    sub = sub.row(align=True)
                    sub.active = settings.use_emitter_deflect
                    sub.prop(settings, "emitter_distance", text="")


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
    bl_ui_units_x = 14

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
    bl_ui_units_x = 12


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
        tex_slot = brush.texture_slot

        col = layout.column()
        col.template_ID_preview(tex_slot, "texture", new="texture.new", rows=3, cols=8)

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
        mask_tex_slot = brush.mask_texture_slot

        col.template_ID_preview(mask_tex_slot, "texture", new="texture.new", rows=3, cols=8)

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
    bl_label = "Front-Face Falloff"
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
        paint_settings = cls.paint_settings(context)
        return (context.sculpt_object and context.tool_settings.sculpt and paint_settings)

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

        col = layout.column(heading="Preserve", align=True)
        col.prop(mesh, "use_remesh_preserve_volume", text="Volume")
        col.prop(mesh, "use_remesh_preserve_paint_mask", text="Paint Mask")
        col.prop(mesh, "use_remesh_preserve_sculpt_face_sets", text="Face Sets")
        if context.preferences.experimental.use_sculpt_vertex_colors:
            col.prop(mesh, "use_remesh_preserve_vertex_colors", text="Vertex Colors")

        layout.operator("object.voxel_remesh", text="Remesh")


# TODO, move to space_view3d.py
class VIEW3D_PT_sculpt_options(Panel, View3DPaintPanel):
    bl_context = ".sculpt_mode"  # dot on purpose (access from topbar)
    bl_label = "Options"
    bl_options = {'DEFAULT_CLOSED'}
    bl_ui_units_x = 12

    @classmethod
    def poll(cls, context):
        return (context.sculpt_object and context.tool_settings.sculpt)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        tool_settings = context.tool_settings
        sculpt = tool_settings.sculpt

        col = layout.column(heading="Display", align=True)
        col.prop(sculpt, "show_low_resolution")
        col.prop(sculpt, "use_sculpt_delay_updates")
        col.prop(sculpt, "use_deform_only")

        col.separator()

        col = layout.column(heading="Auto-Masking", align=True)
        col.prop(sculpt, "use_automasking_topology", text="Topology")
        col.prop(sculpt, "use_automasking_face_sets", text="Face Sets")
        col.prop(sculpt, "use_automasking_boundary_edges", text="Mesh Boundary")
        col.prop(sculpt, "use_automasking_boundary_face_sets", text="Face Sets Boundary")


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
        layout.use_property_split = True
        layout.use_property_decorate = False

        sculpt = context.tool_settings.sculpt

        row = layout.row(align=True, heading="Mirror")
        mesh = context.object.data
        row.prop(mesh, "use_mirror_x", text="X", toggle=True)
        row.prop(mesh, "use_mirror_y", text="Y", toggle=True)
        row.prop(mesh, "use_mirror_z", text="Z", toggle=True)

        row = layout.row(align=True, heading="Lock")
        row.prop(sculpt, "lock_x", text="X", toggle=True)
        row.prop(sculpt, "lock_y", text="Y", toggle=True)
        row.prop(sculpt, "lock_z", text="Z", toggle=True)

        row = layout.row(align=True, heading="Tiling")
        row.prop(sculpt, "tile_x", text="X", toggle=True)
        row.prop(sculpt, "tile_y", text="Y", toggle=True)
        row.prop(sculpt, "tile_z", text="Z", toggle=True)

        layout.prop(sculpt, "use_symmetry_feather", text="Feather")
        layout.prop(sculpt, "radial_symmetry", text="Radial")
        layout.prop(sculpt, "tile_offset", text="Tile Offset")

        layout.separator()

        layout.prop(sculpt, "symmetrize_direction")
        layout.operator("sculpt.symmetrize")


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
        layout.use_property_split = True
        layout.use_property_decorate = False

        tool_settings = context.tool_settings
        wpaint = tool_settings.weight_paint
        mesh = context.object.data

        draw_vpaint_symmetry(layout, wpaint, mesh)

        col = layout.column(align=True)
        col.prop(mesh, 'use_mirror_vertex_group_x', text="Vertex Group X")
        row = col.row()
        row.active = mesh.use_mirror_vertex_group_x
        row.prop(mesh, "use_mirror_topology")


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
        col.prop(tool_settings, "use_lock_relative", text="Lock-Relative")
        col.prop(tool_settings, "use_multipaint", text="Multi-Paint")

        col.prop(wpaint, "use_group_restrict")


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
        layout.use_property_split = True
        layout.use_property_decorate = False

        tool_settings = context.tool_settings
        vpaint = tool_settings.vertex_paint

        draw_vpaint_symmetry(layout, vpaint, context.object.data)


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

        split = layout.split()

        col = split.column()
        col.alignment = 'RIGHT'
        col.label(text="Mirror")

        col = split.column()

        row = col.row(align=True)
        mesh = context.object.data
        row.prop(mesh, "use_mirror_x", text="X", toggle=True)
        row.prop(mesh, "use_mirror_y", text="Y", toggle=True)
        row.prop(mesh, "use_mirror_z", text="Z", toggle=True)


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

        col = layout.column()
        col.prop(ipaint, "use_occlude")
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

        if not pe.is_hair:
            col.prop(pe, "use_auto_velocity", text="Auto-Velocity")
            col.separator()

        sub = col.column(align=True, heading="Mirror")
        sub.prop(ob.data, "use_mirror_x")
        if pe.tool == 'ADD':
            sub.prop(ob.data, "use_mirror_topology")
        col.separator()

        sub = col.column(align=True, heading="Preserve")
        sub.prop(pe, "use_preserve_length", text="Strand Lengths")
        sub.prop(pe, "use_preserve_root", text="Root Positions")


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
            col = layout.column(align=False, heading="Fade Time")
            row = col.row(align=True)
            sub = row.row(align=True)
            sub.prop(pe, "use_fade_time", text="")
            sub = sub.row(align=True)
            sub.active = pe.use_fade_time
            sub.prop(pe, "fade_frames", slider=True, text="")


# ********** grease pencil object tool panels ****************

# Grease Pencil drawing brushes


class GreasePencilPaintPanel:
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


class VIEW3D_PT_tools_grease_pencil_brush_select(Panel, View3DPanel, GreasePencilPaintPanel):
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
        col.menu("VIEW3D_MT_brush_gpencil_context_menu", icon='DOWNARROW_HLT', text="")

        if context.mode == 'PAINT_GPENCIL':
            brush = tool_settings.gpencil_paint.brush
            if brush is not None:
                col.prop(brush, "use_custom_icon", toggle=True, icon='FILE_IMAGE', text="")

                if brush.use_custom_icon:
                    layout.row().prop(brush, "icon_filepath", text="")


class VIEW3D_PT_tools_grease_pencil_brush_settings(Panel, View3DPanel, GreasePencilPaintPanel):
    bl_label = "Brush Settings"
    bl_options = {'DEFAULT_CLOSED'}

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
                    row_mat.enabled = False  # will otherwise allow changing material in active slot

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
    bl_ui_units_x = 13

    @classmethod
    def poll(cls, context):
        brush = context.tool_settings.gpencil_paint.brush
        return brush is not None and brush.gpencil_tool not in {'ERASE', 'TINT'}

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
                col.prop(gp_settings, "hardness", slider=True)
                subcol = col.column(align=True)
                if ma and ma.grease_pencil.mode == 'LINE':
                    subcol.enabled = False
                subcol.prop(gp_settings, "aspect")

            elif brush.gpencil_tool == 'FILL':
                row = col.row(align=True)
                row.prop(gp_settings, "fill_draw_mode", text="Boundary")
                row.prop(gp_settings, "show_fill_boundary", text="", icon='GRID')

                col.separator()
                row = col.row(align=True)
                row.prop(gp_settings, "fill_layer_mode", text="Layers")

                col.separator()
                row = col.row(align=True)
                row.prop(gp_settings, "extend_stroke_factor")
                row.prop(gp_settings, "show_fill_extend", text="", icon='GRID')

                col.separator()
                col.prop(gp_settings, "fill_simplify_level", text="Simplify")
                if gp_settings.fill_draw_mode != 'STROKE':
                    col = layout.column(align=False, heading="Ignore Transparent")
                    col.use_property_decorate = False
                    row = col.row(align=True)
                    sub = row.row(align=True)
                    sub.prop(gp_settings, "show_fill", text="")
                    sub = sub.row(align=True)
                    sub.active = gp_settings.show_fill
                    sub.prop(gp_settings, "fill_threshold", text="")

                col.separator()
                row = col.row(align=True)
                row.prop(gp_settings, "use_fill_limit")


class VIEW3D_PT_tools_grease_pencil_brush_stroke(Panel, View3DPanel):
    bl_context = ".greasepencil_paint"
    bl_parent_id = 'VIEW3D_PT_tools_grease_pencil_brush_settings'
    bl_label = "Stroke"
    bl_category = "Tool"
    bl_options = {'DEFAULT_CLOSED'}
    bl_ui_units_x = 12

    @classmethod
    def poll(cls, context):
        brush = context.tool_settings.gpencil_paint.brush
        return brush is not None and brush.gpencil_tool == 'DRAW'

    def draw(self, _context):
        # layout = self.layout
        pass


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
        return brush is not None and brush.gpencil_tool not in {'ERASE', 'FILL', 'TINT'}

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
        col1.prop(gp_settings, "pen_subdivision_steps")

        col1 = col.column(align=True)
        col1.prop(gp_settings, "simplify_factor")

        col1 = col.column(align=True)
        col1.prop(gp_settings, "use_trim")


class VIEW3D_PT_tools_grease_pencil_brush_random(View3DPanel, Panel):
    bl_context = ".greasepencil_paint"
    bl_parent_id = 'VIEW3D_PT_tools_grease_pencil_brush_stroke'
    bl_label = "Randomize"
    bl_category = "Tool"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        brush = context.tool_settings.gpencil_paint.brush
        return brush is not None and brush.gpencil_tool not in {'ERASE', 'FILL', 'TINT'}

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

        tool_settings = context.tool_settings
        brush = tool_settings.gpencil_paint.brush
        mode = tool_settings.gpencil_paint.color_mode
        gp_settings = brush.gpencil_settings

        if self.is_popover:
            row = layout.row()
            row.prop(gp_settings, "use_settings_random", text="")
            row.label(text=self.bl_label)

        col = layout.column()
        col.enabled = gp_settings.use_settings_random

        row = col.row(align=True)
        row.prop(gp_settings, "random_pressure", text="Radius", slider=True)
        row.prop(gp_settings, "use_stroke_random_radius", text="", icon='GP_SELECT_STROKES')
        row.prop(gp_settings, "use_random_press_radius", text="", icon='STYLUS_PRESSURE')
        if gp_settings.use_random_press_radius and self.is_popover is False:
            col.template_curve_mapping(gp_settings, "curve_random_pressure", brush=True,
                                       use_negative_slope=True)

        row = col.row(align=True)
        row.prop(gp_settings, "random_strength", text="Strength", slider=True)
        row.prop(gp_settings, "use_stroke_random_strength", text="", icon='GP_SELECT_STROKES')
        row.prop(gp_settings, "use_random_press_strength", text="", icon='STYLUS_PRESSURE')
        if gp_settings.use_random_press_strength and self.is_popover is False:
            col.template_curve_mapping(gp_settings, "curve_random_strength", brush=True,
                                       use_negative_slope=True)

        row = col.row(align=True)
        row.prop(gp_settings, "uv_random", text="UV", slider=True)
        row.prop(gp_settings, "use_stroke_random_uv", text="", icon='GP_SELECT_STROKES')
        row.prop(gp_settings, "use_random_press_uv", text="", icon='STYLUS_PRESSURE')
        if gp_settings.use_random_press_uv and self.is_popover is False:
            col.template_curve_mapping(gp_settings, "curve_random_uv", brush=True,
                                       use_negative_slope=True)

        col.separator()

        col1 = col.column(align=True)
        col1.enabled = mode == 'VERTEXCOLOR' and gp_settings.use_settings_random
        row = col1.row(align=True)
        row.prop(gp_settings, "random_hue_factor", slider=True)
        row.prop(gp_settings, "use_stroke_random_hue", text="", icon='GP_SELECT_STROKES')
        row.prop(gp_settings, "use_random_press_hue", text="", icon='STYLUS_PRESSURE')
        if gp_settings.use_random_press_hue and self.is_popover is False:
            col1.template_curve_mapping(gp_settings, "curve_random_hue", brush=True,
                                        use_negative_slope=True)

        row = col1.row(align=True)
        row.prop(gp_settings, "random_saturation_factor", slider=True)
        row.prop(gp_settings, "use_stroke_random_sat", text="", icon='GP_SELECT_STROKES')
        row.prop(gp_settings, "use_random_press_sat", text="", icon='STYLUS_PRESSURE')
        if gp_settings.use_random_press_sat and self.is_popover is False:
            col1.template_curve_mapping(gp_settings, "curve_random_saturation", brush=True,
                                        use_negative_slope=True)

        row = col1.row(align=True)
        row.prop(gp_settings, "random_value_factor", slider=True)
        row.prop(gp_settings, "use_stroke_random_val", text="", icon='GP_SELECT_STROKES')
        row.prop(gp_settings, "use_random_press_val", text="", icon='STYLUS_PRESSURE')
        if gp_settings.use_random_press_val and self.is_popover is False:
            col1.template_curve_mapping(gp_settings, "curve_random_value", brush=True,
                                        use_negative_slope=True)

        col.separator()

        row = col.row(align=True)
        row.prop(gp_settings, "pen_jitter", slider=True)
        row.prop(gp_settings, "use_jitter_pressure", text="", icon='STYLUS_PRESSURE')
        if gp_settings.use_jitter_pressure and self.is_popover is False:
            col.template_curve_mapping(gp_settings, "curve_jitter", brush=True,
                                       use_negative_slope=True)


class VIEW3D_PT_tools_grease_pencil_brush_paint_falloff(GreasePencilBrushFalloff, Panel, View3DPaintPanel):
    bl_context = ".greasepencil_paint"
    bl_label = "Falloff"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        ts = context.tool_settings
        settings = ts.gpencil_paint
        brush = settings.brush
        if brush is None:
            return False

        tool = brush.gpencil_tool

        return (settings and settings.brush and settings.brush.curve and tool == 'TINT')


# Grease Pencil stroke sculpting tools
class GreasePencilSculptPanel:
    bl_context = ".greasepencil_sculpt"
    bl_category = "Tool"

    @classmethod
    def poll(cls, context):
        if context.space_data.type in {'VIEW_3D', 'PROPERTIES'}:
            if context.gpencil_data is None:
                return False

            gpd = context.gpencil_data
            return bool(gpd.is_stroke_sculpt_mode)
        else:
            return True


class VIEW3D_PT_tools_grease_pencil_sculpt_select(Panel, View3DPanel, GreasePencilSculptPanel):
    bl_label = "Brushes"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        tool_settings = context.scene.tool_settings
        gpencil_paint = tool_settings.gpencil_sculpt_paint

        row = layout.row()
        row.column().template_ID_preview(gpencil_paint, "brush", new="brush.add_gpencil", rows=3, cols=8)

        col = row.column()
        col.menu("VIEW3D_MT_brush_gpencil_context_menu", icon='DOWNARROW_HLT', text="")

        if context.mode == 'SCULPT_GPENCIL':
            brush = tool_settings.gpencil_sculpt_paint.brush
            if brush is not None:
                col.prop(brush, "use_custom_icon", toggle=True, icon='FILE_IMAGE', text="")

                if(brush.use_custom_icon):
                    layout.row().prop(brush, "icon_filepath", text="")


class VIEW3D_PT_tools_grease_pencil_sculpt_settings(Panel, View3DPanel, GreasePencilSculptPanel):
    bl_label = "Brush Settings"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        tool_settings = context.scene.tool_settings
        settings = tool_settings.gpencil_sculpt_paint
        brush = settings.brush

        if not self.is_popover:
            from bl_ui.properties_paint_common import (
                brush_basic_gpencil_sculpt_settings,
            )
            brush_basic_gpencil_sculpt_settings(layout, context, brush)


class VIEW3D_PT_tools_grease_pencil_brush_sculpt_falloff(GreasePencilBrushFalloff, Panel, View3DPaintPanel):
    bl_context = ".greasepencil_sculpt"
    bl_label = "Falloff"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        ts = context.tool_settings
        settings = ts.gpencil_sculpt_paint
        return (settings and settings.brush and settings.brush.curve)


# Grease Pencil weight painting tools
class GreasePencilWeightPanel:
    bl_context = ".greasepencil_weight"
    bl_category = "Tool"

    @classmethod
    def poll(cls, context):
        if context.space_data.type in {'VIEW_3D', 'PROPERTIES'}:
            if context.gpencil_data is None:
                return False

            gpd = context.gpencil_data
            return bool(gpd.is_stroke_weight_mode)
        else:
            return True


class VIEW3D_PT_tools_grease_pencil_weight_paint_select(View3DPanel, Panel, GreasePencilWeightPanel):
    bl_label = "Brushes"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        tool_settings = context.scene.tool_settings
        gpencil_paint = tool_settings.gpencil_weight_paint

        row = layout.row()
        row.column().template_ID_preview(gpencil_paint, "brush", new="brush.add_gpencil", rows=3, cols=8)

        col = row.column()
        col.menu("VIEW3D_MT_brush_gpencil_context_menu", icon='DOWNARROW_HLT', text="")

        if context.mode == 'WEIGHT_GPENCIL':
            brush = tool_settings.gpencil_weight_paint.brush
            if brush is not None:
                col.prop(brush, "use_custom_icon", toggle=True, icon='FILE_IMAGE', text="")

                if(brush.use_custom_icon):
                    layout.row().prop(brush, "icon_filepath", text="")


class VIEW3D_PT_tools_grease_pencil_weight_paint_settings(Panel, View3DPanel, GreasePencilWeightPanel):
    bl_label = "Brush Settings"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        tool_settings = context.scene.tool_settings
        settings = tool_settings.gpencil_weight_paint
        brush = settings.brush

        if not self.is_popover:
            from bl_ui.properties_paint_common import (
                brush_basic_gpencil_weight_settings,
            )
            brush_basic_gpencil_weight_settings(layout, context, brush)


class VIEW3D_PT_tools_grease_pencil_brush_weight_falloff(GreasePencilBrushFalloff, Panel, View3DPaintPanel):
    bl_context = ".greasepencil_weight"
    bl_label = "Falloff"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        ts = context.tool_settings
        settings = ts.gpencil_weight_paint
        brush = settings.brush
        return (settings and settings.brush and settings.brush.curve)


# Grease Pencil vertex painting tools
class GreasePencilVertexPanel:
    bl_context = ".greasepencil_vertex"
    bl_category = "Tool"

    @classmethod
    def poll(cls, context):
        if context.space_data.type in {'VIEW_3D', 'PROPERTIES'}:
            if context.gpencil_data is None:
                return False

            gpd = context.gpencil_data
            return bool(gpd.is_stroke_vertex_mode)
        else:
            return True


class VIEW3D_PT_tools_grease_pencil_vertex_paint_select(View3DPanel, Panel, GreasePencilVertexPanel):
    bl_label = "Brushes"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        tool_settings = context.scene.tool_settings
        gpencil_paint = tool_settings.gpencil_vertex_paint

        row = layout.row()
        row.column().template_ID_preview(gpencil_paint, "brush", new="brush.add_gpencil", rows=3, cols=8)

        col = row.column()
        col.menu("VIEW3D_MT_brush_gpencil_context_menu", icon='DOWNARROW_HLT', text="")

        if context.mode == 'VERTEX_GPENCIL':
            brush = tool_settings.gpencil_vertex_paint.brush
            if brush is not None:
                col.prop(brush, "use_custom_icon", toggle=True, icon='FILE_IMAGE', text="")

                if(brush.use_custom_icon):
                    layout.row().prop(brush, "icon_filepath", text="")


class VIEW3D_PT_tools_grease_pencil_vertex_paint_settings(Panel, View3DPanel, GreasePencilVertexPanel):
    bl_label = "Brush Settings"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        tool_settings = context.scene.tool_settings
        settings = tool_settings.gpencil_vertex_paint
        brush = settings.brush

        if not self.is_popover:
            from bl_ui.properties_paint_common import (
                brush_basic_gpencil_vertex_settings,
            )
            brush_basic_gpencil_vertex_settings(layout, context, brush)


class VIEW3D_PT_tools_grease_pencil_brush_vertex_color(View3DPanel, Panel):
    bl_context = ".greasepencil_vertex"
    bl_label = "Color"
    bl_category = "Tool"

    @classmethod
    def poll(cls, context):
        ob = context.object
        ts = context.tool_settings
        settings = ts.gpencil_vertex_paint
        brush = settings.brush

        if ob is None or brush is None:
            return False

        if context.region.type == 'TOOL_HEADER' or brush.gpencil_vertex_tool in {'BLUR', 'AVERAGE', 'SMEAR'}:
            return False

        return True

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False
        ts = context.tool_settings
        settings = ts.gpencil_vertex_paint
        brush = settings.brush
        gp_settings = brush.gpencil_settings

        col = layout.column()

        col.template_color_picker(brush, "color", value_slider=True)

        sub_row = col.row(align=True)
        sub_row.prop(brush, "color", text="")
        sub_row.prop(brush, "secondary_color", text="")

        sub_row.operator("gpencil.tint_flip", icon='FILE_REFRESH', text="")


class VIEW3D_PT_tools_grease_pencil_brush_vertex_falloff(GreasePencilBrushFalloff, Panel, View3DPaintPanel):
    bl_context = ".greasepencil_vertex"
    bl_label = "Falloff"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        ts = context.tool_settings
        settings = ts.gpencil_vertex_paint
        return (settings and settings.brush and settings.brush.curve)


class VIEW3D_PT_tools_grease_pencil_brush_vertex_palette(View3DPanel, Panel):
    bl_context = ".greasepencil_vertex"
    bl_label = "Palette"
    bl_category = "Tool"
    bl_parent_id = 'VIEW3D_PT_tools_grease_pencil_brush_vertex_color'

    @classmethod
    def poll(cls, context):
        ob = context.object
        ts = context.tool_settings
        settings = ts.gpencil_vertex_paint
        brush = settings.brush

        if ob is None or brush is None:
            return False

        if brush.gpencil_vertex_tool in {'BLUR', 'AVERAGE', 'SMEAR'}:
            return False

        return True

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False
        ts = context.tool_settings
        settings = ts.gpencil_vertex_paint

        col = layout.column()

        row = col.row(align=True)
        row.template_ID(settings, "palette", new="palette.new")
        if settings.palette:
            col.template_palette(settings, "palette", color=True)


class VIEW3D_PT_tools_grease_pencil_brush_mixcolor(View3DPanel, Panel):
    bl_context = ".greasepencil_paint"
    bl_label = "Color"
    bl_category = "Tool"

    @classmethod
    def poll(cls, context):
        ob = context.object
        ts = context.tool_settings
        settings = ts.gpencil_paint
        brush = settings.brush

        if ob is None or brush is None:
            return False

        if context.region.type == 'TOOL_HEADER':
            return False

        if brush.gpencil_tool == 'TINT':
            return True

        if brush.gpencil_tool not in {'DRAW', 'FILL'}:
            return False

        return True

    def draw(self, context):
        layout = self.layout
        ts = context.tool_settings
        settings = ts.gpencil_paint
        brush = settings.brush
        gp_settings = brush.gpencil_settings

        if brush.gpencil_tool != 'TINT':
            row = layout.row()
            row.prop(settings, "color_mode", expand=True)

        layout.use_property_split = True
        layout.use_property_decorate = False
        col = layout.column()
        col.enabled = settings.color_mode == 'VERTEXCOLOR' or brush.gpencil_tool == 'TINT'

        col.template_color_picker(brush, "color", value_slider=True)

        sub_row = col.row(align=True)
        sub_row.prop(brush, "color", text="")
        sub_row.prop(brush, "secondary_color", text="")

        sub_row.operator("gpencil.tint_flip", icon='FILE_REFRESH', text="")

        if brush.gpencil_tool in {'DRAW', 'FILL'}:
            col.prop(gp_settings, "vertex_mode", text="Mode")
            col.prop(gp_settings, "vertex_color_factor", slider=True, text="Mix Factor")

        if brush.gpencil_tool == 'TINT':
            col.prop(gp_settings, "vertex_mode", text="Mode")


class VIEW3D_PT_tools_grease_pencil_brush_mix_palette(View3DPanel, Panel):
    bl_context = ".greasepencil_paint"
    bl_label = "Palette"
    bl_category = "Tool"
    bl_parent_id = 'VIEW3D_PT_tools_grease_pencil_brush_mixcolor'

    @classmethod
    def poll(cls, context):
        ob = context.object
        ts = context.tool_settings
        settings = ts.gpencil_paint
        brush = settings.brush

        if ob is None or brush is None:
            return False

        if brush.gpencil_tool == 'TINT':
            return True

        if brush.gpencil_tool not in {'DRAW', 'FILL'}:
            return False

        return True

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False
        ts = context.tool_settings
        settings = ts.gpencil_paint
        brush = settings.brush

        col = layout.column()
        col.enabled = settings.color_mode == 'VERTEXCOLOR' or brush.gpencil_tool == 'TINT'

        row = col.row(align=True)
        row.template_ID(settings, "palette", new="palette.new")
        if settings.palette:
            col.template_palette(settings, "palette", color=True)


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
    bl_ui_units_x = 15


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


class VIEW3D_PT_tools_grease_pencil_vertex_appearance(GreasePencilDisplayPanel, Panel, View3DPanel):
    bl_context = ".greasepencil_vertex"
    bl_parent_id = 'VIEW3D_PT_tools_grease_pencil_vertex_paint_settings'
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
    VIEW3D_MT_brush_gpencil_context_menu,
    VIEW3D_MT_brush_context_menu_paint_modes,
    VIEW3D_PT_tools_object_options,
    VIEW3D_PT_tools_object_options_transform,
    VIEW3D_PT_tools_meshedit_options,
    VIEW3D_PT_tools_meshedit_options_automerge,
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
    VIEW3D_PT_tools_grease_pencil_paint_appearance,
    VIEW3D_PT_tools_grease_pencil_sculpt_select,
    VIEW3D_PT_tools_grease_pencil_sculpt_settings,
    VIEW3D_PT_tools_grease_pencil_sculpt_options,
    VIEW3D_PT_tools_grease_pencil_sculpt_appearance,
    VIEW3D_PT_tools_grease_pencil_weight_paint_select,
    VIEW3D_PT_tools_grease_pencil_weight_paint_settings,
    VIEW3D_PT_tools_grease_pencil_weight_appearance,
    VIEW3D_PT_tools_grease_pencil_vertex_paint_select,
    VIEW3D_PT_tools_grease_pencil_vertex_paint_settings,
    VIEW3D_PT_tools_grease_pencil_vertex_appearance,
    VIEW3D_PT_tools_grease_pencil_brush_mixcolor,
    VIEW3D_PT_tools_grease_pencil_brush_mix_palette,

    VIEW3D_PT_tools_grease_pencil_brush_paint_falloff,
    VIEW3D_PT_tools_grease_pencil_brush_sculpt_falloff,
    VIEW3D_PT_tools_grease_pencil_brush_weight_falloff,
    VIEW3D_PT_tools_grease_pencil_brush_vertex_color,
    VIEW3D_PT_tools_grease_pencil_brush_vertex_palette,
    VIEW3D_PT_tools_grease_pencil_brush_vertex_falloff,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
