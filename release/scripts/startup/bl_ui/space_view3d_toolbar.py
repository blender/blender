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
from bpy.types import Menu, Panel, UIList
from .properties_grease_pencil_common import (
    GreasePencilStrokeEditPanel,
    GreasePencilStrokeSculptPanel,
    GreasePencilAppearancePanel,
)
from .properties_paint_common import (
    UnifiedPaintPanel,
    brush_texture_settings,
    brush_texpaint_common,
    brush_mask_texture_settings,
)
from bl_operators.presets import PresetMenu


class View3DPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'


# **************** standard tool clusters ******************

# Keyframing tools
def draw_keyframing_tools(context, layout):
    col = layout.column(align=True)
    col.label(text="Keyframes:")
    row = col.row(align=True)
    row.operator("anim.keyframe_insert_menu", text="Insert")
    row.operator("anim.keyframe_delete_v3d", text="Remove")


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
    col.prop(vpaint, "radial_symmetry", text="Radial")

# Most of these panels should not be visible in GP edit modes


def is_not_gpencil_edit_mode(context):
    is_gpmode = (
        context.active_object and
        context.active_object.mode in {'GPENCIL_EDIT', 'GPENCIL_PAINT', 'GPENCIL_SCULPT', 'GPENCIL_WEIGHT'}
    )
    return not is_gpmode


# ********** default tools for editmode_mesh ****************


class VIEW3D_PT_tools_meshedit_options(View3DPanel, Panel):
    bl_category = "Options"
    bl_context = ".mesh_edit"  # dot on purpose (access from topbar)
    bl_label = "Mesh Options"

    @classmethod
    def poll(cls, context):
        return context.active_object

    def draw(self, context):
        layout = self.layout

        ob = context.active_object

        tool_settings = context.tool_settings
        mesh = ob.data

        col = layout.column(align=True)
        col.prop(mesh, "use_mirror_x")

        row = col.row(align=True)
        row.active = ob.data.use_mirror_x
        row.prop(mesh, "use_mirror_topology")

        col = layout.column(align=True)
        col.label("Edge Select Mode:")
        col.prop(tool_settings, "edge_path_mode", text="")
        col.prop(tool_settings, "edge_path_live_unwrap")
        col.label("Double Threshold:")
        col.prop(tool_settings, "double_threshold", text="")
        col.prop(tool_settings, "use_mesh_automerge")  # , icon='AUTOMERGE_ON'


# ********** default tools for editmode_curve ****************


class VIEW3D_PT_tools_curveedit_options_stroke(View3DPanel, Panel):
    bl_category = "Options"
    bl_context = ".curve_edit"  # dot on purpose (access from topbar)
    bl_label = "Curve Stroke"

    def draw(self, context):
        layout = self.layout

        tool_settings = context.tool_settings
        cps = tool_settings.curve_paint_settings

        col = layout.column()

        col.prop(cps, "curve_type")

        if cps.curve_type == 'BEZIER':
            col.label("Bezier Options:")
            col.prop(cps, "error_threshold")
            col.prop(cps, "fit_method")
            col.prop(cps, "use_corners_detect")

            col = layout.column()
            col.active = cps.use_corners_detect
            col.prop(cps, "corner_angle")

        col.label("Pressure Radius:")
        row = layout.row(align=True)
        rowsub = row.row(align=True)
        rowsub.prop(cps, "radius_min", text="Min")
        rowsub.prop(cps, "radius_max", text="Max")

        row.prop(cps, "use_pressure_radius", text="", icon_only=True)

        col = layout.column()
        col.label("Taper Radius:")
        row = layout.row(align=True)
        row.prop(cps, "radius_taper_start", text="Start")
        row.prop(cps, "radius_taper_end", text="End")

        col = layout.column()
        col.label("Projection Depth:")
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
    bl_category = "Options"
    bl_context = ".armature_edit"  # dot on purpose (access from topbar)
    bl_label = "Armature Options"

    def draw(self, context):
        arm = context.active_object.data

        self.layout.prop(arm, "use_mirror_x")


# ********** default tools for pose-mode ****************

class VIEW3D_PT_tools_posemode_options(View3DPanel, Panel):
    bl_category = "Options"
    bl_context = ".posemode"  # dot on purpose (access from topbar)
    bl_label = "Pose Options"

    def draw(self, context):
        arm = context.active_object.data

        self.layout.prop(arm, "use_auto_ik")
        self.layout.prop(arm, "use_mirror_x")

# ********** default tools for paint modes ****************


class View3DPaintPanel(UnifiedPaintPanel):
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'


class VIEW3D_PT_imapaint_tools_missing(Panel, View3DPaintPanel):
    bl_category = "Tools"
    bl_context = ".imagepaint"  # dot on purpose (access from topbar)
    bl_label = "Missing Data"

    @classmethod
    def poll(cls, context):
        toolsettings = context.tool_settings.image_paint
        return context.image_paint_object and not toolsettings.detect_data()

    def draw(self, context):
        layout = self.layout
        toolsettings = context.tool_settings.image_paint

        col = layout.column()
        col.label("Missing Data", icon='ERROR')
        if toolsettings.missing_uvs:
            col.separator()
            col.label("Missing UVs", icon='INFO')
            col.label("Unwrap the mesh in edit mode or generate a simple UV layer")
            col.operator("paint.add_simple_uvs")

        if toolsettings.mode == 'MATERIAL':
            if toolsettings.missing_materials:
                col.separator()
                col.label("Missing Materials", icon='INFO')
                col.label("Add a material and paint slot below")
                col.operator_menu_enum("paint.add_texture_paint_slot", "type", text="Add Paint Slot")
            elif toolsettings.missing_texture:
                ob = context.active_object
                mat = ob.active_material

                col.separator()
                if mat:
                    col.label("Missing Texture Slots", icon='INFO')
                    col.label("Add a paint slot below")
                    col.operator_menu_enum("paint.add_texture_paint_slot", "type", text="Add Paint Slot")
                else:
                    col.label("Missing Materials", icon='INFO')
                    col.label("Add a material and paint slot below")
                    col.operator_menu_enum("paint.add_texture_paint_slot", "type", text="Add Paint Slot")

        elif toolsettings.mode == 'IMAGE':
            if toolsettings.missing_texture:
                col.separator()
                col.label("Missing Canvas", icon='INFO')
                col.label("Add or assign a canvas image below")
                col.label("Canvas Image:")
                col.template_ID(toolsettings, "canvas", new="image.new", open="image.open")

        if toolsettings.missing_stencil:
            col.separator()
            col.label("Missing Stencil", icon='INFO')
            col.label("Add or assign a stencil image below")
            col.label("Stencil Image:")
            col.template_ID(toolsettings, "stencil_image", new="image.new", open="image.open")


