# SPDX-FileCopyrightText: 2009-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.types import (
    Menu,
    UIList,
)
from bpy.app.translations import (
    contexts as i18n_contexts,
    pgettext_iface as iface_,
)


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
        row.prop_enum(tool_settings, "annotation_stroke_placement_view2d", 'IMAGE', text="Image")


class GreasePencilSculptAdvancedPanel:

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        tool_settings = context.scene.tool_settings
        brush = tool_settings.gpencil_sculpt_paint.brush
        tool = brush.gpencil_sculpt_brush_type
        gp_settings = brush.gpencil_settings

        if tool in {'SMOOTH', 'RANDOMIZE'}:
            col = layout.column(heading="Affect", align=True)
            col.prop(gp_settings, "use_edit_position", text="Position")
            col.prop(gp_settings, "use_edit_strength", text="Strength", text_ctxt=i18n_contexts.id_gpencil)
            col.prop(gp_settings, "use_edit_thickness", text="Thickness")
            col.prop(gp_settings, "use_edit_uv", text="UV")


# GP Object Tool Settings
class GreasePencilDisplayPanel:
    bl_label = "Brush Tip"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        ob = context.active_object

        if context.mode == 'SCULPT_GREASE_PENCIL':
            brush = context.tool_settings.gpencil_sculpt_paint.brush
        elif context.mode == 'WEIGHT_GREASE_PENCIL':
            brush = context.tool_settings.gpencil_weight_paint.brush
        else:
            brush = context.tool_settings.gpencil_paint.brush

        if ob and ob.type == 'GREASEPENCIL' and brush:
            return True

        return False

    def draw_header(self, context):
        if self.is_popover:
            return

        tool_settings = context.tool_settings
        if context.mode == 'PAINT_GREASE_PENCIL':
            settings = tool_settings.gpencil_paint
        elif context.mode == 'SCULPT_GREASE_PENCIL':
            settings = tool_settings.gpencil_sculpt_paint
        elif context.mode == 'WEIGHT_GREASE_PENCIL':
            settings = tool_settings.gpencil_weight_paint
        elif context.mode == 'VERTEX_GREASE_PENCIL':
            settings = tool_settings.gpencil_vertex_paint
        brush = settings.brush
        if brush:
            self.layout.prop(settings, "show_brush", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        tool_settings = context.tool_settings
        if context.mode == 'PAINT_GREASE_PENCIL':
            settings = tool_settings.gpencil_paint
        elif context.mode == 'SCULPT_GREASE_PENCIL':
            settings = tool_settings.gpencil_sculpt_paint
        elif context.mode == 'WEIGHT_GREASE_PENCIL':
            settings = tool_settings.gpencil_weight_paint
        elif context.mode == 'VERTEX_GREASE_PENCIL':
            settings = tool_settings.gpencil_vertex_paint
        brush = settings.brush
        gp_settings = brush.gpencil_settings
        ob = context.active_object

        if self.is_popover and ob.mode not in {'PAINT_GREASE_PENCIL', 'VERTEX_GREASE_PENCIL'}:
            row = layout.row(align=True)
            row.use_property_split = False
            row.prop(settings, "show_brush", text="Display Cursor")

        if ob.mode == 'PAINT_GREASE_PENCIL':
            if self.is_popover:
                row = layout.row(align=True)
                row.prop(settings, "show_brush", text="Display Cursor")

            if brush.gpencil_brush_type == 'DRAW':
                row = layout.row(align=True)
                row.active = settings.show_brush
                row.prop(gp_settings, "show_lasso", text="Show Fill Color While Drawing")

        elif ob.mode == 'SCULPT_GREASE_PENCIL':
            col = layout.column(align=True)
            col.active = settings.show_brush

            col.prop(brush, "cursor_color_add", text="Cursor Color")
            if brush.gpencil_sculpt_brush_type in {'THICKNESS', 'STRENGTH', 'PINCH', 'TWIST'}:
                col.prop(brush, "cursor_color_subtract", text="Inverse Color")

        elif ob.mode == 'WEIGHT_GREASE_PENCIL':
            col = layout.column(align=True)
            col.active = settings.show_brush

            col.prop(brush, "cursor_color_add", text="Cursor Color")

        elif ob.mode == 'VERTEX_GREASE_PENCIL':
            row = layout.row(align=True)
            row.prop(settings, "show_brush", text="Display Cursor")


class GreasePencilBrushFalloff:
    bl_label = "Falloff"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        tool_settings = context.tool_settings
        settings = None
        if context.mode == 'PAINT_GREASE_PENCIL':
            settings = tool_settings.gpencil_paint
        if context.mode == 'SCULPT_GREASE_PENCIL':
            settings = tool_settings.gpencil_sculpt_paint
        elif context.mode == 'WEIGHT_GREASE_PENCIL':
            settings = tool_settings.gpencil_weight_paint
        elif context.mode == 'VERTEX_GREASE_PENCIL':
            settings = tool_settings.gpencil_vertex_paint

        if settings:
            brush = settings.brush

            col = layout.column(align=True)
            if context.region.type == 'TOOL_HEADER':
                col.prop(brush, "curve_distance_falloff_preset", expand=True)
            else:
                col.prop(brush, "curve_distance_falloff_preset", text="")

            if brush.curve_distance_falloff_preset == 'CUSTOM':
                layout.template_curve_mapping(
                    brush, "curve_distance_falloff",
                    brush=True,
                    use_negative_slope=True,
                    show_presets=True,
                )


class GREASE_PENCIL_MT_move_to_layer(Menu):
    bl_label = "Move to Layer"

    def draw(self, context):
        layout = self.layout
        layout.operator_context = 'INVOKE_REGION_WIN'
        grease_pencil = context.active_object.data

        layout.operator("grease_pencil.move_to_layer", text="New Layer", icon='ADD').add_new_layer = True

        if not grease_pencil.layers:
            return

        layout.separator()

        for i in range(len(grease_pencil.layers) - 1, -1, -1):
            layer = grease_pencil.layers[i]
            if layer == grease_pencil.layers.active:
                icon = 'GREASEPENCIL'
            else:
                icon = 'NONE'
            layout.operator("grease_pencil.move_to_layer", text=layer.name, icon=icon).target_layer_name = layer.name


class GREASE_PENCIL_MT_layer_active(Menu):
    bl_label = "Change Active Layer"

    def draw(self, context):
        layout = self.layout
        layout.operator_context = 'INVOKE_REGION_WIN'
        obd = context.active_object.data

        nlop = layout.operator("grease_pencil.layer_add", text="New Layer", icon='ADD')
        nlop.new_layer_name = "Layer"

        if not obd.layers:
            return

        layout.separator()

        for i in range(len(obd.layers) - 1, -1, -1):
            layer = obd.layers[i]
            if layer == obd.layers.active:
                icon = 'GREASEPENCIL'
            else:
                icon = 'NONE'
            layout.operator("grease_pencil.layer_active", text=layer.name, icon=icon).layer = i


class GPENCIL_UL_annotation_layer(UIList):
    def draw_item(self, _context, layout, _data, item, _icon, _active_data, _active_propname, _index):
        # assert(isinstance(item, bpy.types.GPencilLayer)
        gpl = item
        if gpl.lock:
            layout.active = False

        split = layout.split(factor=0.2)
        split.prop(gpl, "color", text="", emboss=True)
        split.prop(gpl, "info", text="", emboss=False)

        row = layout.row(align=True)

        row.prop(gpl, "show_in_front", text="", icon='XRAY' if gpl.show_in_front else 'FACESEL', emboss=False)

        row.prop(gpl, "annotation_hide", text="", emboss=False)


class AnnotationDataPanel:
    bl_label = "Annotations"
    bl_region_type = 'UI'
    bl_options = {'DEFAULT_CLOSED'}

    def draw_header(self, context):
        space = context.space_data
        if space.type not in {
            'VIEW_3D',
            'TOPBAR',
            'SEQUENCE_EDITOR',
            'IMAGE_EDITOR',
            'NODE_EDITOR',
            'PROPERTIES',
        }:
            self.layout.prop(space, "show_annotation", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_decorate = False
        space = context.space_data

        is_clip_editor = space.type == 'CLIP_EDITOR'

        # Grease Pencil owner.
        gpd_owner = context.annotation_data_owner
        gpd = context.annotation_data

        # Owner selector.
        if is_clip_editor:
            col = layout.column()
            col.label(text="Data Source:")
            row = col.row()
            row.prop(space, "annotation_source", expand=True)

        # Only allow adding annotation ID if its owner exist
        if context.annotation_data_owner is None:
            row = layout.row()
            row.active = False
            row.label(text="No annotation source")
            return

        row = layout.row()
        row.template_ID(gpd_owner, "annotation", new="gpencil.annotation_add", unlink="gpencil.data_unlink")

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
        col.template_list(
            "GPENCIL_UL_annotation_layer", "", gpd, "layers", gpd.layers, "active_index",
            rows=layer_rows, sort_reverse=True, sort_lock=True,
        )

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
            layout.prop(gpl, "annotation_opacity", text="Opacity", slider=True)
            layout.prop(gpl, "thickness")
        else:
            layout.prop(tool_settings, "annotation_thickness", text="Thickness")

        if gpl:
            # Full-Row - Frame Locking (and Delete Frame)
            row = layout.row(align=True)
            row.active = not gpl.lock

            if gpl.active_frame:
                lock_status = iface_("Locked") if gpl.lock_frame else iface_("Unlocked")
                lock_label = iface_("Frame: {:d} ({:s})").format(gpl.active_frame.frame_number, lock_status)
            else:
                lock_label = iface_("Lock Frame")
            row.prop(gpl, "lock_frame", text=lock_label, icon='UNLOCKED', translate=False)
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
        col.prop(gpl, "annotation_onion_use_custom_color")
        split = col.split(factor=0.5)
        split.active = gpl.use_annotation_onion_skinning

        # - Before Frames
        sub = split.column(align=True)
        row = sub.row(align=True)
        if gpl.annotation_onion_use_custom_color:
            row.prop(gpl, "annotation_onion_before_color", text="")
        sub.prop(gpl, "annotation_onion_before_range", text="Before")

        # - After Frames
        sub = split.column(align=True)
        row = sub.row(align=True)
        if gpl.annotation_onion_use_custom_color:
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
        brush = gpencil_paint.brush if gpencil_paint else None

        ob = context.object
        row = layout.row()

        if ob:
            is_sortable = len(ob.material_slots) > 1
            rows = 7

            row.template_list("GPENCIL_UL_matslots", "", ob, "material_slots", ob, "active_material_index", rows=rows)

            # if top-bar popover and brush pinned, disable.
            if is_view3d and brush is not None:
                gp_settings = brush.gpencil_settings
                if gp_settings.use_material_pin:
                    row.enabled = False

            col = row.column(align=True)
            if show_full_ui:
                col.operator("object.material_slot_add", icon='ADD', text="")
                col.operator("object.material_slot_remove", icon='REMOVE', text="")

            col.separator()

            col.menu("GREASE_PENCIL_MT_material_context_menu", icon='DOWNARROW_HLT', text="")

            if is_sortable:
                col.separator()

                col.operator("object.material_slot_move", icon='TRIA_UP', text="").direction = 'UP'
                col.operator("object.material_slot_move", icon='TRIA_DOWN', text="").direction = 'DOWN'

                col.separator()

                sub = col.column(align=True)
                sub.operator(
                    "grease_pencil.material_isolate",
                    icon='RESTRICT_VIEW_ON',
                    text="",
                ).affect_visibility = True
                sub.operator("grease_pencil.material_isolate", icon='LOCKED', text="").affect_visibility = False

            if show_full_ui:
                row = layout.row()

                row.template_ID(ob, "active_material", new="material.new", live_icon=True)

                slot = context.material_slot
                if slot:
                    icon_link = 'MESH_DATA' if slot.link == 'DATA' else 'OBJECT_DATA'
                    row.prop(slot, "link", icon=icon_link, icon_only=True)

                if ob.mode == 'EDIT':
                    row = layout.row(align=True)
                    row.operator("grease_pencil.stroke_material_set", text="Assign")
                    row.operator("grease_pencil.material_select", text="Select").deselect = False
                    row.operator("grease_pencil.material_select", text="Deselect").deselect = True
        # stroke color
            ma = None
            if is_view3d and brush is not None:
                gp_settings = brush.gpencil_settings
                if gp_settings.use_material_pin is False:
                    if len(ob.material_slots) > 0 and ob.active_material_index >= 0:
                        ma = ob.material_slots[ob.active_material_index].material
                else:
                    ma = gp_settings.material
            else:
                if len(ob.material_slots) > 0 and ob.active_material_index >= 0:
                    ma = ob.material_slots[ob.active_material_index].material

            if is_view3d and ma is not None and ma.grease_pencil is not None:
                gpcolor = ma.grease_pencil
                col = layout.column(align=True)
                if gpcolor.show_stroke and gpcolor.stroke_style == 'SOLID':
                    col.prop(gpcolor, "color", text="Stroke Color")
                if gpcolor.show_fill and gpcolor.fill_style == 'SOLID':
                    col.prop(gpcolor, "fill_color", text="Fill Color")

        else:
            space = context.space_data
            row.template_ID(space, "pin_id")


class GreasePencilSimplifyPanel:

    def draw_header(self, context):
        rd = context.scene.render
        self.layout.prop(rd, "simplify_gpencil", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        rd = context.scene.render

        layout.active = rd.use_simplify and rd.simplify_gpencil

        col = layout.column()
        col.prop(rd, "simplify_gpencil_onplay")
        col.prop(rd, "simplify_gpencil_view_fill")
        col.prop(rd, "simplify_gpencil_modifier")
        col.prop(rd, "simplify_gpencil_shader_fx")
        col.prop(rd, "simplify_gpencil_tint")
        col.prop(rd, "simplify_gpencil_antialiasing")


class GREASE_PENCIL_MT_snap(Menu):
    bl_label = "Snap"

    def draw(self, _context):
        layout = self.layout

        layout.operator("grease_pencil.snap_to_grid", text="Selection to Grid")
        layout.operator("grease_pencil.snap_to_cursor", text="Selection to Cursor").use_offset = False
        layout.operator("grease_pencil.snap_to_cursor", text="Selection to Cursor (Keep Offset)").use_offset = True

        layout.separator()

        layout.operator("grease_pencil.snap_cursor_to_selected", text="Cursor to Selected")
        layout.operator("view3d.snap_cursor_to_center", text="Cursor to World Origin")
        layout.operator("view3d.snap_cursor_to_grid", text="Cursor to Grid")


class GREASE_PENCIL_MT_snap_pie(Menu):
    bl_label = "Snap"

    def draw(self, _context):
        layout = self.layout
        pie = layout.menu_pie()

        pie.operator("view3d.snap_cursor_to_grid", text="Cursor to Grid", icon='CURSOR')
        pie.operator("grease_pencil.snap_to_grid", text="Selection to Grid", icon='RESTRICT_SELECT_OFF')
        pie.operator("grease_pencil.snap_cursor_to_selected", text="Cursor to Selected", icon='CURSOR')
        pie.operator(
            "grease_pencil.snap_to_cursor",
            text="Selection to Cursor",
            icon='RESTRICT_SELECT_OFF',
        ).use_offset = False
        pie.operator(
            "grease_pencil.snap_to_cursor",
            text="Selection to Cursor (Keep Offset)",
            icon='RESTRICT_SELECT_OFF',
        ).use_offset = True
        pie.separator()
        pie.operator("view3d.snap_cursor_to_center", text="Cursor to World Origin", icon='CURSOR')
        pie.separator()


class GREASE_PENCIL_MT_draw_delete(Menu):
    bl_label = "Delete"

    def draw(self, _context):
        layout = self.layout
        layout.operator_context = 'INVOKE_REGION_WIN'

        layout.operator(
            "grease_pencil.delete_frame",
            text="Delete Active Keyframe (Active Layer)",
        ).type = 'ACTIVE_FRAME'
        layout.operator(
            "grease_pencil.delete_frame",
            text="Delete Active Keyframes (All Layers)",
        ).type = 'ALL_FRAMES'


class GREASE_PENCIL_MT_stroke_simplify(Menu):
    bl_label = "Simplify Stroke"

    def draw(self, _context):
        layout = self.layout
        layout.operator_enum("grease_pencil.stroke_simplify", "mode")


classes = (
    GPENCIL_UL_annotation_layer,

    GREASE_PENCIL_MT_move_to_layer,
    GREASE_PENCIL_MT_layer_active,

    GREASE_PENCIL_MT_snap,
    GREASE_PENCIL_MT_snap_pie,

    GREASE_PENCIL_MT_draw_delete,

    GREASE_PENCIL_MT_stroke_simplify,

)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
