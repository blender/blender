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
from bpy.types import Menu, UIList, Operator
from bpy.app.translations import pgettext_iface as iface_


# XXX: To be replaced with active tools
# Currently only used by the clip editor
class AnnotationDrawingToolsPanel:
    # subclass must set
    # bl_space_type = 'IMAGE_EDITOR'
    bl_label = "Annotation"
    bl_category = "Annotation"
    bl_region_type = 'TOOLS'

    def draw(self, context):
        layout = self.layout

        tool_settings = context.tool_settings

        col = layout.column(align=True)

        col.label(text="Draw:")
        row = col.row(align=True)
        row.operator("gpencil.annotate", icon='GREASEPENCIL', text="Draw").mode = 'DRAW'
        # XXX: Needs a dedicated icon
        row.operator("gpencil.annotate", icon='FORCE_CURVE', text="Erase").mode = 'ERASER'

        row = col.row(align=True)
        row.operator("gpencil.annotate", icon='LINE_DATA', text="Line").mode = 'DRAW_STRAIGHT'
        row.operator("gpencil.annotate", icon='MESH_DATA', text="Poly").mode = 'DRAW_POLY'

        col.separator()

        col.label(text="Stroke Placement:")
        row = col.row(align=True)
        row.prop_enum(tool_settings, "annotation_stroke_placement_view2d", 'VIEW')
        row.prop_enum(tool_settings, "annotation_stroke_placement_view2d", 'CURSOR', text="Cursor")


class GreasePencilSculptOptionsPanel:
    bl_label = "Sculpt Strokes"

    @classmethod
    def poll(cls, context):
        tool_settings = context.scene.tool_settings
        settings = tool_settings.gpencil_sculpt_paint
        brush = settings.brush
        tool = brush.gpencil_sculpt_tool

        return bool(tool in {'SMOOTH', 'RANDOMIZE'})

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        tool_settings = context.scene.tool_settings
        settings = tool_settings.gpencil_sculpt_paint
        brush = settings.brush
        gp_settings = brush.gpencil_settings
        tool = brush.gpencil_sculpt_tool

        if tool in {'SMOOTH', 'RANDOMIZE'}:
            layout.prop(gp_settings, "use_edit_position", text="Affect Position")
            layout.prop(gp_settings, "use_edit_strength", text="Affect Strength")
            layout.prop(gp_settings, "use_edit_thickness", text="Affect Thickness")

            if tool == 'SMOOTH':
                layout.prop(gp_settings, "use_edit_pressure")

            layout.prop(gp_settings, "use_edit_uv", text="Affect UV")