# TODO, move to space_view3d.py
class VIEW3D_PT_tools_brush(Panel, View3DPaintPanel):
    bl_context = ".paint_common"  # dot on purpose (access from topbar)
    bl_label = "Brush"

    @classmethod
    def poll(cls, context):
        return cls.paint_settings(context)

    def draw(self, context):
        layout = self.layout

        toolsettings = context.tool_settings
        settings = self.paint_settings(context)
        brush = settings.brush

        if not context.particle_edit_object:
            col = layout.split().column()
            col.template_ID_preview(settings, "brush", new="brush.add", rows=3, cols=8)

        # Particle Mode #
        if context.particle_edit_object:
            tool = settings.tool


            if self.is_popover:
                # Topbar shows these already.
                pass
            else:
                if tool != 'NONE':
                    layout.column().prop(settings, "tool", expand=True)
                    col = layout.column()
                    col.prop(brush, "size", slider=True)
                    if tool == 'ADD':
                        col.prop(brush, "count")
                    else:
                        col.prop(brush, "strength", slider=True)

            if tool == 'ADD':
                col = layout.column()
                col.prop(settings, "use_default_interpolate")
                col.prop(brush, "steps", slider=True)
                col.prop(settings, "default_key_count", slider=True)
            elif tool == 'LENGTH':
                layout.row().prop(brush, "length_mode", expand=True)
            elif tool == 'PUFF':
                layout.row().prop(brush, "puff_mode", expand=True)
                layout.prop(brush, "use_puff_volume")

        # Sculpt Mode #

        elif context.sculpt_object and brush:
            capabilities = brush.sculpt_capabilities

            col = layout.column()

            col.separator()

            row = col.row(align=True)

            ups = toolsettings.unified_paint_settings
            if ((ups.use_unified_size and ups.use_locked_size) or
                    ((not ups.use_unified_size) and brush.use_locked_size)):
                self.prop_unified_size(row, context, brush, "use_locked_size", icon='LOCKED')
                self.prop_unified_size(row, context, brush, "unprojected_radius", slider=True, text="Radius")
            else:
                self.prop_unified_size(row, context, brush, "use_locked_size", icon='UNLOCKED')
                self.prop_unified_size(row, context, brush, "size", slider=True, text="Radius")

            self.prop_unified_size(row, context, brush, "use_pressure_size")

            # strength, use_strength_pressure, and use_strength_attenuation
            col.separator()
            row = col.row(align=True)

            if capabilities.has_space_attenuation:
                row.prop(brush, "use_space_attenuation", toggle=True, icon_only=True)

            self.prop_unified_strength(row, context, brush, "strength", text="Strength")

            if capabilities.has_strength_pressure:
                self.prop_unified_strength(row, context, brush, "use_pressure_strength")

            # auto_smooth_factor and use_inverse_smooth_pressure
            if capabilities.has_auto_smooth:
                col.separator()

                row = col.row(align=True)
                row.prop(brush, "auto_smooth_factor", slider=True)
                row.prop(brush, "use_inverse_smooth_pressure", toggle=True, text="")

            # normal_weight
            if capabilities.has_normal_weight:
                col.separator()
                row = col.row(align=True)
                row.prop(brush, "normal_weight", slider=True)

            # crease_pinch_factor
            if capabilities.has_pinch_factor:
                col.separator()
                row = col.row(align=True)
                row.prop(brush, "crease_pinch_factor", slider=True, text="Pinch")

            # rake_factor
            if capabilities.has_rake_factor:
                col.separator()
                row = col.row(align=True)
                row.prop(brush, "rake_factor", slider=True)

            # use_original_normal and sculpt_plane
            if capabilities.has_sculpt_plane:
                col.separator()
                row = col.row(align=True)

                row.prop(brush, "use_original_normal", toggle=True, icon_only=True)

                row.prop(brush, "sculpt_plane", text="")

            if brush.sculpt_tool == 'MASK':
                col.prop(brush, "mask_tool", text="")

            # plane_offset, use_offset_pressure, use_plane_trim, plane_trim
            if capabilities.has_plane_offset:
                row = col.row(align=True)
                row.prop(brush, "plane_offset", slider=True)
                row.prop(brush, "use_offset_pressure", text="")

                col.separator()

                row = col.row()
                row.prop(brush, "use_plane_trim", text="Trim")
                row = col.row()
                row.active = brush.use_plane_trim
                row.prop(brush, "plane_trim", slider=True, text="Distance")

            # height
            if capabilities.has_height:
                row = col.row()
                row.prop(brush, "height", slider=True, text="Height")

            # use_frontface
            col.separator()
            col.prop(brush, "use_frontface", text="Front Faces Only")
            col.prop(brush, "use_projected")

            # direction
            col.separator()
            col.row().prop(brush, "direction", expand=True)

            # use_accumulate
            if capabilities.has_accumulate:
                col.separator()

                col.prop(brush, "use_accumulate")

            # use_persistent, set_persistent_base
            if capabilities.has_persistence:
                col.separator()

                ob = context.sculpt_object
                do_persistent = True

                # not supported yet for this case
                for md in ob.modifiers:
                    if md.type == 'MULTIRES':
                        do_persistent = False
                        break

                if do_persistent:
                    col.prop(brush, "use_persistent")
                    col.operator("sculpt.set_persistent_base")

        # Texture Paint Mode #

        elif context.image_paint_object and brush:
            brush_texpaint_common(self, context, layout, brush, settings, True)

        # Weight Paint Mode #
        elif context.weight_paint_object and brush:

            col = layout.column()

            row = col.row(align=True)
            self.prop_unified_weight(row, context, brush, "weight", slider=True, text="Weight")

            row = col.row(align=True)
            self.prop_unified_size(row, context, brush, "size", slider=True, text="Radius")
            self.prop_unified_size(row, context, brush, "use_pressure_size")

            row = col.row(align=True)
            self.prop_unified_strength(row, context, brush, "strength", text="Strength")
            self.prop_unified_strength(row, context, brush, "use_pressure_strength")

            col.separator()
            col.prop(brush, "vertex_tool", text="Blend")

            if brush.vertex_tool != 'SMEAR':
                col.prop(brush, "use_accumulate")
                col.separator()

            col.prop(brush, "use_frontface", text="Front Faces Only")
            row = col.row()
            row.prop(brush, "use_frontface_falloff", text="Falloff Angle")
            sub = row.row()
            sub.active = brush.use_frontface_falloff
            sub.prop(brush, "falloff_angle", text="")

            col.prop(brush, "use_projected")

            col = layout.column()
            col.prop(toolsettings, "use_auto_normalize", text="Auto Normalize")
            col.prop(toolsettings, "use_multipaint", text="Multi-Paint")

        # Vertex Paint Mode #
        elif context.vertex_paint_object and brush:
            col = layout.column()
            self.prop_unified_color_picker(col, context, brush, "color", value_slider=True)
            if settings.palette:
                col.template_palette(settings, "palette", color=True)
            row = col.row(align=True)
            self.prop_unified_color(row, context, brush, "color", text="")
            self.prop_unified_color(row, context, brush, "secondary_color", text="")
            row.separator()
            row.operator("paint.brush_colors_flip", icon='FILE_REFRESH', text="")

            col.separator()
            row = col.row(align=True)
            self.prop_unified_size(row, context, brush, "size", slider=True, text="Radius")
            self.prop_unified_size(row, context, brush, "use_pressure_size")

            row = col.row(align=True)
            self.prop_unified_strength(row, context, brush, "strength", text="Strength")
            self.prop_unified_strength(row, context, brush, "use_pressure_strength")

            col.separator()
            col.prop(brush, "vertex_tool", text="Blend")
            col.prop(brush, "use_alpha")

            if brush.vertex_tool != 'SMEAR':
                col.prop(brush, "use_accumulate")
                col.separator()

            col.prop(brush, "use_frontface", text="Front Faces Only")
            row = col.row()
            row.prop(brush, "use_frontface_falloff", text="Falloff Angle")
            sub = row.row()
            sub.active = brush.use_frontface_falloff
            sub.prop(brush, "falloff_angle", text="")

            col.prop(brush, "use_projected")

            col.separator()
            col.template_ID(settings, "palette", new="palette.new")


class TEXTURE_UL_texpaintslots(UIList):
    def draw_item(self, context, layout, data, item, icon, active_data, active_propname, index):
        mat = data

        if self.layout_type in {'DEFAULT', 'COMPACT'}:
            layout.prop(item, "name", text="", emboss=False, icon_value=icon)
        elif self.layout_type == 'GRID':
            layout.alignment = 'CENTER'
            layout.label(text="")


class VIEW3D_MT_tools_projectpaint_uvlayer(Menu):
    bl_label = "Clone Layer"

    def draw(self, context):
        layout = self.layout

        for i, uv_layer in enumerate(context.active_object.data.uv_layers):
            props = layout.operator("wm.context_set_int", text=uv_layer.name, translate=False)
            props.data_path = "active_object.data.uv_layers.active_index"
            props.value = i


# TODO, move to space_view3d.py
class VIEW3D_PT_slots_projectpaint(View3DPanel, Panel):
    bl_context = ".imagepaint"  # dot on purpose (access from topbar)
    bl_label = "Slots"

    @classmethod
    def poll(cls, context):
        brush = context.tool_settings.image_paint.brush
        ob = context.active_object
        return (brush is not None and ob is not None)

    def draw(self, context):
        layout = self.layout

        settings = context.tool_settings.image_paint
        # brush = settings.brush

        ob = context.active_object
        col = layout.column()

        col.label("Painting Mode:")
        col.prop(settings, "mode", text="")
        col.separator()

        if settings.mode == 'MATERIAL':
            if len(ob.material_slots) > 1:
                col.label("Materials:")
                col.template_list("MATERIAL_UL_matslots", "layers",
                                  ob, "material_slots",
                                  ob, "active_material_index", rows=2)

            mat = ob.active_material
            if mat:
                col.label("Available Paint Slots:")
                col.template_list("TEXTURE_UL_texpaintslots", "",
                                  mat, "texture_paint_images",
                                  mat, "paint_active_slot", rows=2)

                if mat.texture_paint_slots:
                    slot = mat.texture_paint_slots[mat.paint_active_slot]
                else:
                    slot = None

                if slot and slot.is_valid:
                    col.label("UV Map:")
                    col.prop_search(slot, "uv_layer", ob.data, "uv_layers", text="")

        elif settings.mode == 'IMAGE':
            mesh = ob.data
            uv_text = mesh.uv_layers.active.name if mesh.uv_layers.active else ""
            col.label("Canvas Image:")
            col.template_ID(settings, "canvas", new="image.new", open="image.open")
            col.label("UV Map:")
            col.menu("VIEW3D_MT_tools_projectpaint_uvlayer", text=uv_text, translate=False)

        col.separator()
        col.operator("image.save_dirty", text="Save All Images")


# TODO, move to space_view3d.py
class VIEW3D_PT_stencil_projectpaint(View3DPanel, Panel):
    bl_context = ".imagepaint"  # dot on purpose (access from topbar)
    bl_label = "Mask"

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

        toolsettings = context.tool_settings
        ipaint = toolsettings.image_paint
        ob = context.active_object
        mesh = ob.data

        col = layout.column()
        col.active = ipaint.use_stencil_layer

        stencil_text = mesh.uv_layer_stencil.name if mesh.uv_layer_stencil else ""
        split = col.split(0.5)
        colsub = split.column()
        colsub.alignment = 'RIGHT'
        colsub.label("UV Layer")
        split.column().menu("VIEW3D_MT_tools_projectpaint_stencil", text=stencil_text, translate=False)

        # todo this should be combinded into a single row
        split = col.split(0.5)
        colsub = split.column()
        colsub.alignment = 'RIGHT'
        colsub.label("Stencil Image")
        colsub = split.column()
        colsub.template_ID(ipaint, "stencil_image", new="image.new", open="image.open")

        row = col.row(align=True)
        row.prop(ipaint, "stencil_color", text="Display Color")
        row.prop(ipaint, "invert_stencil", text="", icon='IMAGE_ALPHA')