# GP Object Tool Settings
class GreasePencilDisplayPanel:
    bl_label = "Brush Tip"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        ob = context.active_object
        brush = context.tool_settings.gpencil_paint.brush
        if ob and ob.type == 'GPENCIL' and brush:
            if context.mode == 'PAINT_GPENCIL':
                return brush.gpencil_tool != 'ERASE'
            else:
                # GP Sculpt, Vertex and Weight Paint always have Brush Tip panel.
                return True
        return False

    def draw_header(self, context):
        if self.is_popover:
            return

        tool_settings = context.tool_settings
        if context.mode == 'PAINT_GPENCIL':
            settings = tool_settings.gpencil_paint
        elif context.mode == 'SCULPT_GPENCIL':
            settings = tool_settings.gpencil_sculpt_paint
        elif context.mode == 'WEIGHT_GPENCIL':
            settings = tool_settings.gpencil_weight_paint
        elif context.mode == 'VERTEX_GPENCIL':
            settings = tool_settings.gpencil_vertex_paint
        brush = settings.brush
        if brush:
            self.layout.prop(settings, "show_brush", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        tool_settings = context.tool_settings
        if context.mode == 'PAINT_GPENCIL':
            settings = tool_settings.gpencil_paint
        elif context.mode == 'SCULPT_GPENCIL':
            settings = tool_settings.gpencil_sculpt_paint
        elif context.mode == 'WEIGHT_GPENCIL':
            settings = tool_settings.gpencil_weight_paint
        elif context.mode == 'VERTEX_GPENCIL':
            settings = tool_settings.gpencil_vertex_paint
        brush = settings.brush
        gp_settings = brush.gpencil_settings

        ob = context.active_object
        if ob.mode == 'PAINT_GPENCIL':

            if self.is_popover:
                row = layout.row(align=True)
                row.prop(settings, "show_brush", text="")
                row.label(text="Display Cursor")

            col = layout.column(align=True)
            col.active = settings.show_brush

            if brush.gpencil_tool == 'DRAW':
                col.prop(gp_settings, "show_lasso", text="Show Fill Color While Drawing")

        elif ob.mode == 'SCULPT_GPENCIL':
            col = layout.column(align=True)
            col.active = settings.show_brush

            col.prop(brush, "cursor_color_add", text="Cursor Color")
            if brush.gpencil_sculpt_tool in {'THICKNESS', 'STRENGTH', 'PINCH', 'TWIST'}:
                col.prop(brush, "cursor_color_subtract", text="Inverse Color")

        elif ob.mode == 'WEIGHT_GPENCIL':
            col = layout.column(align=True)
            col.active = settings.show_brush

            col.prop(brush, "cursor_color_add", text="Cursor Color")

        elif ob.mode == 'VERTEX_GPENCIL':
            row = layout.row(align=True)
            row.prop(settings, "show_brush", text="")
            row.label(text="Display Cursor")


class GreasePencilBrushFalloff:
    bl_label = "Falloff"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
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

        return (settings and settings.brush and settings.brush.curve)

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

        if settings:
            brush = settings.brush

            col = layout.column(align=True)
            row = col.row(align=True)
            row.prop(brush, "curve_preset", text="")

            if brush.curve_preset == 'CUSTOM':
                layout.template_curve_mapping(brush, "curve", brush=True)

                col = layout.column(align=True)
                row = col.row(align=True)
                row.operator("brush.curve_preset", icon='SMOOTHCURVE', text="").shape = 'SMOOTH'
                row.operator("brush.curve_preset", icon='SPHERECURVE', text="").shape = 'ROUND'
                row.operator("brush.curve_preset", icon='ROOTCURVE', text="").shape = 'ROOT'
                row.operator("brush.curve_preset", icon='SHARPCURVE', text="").shape = 'SHARP'
                row.operator("brush.curve_preset", icon='LINCURVE', text="").shape = 'LINE'
                row.operator("brush.curve_preset", icon='NOCURVE', text="").shape = 'MAX'


class GPENCIL_MT_snap(Menu):
    bl_label = "Snap"

    def draw(self, _context):
        layout = self.layout

        layout.operator("gpencil.snap_to_grid", text="Selection to Grid")
        layout.operator("gpencil.snap_to_cursor", text="Selection to Cursor").use_offset = False
        layout.operator("gpencil.snap_to_cursor", text="Selection to Cursor (Keep Offset)").use_offset = True

        layout.separator()

        layout.operator("gpencil.snap_cursor_to_selected", text="Cursor to Selected")
        layout.operator("view3d.snap_cursor_to_center", text="Cursor to World Origin")
        layout.operator("view3d.snap_cursor_to_grid", text="Cursor to Grid")


class GPENCIL_MT_snap_pie(Menu):
    bl_label = "Snap"

    def draw(self, _context):
        layout = self.layout
        pie = layout.menu_pie()

        pie.operator("view3d.snap_cursor_to_grid", text="Cursor to Grid", icon='CURSOR')
        pie.operator("gpencil.snap_to_grid", text="Selection to Grid", icon='RESTRICT_SELECT_OFF')
        pie.operator("gpencil.snap_cursor_to_selected", text="Cursor to Selected", icon='CURSOR')
        pie.operator(
            "gpencil.snap_to_cursor",
            text="Selection to Cursor",
            icon='RESTRICT_SELECT_OFF'
        ).use_offset = False
        pie.operator(
            "gpencil.snap_to_cursor",
            text="Selection to Cursor (Keep Offset)",
            icon='RESTRICT_SELECT_OFF'
        ).use_offset = True
        pie.separator()
        pie.operator("view3d.snap_cursor_to_center", text="Cursor to World Origin", icon='CURSOR')
        pie.separator()


class GPENCIL_MT_move_to_layer(Menu):
    bl_label = "Move to Layer"

    def draw(self, context):
        layout = self.layout
        gpd = context.gpencil_data
        if gpd:
            gpl_active = context.active_gpencil_layer
            tot_layers = len(gpd.layers)
            i = tot_layers - 1
            while i >= 0:
                gpl = gpd.layers[i]
                if gpl.info == gpl_active.info:
                    icon = 'GREASEPENCIL'
                else:
                    icon = 'NONE'
                layout.operator("gpencil.move_to_layer", text=gpl.info, icon=icon).layer = i
                i -= 1

            layout.separator()

        layout.operator("gpencil.move_to_layer", text="New Layer", icon='ADD').layer = -1


class GPENCIL_MT_layer_active(Menu):
    bl_label = "Change Active Layer"

    def draw(self, context):
        layout = self.layout
        layout.operator_context = 'INVOKE_REGION_WIN'

        gpd = context.gpencil_data
        if gpd:
            gpl_active = context.active_gpencil_layer
            tot_layers = len(gpd.layers)
            i = tot_layers - 1
            while i >= 0:
                gpl = gpd.layers[i]
                if gpl.info == gpl_active.info:
                    icon = 'GREASEPENCIL'
                else:
                    icon = 'NONE'
                layout.operator("gpencil.layer_active", text=gpl.info, icon=icon).layer = i
                i -= 1

            layout.separator()

        layout.operator("gpencil.layer_add", text="New Layer", icon='ADD')


class GPENCIL_MT_material_active(Menu):
    bl_label = "Change Active Material"

    @classmethod
    def poll(cls, context):
        ob = context.active_object
        if ob is None or len(ob.material_slots) == 0:
            return False

        return True

    def draw(self, context):
        layout = self.layout
        layout.operator_context = 'INVOKE_REGION_WIN'
        ob = context.active_object

        for slot in ob.material_slots:
            mat = slot.material
            if mat:
                icon = mat.id_data.preview.icon_id
                layout.operator("gpencil.material_set", text=mat.name, icon_value=icon).slot = mat.name


class GPENCIL_MT_gpencil_draw_delete(Menu):
    bl_label = "Delete"

    def draw(self, _context):
        layout = self.layout

        layout.operator_context = 'INVOKE_REGION_WIN'

        layout.operator("gpencil.delete", text="Delete Active Keyframe (Active Layer)").type = 'FRAME'
        layout.operator("gpencil.active_frames_delete_all", text="Delete Active Keyframes (All Layers)")


class GPENCIL_MT_cleanup(Menu):
    bl_label = "Clean Up"

    def draw(self, context):

        ob = context.active_object

        layout = self.layout

        layout.operator("gpencil.frame_clean_fill", text="Boundary Strokes").mode = 'ACTIVE'
        layout.operator("gpencil.frame_clean_fill", text="Boundary Strokes all Frames").mode = 'ALL'

        layout.separator()

        layout.operator("gpencil.frame_clean_loose", text="Delete Loose Points")

        if ob.mode != 'PAINT_GPENCIL':
            layout.operator("gpencil.stroke_merge_by_distance", text="Merge by Distance")

        layout.separator()

        layout.operator("gpencil.frame_clean_duplicate", text="Delete Duplicated Frames")
        layout.operator("gpencil.recalc_geometry", text="Recalculate Geometry")
        if ob.mode != 'PAINT_GPENCIL':
            layout.operator("gpencil.reproject")


class GPENCIL_UL_annotation_layer(UIList):
    def draw_item(self, _context, layout, _data, item, icon, _active_data, _active_propname, _index):
        # assert(isinstance(item, bpy.types.GPencilLayer)
        gpl = item

        if self.layout_type in {'DEFAULT', 'COMPACT'}:
            if gpl.lock:
                layout.active = False

            split = layout.split(factor=0.2)
            split.prop(gpl, "color", text="", emboss=True)
            split.prop(gpl, "info", text="", emboss=False)

            row = layout.row(align=True)
            row.prop(gpl, "annotation_hide", text="", emboss=False)
        elif self.layout_type == 'GRID':
            layout.alignment = 'CENTER'
            layout.label(text="", icon_value=icon)


class AnnotationDataPanel:
    bl_label = "Annotations"
    bl_region_type = 'UI'
    bl_options = {'DEFAULT_CLOSED'}

    def draw_header(self, context):
        if context.space_data.type not in {'VIEW_3D', 'TOPBAR'}:
            self.layout.prop(context.space_data, "show_annotation", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_decorate = False

        is_clip_editor = context.space_data.type == 'CLIP_EDITOR'

        # Grease Pencil owner.
        gpd_owner = context.annotation_data_owner
        gpd = context.annotation_data

        # Owner selector.
        if is_clip_editor:
            col = layout.column()
            col.label(text="Data Source:")
            row = col.row()
            row.prop(context.space_data, "annotation_source", expand=True)

        # Only allow adding annotation ID if its owner exist
        if context.annotation_data_owner is None:
            row = layout.row()
            row.active = False
            row.label(text="No annotation source")
            return

        row = layout.row()
        row.template_ID(gpd_owner, "grease_pencil", new="gpencil.annotation_add", unlink="gpencil.data_unlink")

        # List of layers/notes.
        if gpd and gpd.layers:
            self.draw_layers(context, layout, gpd)

    def draw_layers(self, context, layout, gpd):
        row = layout.row()

        col = row.column()
        if len(gpd.layers) >= 2:
            layer_rows = 5
        else:
            layer_rows = 3
        col.template_list("GPENCIL_UL_annotation_layer", "", gpd, "layers", gpd.layers, "active_index",
                          rows=layer_rows, sort_reverse=True, sort_lock=True)

        col = row.column()

        sub = col.column(align=True)
        sub.operator("gpencil.layer_annotation_add", icon='ADD', text="")
        sub.operator("gpencil.layer_annotation_remove", icon='REMOVE', text="")

        gpl = context.active_annotation_layer
        if gpl:
            if len(gpd.layers) > 1:
                col.separator()

                sub = col.column(align=True)
                sub.operator("gpencil.layer_annotation_move", icon='TRIA_UP', text="").type = 'UP'
                sub.operator("gpencil.layer_annotation_move", icon='TRIA_DOWN', text="").type = 'DOWN'

        tool_settings = context.tool_settings
        if gpd and gpl:
            layout.prop(gpl, "opacity", text="Opacity", slider=True)
            layout.prop(gpl, "thickness")
        else:
            layout.prop(tool_settings, "annotation_thickness", text="Thickness")

        if gpl:
            # Full-Row - Frame Locking (and Delete Frame)
            row = layout.row(align=True)
            row.active = not gpl.lock

            if gpl.active_frame:
                lock_status = iface_("Locked") if gpl.lock_frame else iface_("Unlocked")
                lock_label = iface_("Frame: %d (%s)") % (gpl.active_frame.frame_number, lock_status)
            else:
                lock_label = iface_("Lock Frame")
            row.prop(gpl, "lock_frame", text=lock_label, icon='UNLOCKED')
            row.operator("gpencil.annotation_active_frame_delete", text="", icon='X')


class AnnotationOnionSkin:
    bl_label = "Onion Skin"
    bl_region_type = 'UI'
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        # Show this panel as long as someone that might own this exists
        # AND the owner isn't an object (e.g. GP Object)
        if context.annotation_data_owner is None:
            return False
        elif type(context.annotation_data_owner) is bpy.types.Object:
            return False
        else:
            gpl = context.active_annotation_layer
            if gpl is None:
                return False

        return True

    def draw_header(self, context):
        gpl = context.active_annotation_layer
        self.layout.prop(gpl, "use_annotation_onion_skinning", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_decorate = False

        gpl = context.active_annotation_layer
        col = layout.column()
        split = col.split(factor=0.5)
        split.active = gpl.use_annotation_onion_skinning

        # - Before Frames
        sub = split.column(align=True)
        row = sub.row(align=True)
        row.prop(gpl, "annotation_onion_before_color", text="")
        sub.prop(gpl, "annotation_onion_before_range", text="Before")

        # - After Frames
        sub = split.column(align=True)
        row = sub.row(align=True)
        row.prop(gpl, "annotation_onion_after_color", text="")
        sub.prop(gpl, "annotation_onion_after_range", text="After")


class GreasePencilMaterialsPanel:
    # Mix-in, use for properties editor and top-bar.
    def draw(self, context):
        layout = self.layout
        show_full_ui = (self.bl_space_type == 'PROPERTIES')

        is_view3d = (self.bl_space_type == 'VIEW_3D')
        tool_settings = context.scene.tool_settings
        gpencil_paint = tool_settings.gpencil_paint
        brush = gpencil_paint.brush

        ob = context.object
        row = layout.row()

        if ob:
            is_sortable = len(ob.material_slots) > 1
            rows = 7

            row.template_list("GPENCIL_UL_matslots", "", ob, "material_slots", ob, "active_material_index", rows=rows)

            # if topbar popover and brush pinned, disable
            if is_view3d and brush is not None:
                gp_settings = brush.gpencil_settings
                if gp_settings.use_material_pin:
                    row.enabled = False

            col = row.column(align=True)
            if show_full_ui:
                col.operator("object.material_slot_add", icon='ADD', text="")
                col.operator("object.material_slot_remove", icon='REMOVE', text="")

            col.separator()

            col.menu("GPENCIL_MT_material_context_menu", icon='DOWNARROW_HLT', text="")

            if is_sortable:
                col.separator()

                col.operator("object.material_slot_move", icon='TRIA_UP', text="").direction = 'UP'
                col.operator("object.material_slot_move", icon='TRIA_DOWN', text="").direction = 'DOWN'

                col.separator()

                sub = col.column(align=True)
                sub.operator("gpencil.material_isolate", icon='RESTRICT_VIEW_ON', text="").affect_visibility = True
                sub.operator("gpencil.material_isolate", icon='LOCKED', text="").affect_visibility = False

            if show_full_ui:
                row = layout.row()

                row.template_ID(ob, "active_material", new="material.new", live_icon=True)

                slot = context.material_slot
                if slot:
                    icon_link = 'MESH_DATA' if slot.link == 'DATA' else 'OBJECT_DATA'
                    row.prop(slot, "link", icon=icon_link, icon_only=True)

                if ob.data.use_stroke_edit_mode:
                    row = layout.row(align=True)
                    row.operator("gpencil.stroke_change_color", text="Assign")
                    row.operator("gpencil.material_select", text="Select").deselect = False
                    row.operator("gpencil.material_select", text="Deselect").deselect = True
        # stroke color
            ma = None
            if is_view3d and brush is not None:
                gp_settings = brush.gpencil_settings
                if gp_settings.use_material_pin is False:
                    if len(ob.material_slots) > 0 and ob.active_material_index >= 0:
                        ma = ob.material_slots[ob.active_material_index].material
                else:
                    ma = gp_settings.material

            if ma is not None and ma.grease_pencil is not None:
                gpcolor = ma.grease_pencil
                if gpcolor.stroke_style == 'SOLID':
                    row = layout.row()
                    row.prop(gpcolor, "color", text="Stroke Color")

        else:
            space = context.space_data
            row.template_ID(space, "pin_id")


class GreasePencilVertexcolorPanel:

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        ts = context.scene.tool_settings
        is_vertex = context.mode == 'VERTEX_GPENCIL'
        gpencil_paint = ts.gpencil_vertex_paint if is_vertex else ts.gpencil_paint
        brush = gpencil_paint.brush
        gp_settings = brush.gpencil_settings
        tool = brush.gpencil_vertex_tool if is_vertex else brush.gpencil_tool

        ob = context.object

        if ob:
            col = layout.column()
            col.template_color_picker(brush, "color", value_slider=True)

            sub_row = layout.row(align=True)
            sub_row.prop(brush, "color", text="")
            sub_row.prop(brush, "secondary_color", text="")

            sub_row.operator("gpencil.tint_flip", icon='FILE_REFRESH', text="")

            row = layout.row(align=True)
            row.template_ID(gpencil_paint, "palette", new="palette.new")
            if gpencil_paint.palette:
                layout.template_palette(gpencil_paint, "palette", color=True)

            if tool in {'DRAW', 'FILL'} and is_vertex is False:
                row = layout.row(align=True)
                row.prop(gp_settings, "vertex_mode", text="Mode")
                row = layout.row(align=True)
                row.prop(gp_settings, "vertex_color_factor", slider=True, text="Mix Factor")


class GPENCIL_UL_layer(UIList):
    def draw_item(self, _context, layout, _data, item, icon, _active_data, _active_propname, _index):
        # assert(isinstance(item, bpy.types.GPencilLayer)
        gpl = item

        if self.layout_type in {'DEFAULT', 'COMPACT'}:
            if gpl.lock:
                layout.active = False

            row = layout.row(align=True)
            row.label(
                text="",
                icon='BONE_DATA' if gpl.is_parented else 'BLANK1',
            )
            row.prop(gpl, "info", text="", emboss=False)

            row = layout.row(align=True)

            icon_mask = 'MOD_MASK' if gpl.use_mask_layer else 'LAYER_ACTIVE'

            row.prop(gpl, "use_mask_layer", text="", icon=icon_mask, emboss=False)

            subrow = row.row(align=True)
            subrow.prop(
                gpl,
                "use_onion_skinning",
                text="",
                icon='ONIONSKIN_ON' if gpl.use_onion_skinning else 'ONIONSKIN_OFF',
                emboss=False,
            )
            row.prop(gpl, "hide", text="", emboss=False)
            row.prop(gpl, "lock", text="", emboss=False)
        elif self.layout_type == 'GRID':
            layout.alignment = 'CENTER'
            layout.label(
                text="",
                icon_value=icon,
            )


class GreasePencilSimplifyPanel:

    def draw_header(self, context):
        rd = context.scene.render
        self.layout.prop(rd, "simplify_gpencil", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        rd = context.scene.render

        layout.active = rd.simplify_gpencil

        col = layout.column()
        col.prop(rd, "simplify_gpencil_onplay")
        col.prop(rd, "simplify_gpencil_view_fill")
        col.prop(rd, "simplify_gpencil_modifier")
        col.prop(rd, "simplify_gpencil_shader_fx")
        col.prop(rd, "simplify_gpencil_tint")
        col.prop(rd, "simplify_gpencil_antialiasing")


class GreasePencilLayerTransformPanel:

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        ob = context.object
        gpd = ob.data
        gpl = gpd.layers.active
        layout.active = not gpl.lock

        row = layout.row(align=True)
        row.prop(gpl, "location")

        row = layout.row(align=True)
        row.prop(gpl, "rotation")

        row = layout.row(align=True)
        row.prop(gpl, "scale")


class GreasePencilLayerAdjustmentsPanel:

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        ob = context.object
        gpd = ob.data
        gpl = gpd.layers.active
        layout.active = not gpl.lock

        # Layer options
        # Offsets - Color Tint
        layout.enabled = not gpl.lock
        col = layout.column(align=True)
        col.prop(gpl, "tint_color")
        col.prop(gpl, "tint_factor", text="Factor", slider=True)

        # Offsets - Thickness
        col = layout.row(align=True)
        col.prop(gpl, "line_change", text="Stroke Thickness")


class GPENCIL_UL_masks(UIList):
    def draw_item(self, _context, layout, _data, item, icon, _active_data, _active_propname, _index):
        mask = item
        if self.layout_type in {'DEFAULT', 'COMPACT'}:
            row = layout.row(align=True)
            row.prop(mask, "name", text="", emboss=False, icon_value=icon)
            row.prop(mask, "invert", text="", emboss=False)
            row.prop(mask, "hide", text="", emboss=False)
        elif self.layout_type == 'GRID':
            layout.alignment = 'CENTER'
            layout.prop(mask, "name", text="", emboss=False, icon_value=icon)


class GPENCIL_MT_layer_mask_menu(Menu):
    bl_label = "Layer Specials"

    def draw(self, context):
        layout = self.layout
        ob = context.object
        gpd = ob.data
        gpl_active = gpd.layers.active
        done = False
        for gpl in gpd.layers:
            if gpl != gpl_active and gpl.info not in gpl_active.mask_layers:
                done = True
                layout.operator("gpencil.layer_mask_add", text=gpl.info).name = gpl.info

        if done is False:
            layout.label(text="No layers to add")


class GreasePencilLayerMasksPanel:
    def draw_header(self, context):
        ob = context.active_object
        gpd = ob.data
        gpl = gpd.layers.active

        self.layout.prop(gpl, "use_mask_layer", text="")

    def draw(self, context):
        ob = context.active_object
        gpd = ob.data
        gpl = gpd.layers.active

        layout = self.layout
        layout.enabled = gpl.use_mask_layer

        if gpl:
            rows = 4
            row = layout.row()
            col = row.column()
            col.template_list("GPENCIL_UL_masks", "", gpl, "mask_layers", gpl.mask_layers,
                              "active_mask_index", rows=rows, sort_lock=True)

            col2 = row.column(align=True)
            col2.menu("GPENCIL_MT_layer_mask_menu", icon='ADD', text="")
            col2.operator("gpencil.layer_mask_remove", icon='REMOVE', text="")


class GreasePencilLayerRelationsPanel:

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        scene = context.scene
        ob = context.object
        gpd = ob.data
        gpl = gpd.layers.active

        col = layout.column()
        col.active = not gpl.lock
        col.prop(gpl, "parent")
        col.prop(gpl, "parent_type", text="Type")
        parent = gpl.parent

        if parent and gpl.parent_type == 'BONE' and parent.type == 'ARMATURE':
            col.prop_search(gpl, "parent_bone", parent.data, "bones", text="Bone")

        layout.separator()

        col = layout.row(align=True)
        col.prop(gpl, "pass_index")

        col = layout.row(align=True)
        col.prop_search(gpl, "viewlayer_render", scene, "view_layers", text="View Layer")


class GreasePencilLayerDisplayPanel:

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        ob = context.object
        gpd = ob.data
        gpl = gpd.layers.active

        use_colors = context.preferences.edit.use_anim_channel_group_colors

        col = layout.column(align=True)
        col.active = use_colors
        row = col.row(align=True)
        row.prop(gpl, "channel_color")
        if not use_colors:
            col.label(text="Channel Colors are disabled in Animation preferences")

        row = layout.row(align=True)
        row.prop(gpl, "use_solo_mode", text="Show Only on Keyframed")


class GreasePencilFlipTintColors(Operator):
    bl_label = "Flip Colors"
    bl_idname = "gpencil.tint_flip"
    bl_description = "Switch tint colors"

    def execute(self, context):
        try:
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

            brush = settings.brush
            if brush is not None:
                color = brush.color
                secondary_color = brush.secondary_color

                orig_prim = color.hsv
                orig_sec = secondary_color.hsv

                color.hsv = orig_sec
                secondary_color.hsv = orig_prim

            return {'FINISHED'}

        except Exception as e:
            utils_core.error_handlers(self, "gpencil.tint_flip", e,
                                      "Flip Colors could not be completed")

            return {'CANCELLED'}


classes = (
    GPENCIL_MT_snap,
    GPENCIL_MT_snap_pie,
    GPENCIL_MT_cleanup,
    GPENCIL_MT_move_to_layer,
    GPENCIL_MT_layer_active,
    GPENCIL_MT_material_active,

    GPENCIL_MT_gpencil_draw_delete,
    GPENCIL_MT_layer_mask_menu,

    GPENCIL_UL_annotation_layer,
    GPENCIL_UL_layer,
    GPENCIL_UL_masks,

    GreasePencilFlipTintColors,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