# TODO, move to space_view3d.py
class VIEW3D_PT_tools_brush_overlay(Panel, View3DPaintPanel):
    bl_context = ".paint_common"  # dot on purpose (access from topbar)
    bl_label = "Overlay"

    @classmethod
    def poll(cls, context):
        settings = cls.paint_settings(context)
        return (settings and
                settings.brush and
                (context.sculpt_object or
                 context.vertex_paint_object or
                 context.weight_paint_object or
                 context.image_paint_object))

    def draw(self, context):
        layout = self.layout

        settings = self.paint_settings(context)
        brush = settings.brush
        tex_slot = brush.texture_slot
        tex_slot_mask = brush.mask_texture_slot

        col = layout.column()

        col.label(text="Curve:")

        row = col.row(align=True)
        if brush.use_cursor_overlay:
            row.prop(brush, "use_cursor_overlay", toggle=True, text="", icon='RESTRICT_VIEW_OFF')
        else:
            row.prop(brush, "use_cursor_overlay", toggle=True, text="", icon='RESTRICT_VIEW_ON')

        sub = row.row(align=True)
        sub.prop(brush, "cursor_overlay_alpha", text="Alpha")
        sub.prop(brush, "use_cursor_overlay_override", toggle=True, text="", icon='BRUSH_DATA')

        col.active = brush.brush_capabilities.has_overlay

        if context.image_paint_object or context.sculpt_object or context.vertex_paint_object:
            col.label(text="Texture:")
            row = col.row(align=True)
            if tex_slot.map_mode != 'STENCIL':
                if brush.use_primary_overlay:
                    row.prop(brush, "use_primary_overlay", toggle=True, text="", icon='RESTRICT_VIEW_OFF')
                else:
                    row.prop(brush, "use_primary_overlay", toggle=True, text="", icon='RESTRICT_VIEW_ON')

            sub = row.row(align=True)
            sub.prop(brush, "texture_overlay_alpha", text="Alpha")
            sub.prop(brush, "use_primary_overlay_override", toggle=True, text="", icon='BRUSH_DATA')

        if context.image_paint_object:
            col.label(text="Mask Texture:")

            row = col.row(align=True)
            if tex_slot_mask.map_mode != 'STENCIL':
                if brush.use_secondary_overlay:
                    row.prop(brush, "use_secondary_overlay", toggle=True, text="", icon='RESTRICT_VIEW_OFF')
                else:
                    row.prop(brush, "use_secondary_overlay", toggle=True, text="", icon='RESTRICT_VIEW_ON')

            sub = row.row(align=True)
            sub.prop(brush, "mask_overlay_alpha", text="Alpha")
            sub.prop(brush, "use_secondary_overlay_override", toggle=True, text="", icon='BRUSH_DATA')


# TODO, move to space_view3d.py
class VIEW3D_PT_tools_brush_texture(Panel, View3DPaintPanel):
    bl_context = ".paint_common"  # dot on purpose (access from topbar)
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
class VIEW3D_PT_tools_mask_texture(Panel, View3DPaintPanel):
    bl_context = ".imagepaint"  # dot on purpose (access from topbar)
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
class VIEW3D_PT_tools_brush_stroke(Panel, View3DPaintPanel):
    bl_context = ".paint_common"  # dot on purpose (access from topbar)
    bl_label = "Stroke"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        settings = cls.paint_settings(context)
        return (settings and
                settings.brush and
                (context.sculpt_object or
                 context.vertex_paint_object or
                 context.weight_paint_object or
                 context.image_paint_object))

    def draw(self, context):
        layout = self.layout

        settings = self.paint_settings(context)
        brush = settings.brush
        layout.use_property_split = True

        col = layout.column()

        col.prop(brush, "stroke_method")

        if brush.use_anchor:
            col.separator()
            col.prop(brush, "use_edge_to_edge", "Edge To Edge")

        if brush.use_airbrush:
            col.separator()
            col.prop(brush, "rate", text="Rate", slider=True)

        if brush.use_space:
            col.separator()
            row = col.row(align=True)
            row.prop(brush, "spacing", text="Spacing")
            row.prop(brush, "use_pressure_spacing", toggle=True, text="")

        if brush.use_line or brush.use_curve:
            col.separator()
            row = col.row(align=True)
            row.prop(brush, "spacing", text="Spacing")

        if brush.use_curve:
            col.separator()
            col.template_ID(brush, "paint_curve", new="paintcurve.new")
            col.operator("paintcurve.draw")

        if context.sculpt_object:
            if brush.sculpt_capabilities.has_jitter:
                col.separator()

                colsub = col.split(0.5)
                row = colsub.row(align=True)
                row.alignment = 'RIGHT'
                row.label("Jitter")
                row = colsub.row(align=True)
                row.prop(brush, "use_relative_jitter", icon_only=True)
                if brush.use_relative_jitter:
                    row.prop(brush, "jitter", slider=True, text="")
                else:
                    row.prop(brush, "jitter_absolute", text="")
                row.prop(brush, "use_pressure_jitter", toggle=True, text="")

            if brush.sculpt_capabilities.has_smooth_stroke:
                col = layout.column()
                col.separator()

                col.prop(brush, "use_smooth_stroke")

                sub = col.column()
                sub.active = brush.use_smooth_stroke
                sub.prop(brush, "smooth_stroke_radius", text="Radius", slider=True)
                sub.prop(brush, "smooth_stroke_factor", text="Factor", slider=True)
        else:
            col.separator()

            row = col.row(align=True)
            row.prop(brush, "use_relative_jitter", icon_only=True)
            if brush.use_relative_jitter:
                row.prop(brush, "jitter", slider=True)
            else:
                row.prop(brush, "jitter_absolute")
            row.prop(brush, "use_pressure_jitter", toggle=True, text="")

            col = layout.column()
            col.separator()

            if brush.brush_capabilities.has_smooth_stroke:
                col.prop(brush, "use_smooth_stroke")

                sub = col.column()
                sub.active = brush.use_smooth_stroke
                sub.prop(brush, "smooth_stroke_radius", text="Radius", slider=True)
                sub.prop(brush, "smooth_stroke_factor", text="Factor", slider=True)

        layout.prop(settings, "input_samples")


# TODO, move to space_view3d.py
class VIEW3D_PT_tools_brush_curve(Panel, View3DPaintPanel):
    bl_context = ".paint_common"  # dot on purpose (access from topbar)
    bl_label = "Curve"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        settings = cls.paint_settings(context)
        return (settings and settings.brush and settings.brush.curve)

    def draw(self, context):
        layout = self.layout

        settings = self.paint_settings(context)

        brush = settings.brush

        layout.template_curve_mapping(brush, "curve", brush=True)

        col = layout.column(align=True)
        row = col.row(align=True)
        row.operator("brush.curve_preset", icon='SMOOTHCURVE', text="").shape = 'SMOOTH'
        row.operator("brush.curve_preset", icon='SPHERECURVE', text="").shape = 'ROUND'
        row.operator("brush.curve_preset", icon='ROOTCURVE', text="").shape = 'ROOT'
        row.operator("brush.curve_preset", icon='SHARPCURVE', text="").shape = 'SHARP'
        row.operator("brush.curve_preset", icon='LINCURVE', text="").shape = 'LINE'
        row.operator("brush.curve_preset", icon='NOCURVE', text="").shape = 'MAX'


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

        toolsettings = context.tool_settings
        sculpt = toolsettings.sculpt
        settings = self.paint_settings(context)
        brush = settings.brush

        col = layout.column()
        col.active = context.sculpt_object.use_dynamic_topology_sculpting

        sub = col.column()
        sub.active = (brush and brush.sculpt_tool != 'MASK')
        if sculpt.detail_type_method in {'CONSTANT', 'MANUAL'}:
            row = sub.row(align=True)
            row.prop(sculpt, "constant_detail_resolution")
            row.operator("sculpt.sample_detail_size", text="", icon='EYEDROPPER')
        elif (sculpt.detail_type_method == 'BRUSH'):
            sub.prop(sculpt, "detail_percent")
        else:
            sub.prop(sculpt, "detail_size")
        sub.prop(sculpt, "detail_refine_method", text="Refine Method")
        sub.prop(sculpt, "detail_type_method", text="Detailing")

        col.prop(sculpt, "use_smooth_shading")

        col.separator()

        col.prop(sculpt, "symmetrize_direction")
        col.operator("sculpt.symmetrize")
        col.operator("sculpt.optimize")
        if sculpt.detail_type_method in {'CONSTANT', 'MANUAL'}:
            col.operator("sculpt.detail_flood_fill")


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
        # scene = context.scene

        toolsettings = context.tool_settings
        sculpt = toolsettings.sculpt
        capabilities = sculpt.brush.sculpt_capabilities

        col = layout.column(align=True)
        col.active = capabilities.has_gravity
        col.label(text="Gravity:")
        col.prop(sculpt, "gravity", slider=True, text="Factor")
        col.prop(sculpt, "gravity_object")
        col.separator()

        layout.prop(sculpt, "use_threaded", text="Threaded Sculpt")
        layout.prop(sculpt, "show_low_resolution")
        layout.prop(sculpt, "use_deform_only")
        layout.prop(sculpt, "show_diffuse_color")
        layout.prop(sculpt, "show_mask")

        self.unified_paint_settings(layout, context)


# TODO, move to space_view3d.py
class VIEW3D_PT_sculpt_symmetry(Panel, View3DPaintPanel):
    bl_context = ".sculpt_mode"  # dot on purpose (access from topbar)
    bl_label = "Symmetry/Lock"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        return (context.sculpt_object and context.tool_settings.sculpt)

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

        layout.prop(sculpt, "use_symmetry_feather", text="Feather")
        layout.column().prop(sculpt, "radial_symmetry", text="Radial")
        layout.column().prop(sculpt, "tile_offset", text="Tile Offset")


# TODO, move to space_view3d.py
class VIEW3D_PT_tools_brush_appearance(Panel, View3DPaintPanel):
    bl_context = ".paint_common"  # dot on purpose (access from topbar)
    bl_label = "Appearance"

    @classmethod
    def poll(cls, context):
        settings = cls.paint_settings(context)
        return (settings is not None) and (not isinstance(settings, bpy.types.ParticleEdit))

    def draw(self, context):
        layout = self.layout

        settings = self.paint_settings(context)
        brush = settings.brush

        if brush is None:  # unlikely but can happen
            layout.label(text="Brush Unset")
            return

        col = layout.column()
        col.prop(settings, "show_brush")

        sub = col.column()
        sub.active = settings.show_brush

        if context.sculpt_object and context.tool_settings.sculpt:
            if brush.sculpt_capabilities.has_secondary_color:
                sub.row().prop(brush, "cursor_color_add", text="Add")
                sub.row().prop(brush, "cursor_color_subtract", text="Subtract")
            else:
                sub.prop(brush, "cursor_color_add", text="")
        else:
            sub.prop(brush, "cursor_color_add", text="")

        col.separator()

        col = col.column(align=True)
        col.prop(brush, "use_custom_icon")
        sub = col.column()
        sub.active = brush.use_custom_icon
        sub.prop(brush, "icon_filepath", text="")

# ********** default tools for weight-paint ****************


# TODO, move to space_view3d.py
class VIEW3D_PT_tools_weightpaint_symmetry(Panel, View3DPaintPanel):
    bl_context = ".weightpaint"
    bl_options = {'DEFAULT_CLOSED'}
    bl_label = "Symmetry"

    def draw(self, context):
        layout = self.layout
        toolsettings = context.tool_settings
        wpaint = toolsettings.weight_paint
        draw_vpaint_symmetry(layout, wpaint)


# TODO, move to space_view3d.py
class VIEW3D_PT_tools_weightpaint_options(Panel, View3DPaintPanel):
    bl_context = ".weightpaint"
    bl_label = "Options"

    def draw(self, context):
        layout = self.layout

        tool_settings = context.tool_settings
        wpaint = tool_settings.weight_paint

        col = layout.column()
        col.prop(wpaint, "use_group_restrict")

        obj = context.weight_paint_object
        if obj.type == 'MESH':
            mesh = obj.data
            col.prop(mesh, "use_mirror_x")
            row = col.row()
            row.active = mesh.use_mirror_x
            row.prop(mesh, "use_mirror_topology")

        col.label("Show Zero Weights:")
        sub = col.row()
        sub.prop(tool_settings, "vertex_group_user", expand=True)

        self.unified_paint_settings(col, context)

# ********** default tools for vertex-paint ****************


# TODO, move to space_view3d.py
class VIEW3D_PT_tools_vertexpaint(Panel, View3DPaintPanel):
    bl_context = ".vertexpaint"  # dot on purpose (access from topbar)
    bl_label = "Options"

    def draw(self, context):
        layout = self.layout

        toolsettings = context.tool_settings
        vpaint = toolsettings.vertex_paint

        col = layout.column()

        self.unified_paint_settings(col, context)


# TODO, move to space_view3d.py
class VIEW3D_PT_tools_vertexpaint_symmetry(Panel, View3DPaintPanel):
    bl_context = ".vertexpaint"  # dot on purpose (access from topbar)
    bl_options = {'DEFAULT_CLOSED'}
    bl_label = "Symmetry"

    def draw(self, context):
        layout = self.layout
        toolsettings = context.tool_settings
        vpaint = toolsettings.vertex_paint
        draw_vpaint_symmetry(layout, vpaint)


# ********** default tools for texture-paint ****************


# TODO, move to space_view3d.py
class VIEW3D_PT_tools_imagepaint_external(Panel, View3DPaintPanel):
    bl_context = ".imagepaint"  # dot on purpose (access from topbar)
    bl_label = "External"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout

        toolsettings = context.tool_settings
        ipaint = toolsettings.image_paint

        col = layout.column()
        row = col.split(align=True, percentage=0.55)
        row.operator("image.project_edit", text="Quick Edit")
        row.operator("image.project_apply", text="Apply")

        col.row().prop(ipaint, "screen_grab_size", text="")

        col.operator("paint.project_image", text="Apply Camera Image")


# TODO, move to space_view3d.py
class VIEW3D_PT_tools_imagepaint_symmetry(Panel, View3DPaintPanel):
    bl_context = ".imagepaint"  # dot on purpose (access from topbar)
    bl_label = "Symmetry"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout

        toolsettings = context.tool_settings
        ipaint = toolsettings.image_paint

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
class VIEW3D_PT_tools_projectpaint(View3DPaintPanel, Panel):
    bl_context = ".imagepaint"  # dot on purpose (access from topbar)
    bl_label = "Project Paint"

    @classmethod
    def poll(cls, context):
        brush = context.tool_settings.image_paint.brush
        return (brush is not None)

    def draw(self, context):
        layout = self.layout

        toolsettings = context.tool_settings
        ipaint = toolsettings.image_paint

        col = layout.column()

        col.prop(ipaint, "use_occlude")
        col.prop(ipaint, "use_backface_culling")

        row = layout.row()
        row.prop(ipaint, "use_normal_falloff")

        sub = row.row()
        sub.active = (ipaint.use_normal_falloff)
        sub.prop(ipaint, "normal_angle", text="")

        layout.prop(ipaint, "use_cavity")
        if ipaint.use_cavity:
            layout.template_curve_mapping(ipaint, "cavity_curve", brush=True)

        layout.prop(ipaint, "seam_bleed")
        layout.prop(ipaint, "dither")
        self.unified_paint_settings(layout, context)


# TODO, move to space_view3d.py
class VIEW3D_PT_imagepaint_options(View3DPaintPanel):
    bl_label = "Options"

    @classmethod
    def poll(cls, context):
        return (context.image_paint_object and context.tool_settings.image_paint)

    def draw(self, context):
        layout = self.layout

        col = layout.column()
        self.unified_paint_settings(col, context)


class VIEW3D_MT_tools_projectpaint_stencil(Menu):
    bl_label = "Mask Layer"

    def draw(self, context):
        layout = self.layout
        for i, uv_layer in enumerate(context.active_object.data.uv_layers):
            props = layout.operator("wm.context_set_int", text=uv_layer.name, translate=False)
            props.data_path = "active_object.data.uv_layer_stencil_index"
            props.value = i


# TODO, move to space_view3d.py
class VIEW3D_PT_tools_particlemode(View3DPanel, Panel):
    """Default tools for particle mode"""
    bl_context = ".particlemode"
    bl_label = "Options"

    def draw(self, context):
        layout = self.layout

        pe = context.tool_settings.particle_edit
        ob = pe.object

        layout.prop(pe, "type", text="")

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
        if pe.is_hair:
            col.active = pe.is_editable
            col.prop(pe, "use_emitter_deflect", text="Deflect Emitter")
            sub = col.row(align=True)
            sub.active = pe.use_emitter_deflect
            sub.prop(pe, "emitter_distance", text="Distance")

        col = layout.column(align=True)
        col.active = pe.is_editable
        col.label(text="Keep:")
        col.prop(pe, "use_preserve_length", text="Lengths")
        col.prop(pe, "use_preserve_root", text="Root")
        if not pe.is_hair:
            col.label(text="Correct:")
            col.prop(pe, "use_auto_velocity", text="Velocity")
        col.prop(ob.data, "use_mirror_x")

        col.prop(pe, "shape_object")
        col.operator("particle.shape_cut")

        col = layout.column(align=True)
        col.active = pe.is_editable
        col.label(text="Draw:")
        col.prop(pe, "draw_step", text="Path Steps")
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
class VIEW3D_PT_tools_grease_pencil_brush(View3DPanel, Panel):
    bl_context = ".greasepencil_paint"
    bl_label = "Brush"

    @classmethod
    def poll(cls, context):
        is_3d_view = context.space_data.type == 'VIEW_3D'
        if is_3d_view:
            if context.gpencil_data is None:
                return False

            gpd = context.gpencil_data
            return bool(gpd.is_stroke_paint_mode)
        else:
            return True

    @staticmethod
    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        ts = context.scene.tool_settings
        settings = ts.gpencil_paint

        row = layout.row()
        col = row.column()
        col.template_ID_preview(settings, "brush", new="brush.add_gpencil", rows=3, cols=8)

        col = row.column()
        brush = context.active_gpencil_brush
        gp_settings = brush.gpencil_settings

        sub = col.column(align=True)
        sub.operator("gpencil.brush_presets_create", icon='HELP', text="")

        if brush is not None:
            # XXX: Items in "sub" currently show up beside the brush selector in a separate column
            if gp_settings.gpencil_brush_type == 'ERASE':
                sub.prop(gp_settings, "default_eraser", text="")

            # Brush details
            if gp_settings.gpencil_brush_type == 'ERASE':
                col = layout.column(align=True)
                col.prop(brush, "size", text="Radius")

                col.separator()
                row = col.row()
                row.prop(gp_settings, "eraser_mode", expand=True)
            elif gp_settings.gpencil_brush_type == 'FILL':
                col = layout.column(align=True)
                col.prop(gp_settings, "gpencil_fill_leak", text="Leak Size")
                col.prop(brush, "size", text="Thickness")
                col.prop(gp_settings, "gpencil_fill_simplyfy_level", text="Simplify")

                col = layout.row(align=True)
                col.template_ID(gp_settings, "material")

                row = layout.row(align=True)
                row.prop(gp_settings, "gpencil_fill_draw_mode", text="Boundary Draw Mode")
                row.prop(gp_settings, "gpencil_fill_show_boundary", text="", icon='GRID')

                col = layout.column(align=True)
                col.enabled = gp_settings.gpencil_fill_draw_mode != "STROKE"
                col.prop(gp_settings, "gpencil_fill_hide", text="Hide Transparent Lines")
                sub = col.row(align=True)
                sub.enabled = gp_settings.gpencil_fill_hide
                sub.prop(gp_settings, "gpencil_fill_threshold", text="Threshold")
            else:  # bgpsettings.gpencil_brush_type == 'DRAW':
                row = layout.row(align=True)
                row.prop(brush, "size", text="Radius")
                row.prop(gp_settings, "use_pressure", text="", icon='STYLUS_PRESSURE')
                row = layout.row(align=True)
                row.prop(gp_settings, "pen_strength", slider=True)
                row.prop(gp_settings, "use_strength_pressure", text="", icon='STYLUS_PRESSURE')

                row = layout.row(align=True)
                row.template_ID(gp_settings, "material")


# Grease Pencil drawing brushes options
class VIEW3D_PT_tools_grease_pencil_brush_option(View3DPanel, Panel):
    bl_context = ".greasepencil_paint"
    bl_label = "Options"
    bl_options = {'DEFAULT_CLOSED'}

    def draw_header_preset(self, context):
        VIEW3D_PT_gpencil_brush_presets.draw_panel_header(self.layout)

    @staticmethod
    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        brush = context.active_gpencil_brush
        gp_settings = brush.gpencil_settings

        if brush is not None:
            col = layout.column(align=True)
            col.prop(gp_settings, "input_samples")
            col.separator()

            col.prop(gp_settings, "active_smooth_factor")
            col.separator()

            col.prop(gp_settings, "angle", slider=True)
            col.prop(gp_settings, "angle_factor", text="Factor", slider=True)
            col.separator()


class VIEW3D_PT_tools_grease_pencil_brush_stabilizer(View3DPanel, Panel):
    bl_context = ".greasepencil_paint"
    bl_parent_id = 'VIEW3D_PT_tools_grease_pencil_brush_option'
    bl_label = "Stabilizer"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        brush = context.active_gpencil_brush
        gp_settings = brush.gpencil_settings

        return brush is not None and gp_settings.gpencil_brush_type == 'DRAW'

    def draw_header(self, context):
        brush = context.active_gpencil_brush
        gp_settings = brush.gpencil_settings
        self.layout.prop(gp_settings, "use_stabilizer", text="")

    @staticmethod
    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        brush = context.active_gpencil_brush
        gp_settings = brush.gpencil_settings
        layout.active = gp_settings.use_stabilizer

        layout.prop(brush, "smooth_stroke_radius", text="Radius", slider=True)
        layout.prop(brush, "smooth_stroke_factor", text="Factor", slider=True)


class VIEW3D_PT_tools_grease_pencil_brush_settings(View3DPanel, Panel):
    bl_context = ".greasepencil_paint"
    bl_parent_id = 'VIEW3D_PT_tools_grease_pencil_brush_option'
    bl_label = "Post-processing Settings"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        brush = context.active_gpencil_brush

        return brush is not None

    def draw_header(self, context):
        brush = context.active_gpencil_brush
        gp_settings = brush.gpencil_settings
        self.layout.prop(gp_settings, "enable_settings", text="")

    @staticmethod
    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        brush = context.active_gpencil_brush
        gp_settings = brush.gpencil_settings
        layout.active = gp_settings.enable_settings

        col = layout.column(align=True)
        col.prop(gp_settings, "pen_smooth_factor")
        col.prop(gp_settings, "pen_thick_smooth_factor")

        col = layout.column(align=True)
        col.prop(gp_settings, "pen_smooth_steps")
        col.prop(gp_settings, "pen_thick_smooth_steps")

        col = layout.column(align=True)
        col.prop(gp_settings, "pen_subdivision_steps")
        col.prop(gp_settings, "random_subdiv", text="Randomness", slider=True)


class VIEW3D_PT_tools_grease_pencil_brush_random(View3DPanel, Panel):
    bl_context = ".greasepencil_paint"
    bl_parent_id = 'VIEW3D_PT_tools_grease_pencil_brush_option'
    bl_label = "Random Settings"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        brush = context.active_gpencil_brush

        return brush is not None

    def draw_header(self, context):
        brush = context.active_gpencil_brush
        gp_settings = brush.gpencil_settings
        self.layout.prop(gp_settings, "enable_random", text="")

    @staticmethod
    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        brush = context.active_gpencil_brush
        gp_settings = brush.gpencil_settings
        layout.active = gp_settings.enable_random

        layout.prop(gp_settings, "random_pressure", text="Pressure", slider=True)
        layout.prop(gp_settings, "random_strength", text="Strength", slider=True)
        layout.prop(gp_settings, "uv_random", text="UV", slider=True)

        row = layout.row(align=True)
        row.prop(gp_settings, "pen_jitter", slider=True)
        row.prop(gp_settings, "use_jitter_pressure", text="", icon='STYLUS_PRESSURE')


# Grease Pencil drawingcurves
class VIEW3D_PT_tools_grease_pencil_brushcurves(View3DPanel, Panel):
    bl_context = ".greasepencil_paint"
    bl_label = "Curves"
    bl_options = {'DEFAULT_CLOSED'}

    @staticmethod
    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        brush = context.active_gpencil_brush
        gp_settings = brush.gpencil_settings

        # Brush
        layout.label("Sensitivity")
        layout.template_curve_mapping(gp_settings, "curve_sensitivity", brush=True)

        layout.label("Strength")
        layout.template_curve_mapping(gp_settings, "curve_strength", brush=True)

        layout.label("Jitter")
        layout.template_curve_mapping(gp_settings, "curve_jitter", brush=True)


# Grease Pencil create shapes
class VIEW3D_PT_tools_grease_pencil_shapes(View3DPanel, Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'
    bl_label = "Shapes"

    @classmethod
    def poll(cls, context):
        ob = context.active_object
        return ob and ob.type == 'GPENCIL'

    @staticmethod
    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        col = layout.column(align=True)
        col.operator("gpencil.primitive", text="Line", icon='IPO_CONSTANT').type = 'LINE'
        col.operator("gpencil.primitive", text="Rectangle", icon='UV_FACESEL').type = 'BOX'
        col.operator("gpencil.primitive", text="Circle", icon='ANTIALIASED').type = 'CIRCLE'

        layout.operator("object.gpencil_add", text="Monkey", icon='MONKEY').type = 'MONKEY'


# Grease Pencil stroke editing tools
class VIEW3D_PT_tools_grease_pencil_edit(GreasePencilStrokeEditPanel, Panel):
    bl_space_type = 'VIEW_3D'


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

    @staticmethod
    def draw(self, context):
        layout = self.layout
        settings = context.tool_settings.gpencil_interpolate

        col = layout.column(align=True)
        col.label("Interpolate Strokes")
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
            col.template_curve_mapping(settings, "interpolation_curve", brush=True)
        elif settings.type != 'LINEAR':
            col.prop(settings, "easing")

            if settings.type == 'BACK':
                layout.prop(settings, "back")
            elif setting.type == 'ELASTIC':
                sub = layout.column(align=True)
                sub.prop(settings, "amplitude")
                sub.prop(settings, "period")


# Grease Pencil stroke sculpting tools
class VIEW3D_PT_tools_grease_pencil_sculpt(GreasePencilStrokeSculptPanel, View3DPanel, Panel):
    bl_context = ".greasepencil_sculpt"
    bl_category = "Tools"
    bl_label = "Sculpt Strokes"


# Grease Pencil weight painting tools
class VIEW3D_PT_tools_grease_pencil_weight_paint(View3DPanel, Panel):
    bl_context = ".greasepencil_weight"
    bl_category = "Tools"
    bl_label = "Weight Paint"

    @staticmethod
    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        gpd = context.gpencil_data
        settings = context.tool_settings.gpencil_sculpt
        tool = settings.tool
        brush = settings.brush

        layout.template_icon_view(settings, "weight_tool", show_labels=True)

        col = layout.column()
        col.prop(brush, "size", slider=True)
        row = col.row(align=True)
        row.prop(brush, "strength", slider=True)
        row.prop(brush, "use_pressure_strength", text="")

        col.prop(brush, "use_falloff")


# Grease Pencil Brush Appeareance (one for each mode)
class VIEW3D_PT_tools_grease_pencil_paint_appearance(GreasePencilAppearancePanel, View3DPanel, Panel):
    bl_context = ".greasepencil_paint"
    bl_label = "Appearance"


class VIEW3D_PT_tools_grease_pencil_sculpt_appearance(GreasePencilAppearancePanel, View3DPanel, Panel):
    bl_context = ".greasepencil_sculpt"
    bl_label = "Appearance"


class VIEW3D_PT_tools_grease_pencil_weight_appearance(GreasePencilAppearancePanel, View3DPanel, Panel):
    bl_context = ".greasepencil_weight"
    bl_label = "Appearance"


class VIEW3D_PT_gpencil_brush_presets(PresetMenu):
    """Brush settings"""
    bl_label = "Brush Presets"
    preset_subdir = "gpencil_brush"
    preset_operator = "script.execute_preset"
    preset_add_operator = "scene.gpencil_brush_preset_add"


classes = (
    VIEW3D_PT_tools_meshedit_options,
    VIEW3D_PT_tools_curveedit_options_stroke,
    VIEW3D_PT_tools_armatureedit_options,
    VIEW3D_PT_tools_posemode_options,
    VIEW3D_PT_imapaint_tools_missing,
    VIEW3D_PT_tools_brush,
    TEXTURE_UL_texpaintslots,
    VIEW3D_MT_tools_projectpaint_uvlayer,
    VIEW3D_PT_slots_projectpaint,
    VIEW3D_PT_stencil_projectpaint,
    VIEW3D_PT_tools_brush_overlay,
    VIEW3D_PT_tools_brush_texture,
    VIEW3D_PT_tools_mask_texture,
    VIEW3D_PT_tools_brush_stroke,
    VIEW3D_PT_tools_brush_curve,
    VIEW3D_PT_sculpt_dyntopo,
    VIEW3D_PT_sculpt_options,
    VIEW3D_PT_sculpt_symmetry,
    VIEW3D_PT_tools_brush_appearance,
    VIEW3D_PT_tools_weightpaint_symmetry,
    VIEW3D_PT_tools_weightpaint_options,
    VIEW3D_PT_tools_vertexpaint,
    VIEW3D_PT_tools_vertexpaint_symmetry,
    VIEW3D_PT_tools_imagepaint_external,
    VIEW3D_PT_tools_imagepaint_symmetry,
    VIEW3D_PT_tools_projectpaint,
    VIEW3D_MT_tools_projectpaint_stencil,
    VIEW3D_PT_tools_particlemode,

    VIEW3D_PT_gpencil_brush_presets,
    VIEW3D_PT_tools_grease_pencil_brush,
    VIEW3D_PT_tools_grease_pencil_brush_option,
    VIEW3D_PT_tools_grease_pencil_brush_settings,
    VIEW3D_PT_tools_grease_pencil_brush_stabilizer,
    VIEW3D_PT_tools_grease_pencil_brush_random,
    VIEW3D_PT_tools_grease_pencil_brushcurves,
    VIEW3D_PT_tools_grease_pencil_shapes,
    VIEW3D_PT_tools_grease_pencil_sculpt,
    VIEW3D_PT_tools_grease_pencil_weight_paint,
    VIEW3D_PT_tools_grease_pencil_paint_appearance,
    VIEW3D_PT_tools_grease_pencil_sculpt_appearance,
    VIEW3D_PT_tools_grease_pencil_weight_appearance,
    VIEW3D_PT_tools_grease_pencil_interpolate,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
