# SPDX-FileCopyrightText: 2009-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.types import Menu, Panel, UIList, WindowManager
from bpy.app.translations import (
    pgettext_iface as iface_,
    contexts as i18n_contexts,
)

from bl_ui.properties_grease_pencil_common import (
    GreasePencilSculptAdvancedPanel,
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
            layout.label(text="No brush selected", icon='INFO')
            return

        if brush.library and brush.library.is_editable:
            layout.operator("brush.asset_save_as", text="Duplicate Asset...", icon='DUPLICATE')
            layout.operator("brush.asset_delete", text="Delete Asset")
        else:
            layout.operator("brush.asset_save_as", text="Save As Asset...", icon='FILE_TICK')
            layout.operator("brush.asset_delete", text="Delete")

        layout.separator()

        layout.operator("brush.asset_edit_metadata", text="Edit Metadata...")
        layout.operator("brush.asset_load_preview", text="Edit Preview Image...")
        layout.operator("brush.asset_save", text="Save Changes to Asset")
        layout.operator("brush.asset_revert", text="Revert to Asset")


class View3DPanel:
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'


# **************** standard tool clusters ******************

# Used by vertex & weight paint
def draw_vpaint_symmetry(layout, obj):
    mesh = obj.data

    col = layout.column()
    row = col.row(heading="Mirror", align=True)
    row.prop(obj, "use_mesh_mirror_x", text="X", toggle=True)
    row.prop(obj, "use_mesh_mirror_y", text="Y", toggle=True)
    row.prop(obj, "use_mesh_mirror_z", text="Z", toggle=True)

    col = layout.column()
    col.active = not mesh.use_mirror_vertex_groups
    col.prop(mesh, "radial_symmetry", text="Radial")


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

    def draw(self, _context):
        # layout = self.layout
        pass


class VIEW3D_PT_tools_meshedit_options_transform(View3DPanel, Panel):
    bl_category = "Tool"
    bl_context = ".mesh_edit"  # dot on purpose (access from topbar)
    bl_label = "Transform"
    bl_parent_id = "VIEW3D_PT_tools_meshedit_options"

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

        col = layout.column(align=True)
        col.prop(tool_settings, "use_transform_correct_face_attributes")
        sub = col.column(align=True)
        sub.active = tool_settings.use_transform_correct_face_attributes
        sub.prop(tool_settings, "use_transform_correct_keep_connected")
        col.separator()

        col = layout.column(heading="Mirror")
        sub = col.row(align=True)
        sub.prop(mesh, "use_mirror_x", text="X", toggle=True)
        sub.prop(mesh, "use_mirror_y", text="Y", toggle=True)
        sub.prop(mesh, "use_mirror_z", text="Z", toggle=True)

        col = layout.column(align=True)
        col.active = mesh.use_mirror_x or mesh.use_mirror_y or mesh.use_mirror_z
        col.prop(mesh, "use_mirror_topology")
        col.separator()

        col = layout.column(align=True)
        col.prop(tool_settings, "use_mesh_automerge", text="Auto Merge", toggle=False)
        sub = col.column(align=True)
        sub.active = tool_settings.use_mesh_automerge
        sub.prop(tool_settings, "use_mesh_automerge_and_split", toggle=False)
        sub.prop(tool_settings, "double_threshold", text="Threshold")


class VIEW3D_PT_tools_meshedit_options_uvs(View3DPanel, Panel):
    bl_category = "Tool"
    bl_context = ".mesh_edit"  # dot on purpose (access from topbar)
    bl_label = "UVs"
    bl_parent_id = "VIEW3D_PT_tools_meshedit_options"

    def draw(self, context):
        layout = self.layout

        layout.use_property_decorate = False
        layout.use_property_split = True

        tool_settings = context.tool_settings

        layout.prop(tool_settings, "use_edge_path_live_unwrap")


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
    def draw_item(self, _context, layout, _data, item, _icon, _active_data, _active_propname, _index):
        # mat = data

        # Hint that painting on linked images is prohibited
        ima = _data.texture_paint_images.get(item.name)
        if ima is not None and not ima.is_editable:
            layout.enabled = False

        layout.label(text=item.name, icon_value=item.icon_value)


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

        if not tool_context.use_brushes:
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


class VIEW3D_PT_tools_brush_select(Panel, View3DPaintBrushPanel, BrushSelectPanel):
    bl_context = ".paint_common"
    bl_label = "Brush Asset"


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

    @classmethod
    def poll(cls, context):
        mode = cls.get_brush_mode(context)
        if mode == 'SCULPT_GREASE_PENCIL':
            settings = cls.paint_settings(context)
            tool = settings.brush.gpencil_sculpt_brush_type
            return tool in {'SMOOTH', 'RANDOMIZE'}

        return mode is not None and mode != 'SCULPT_CURVES'

    def draw(self, context):
        layout = self.layout

        layout.use_property_split = True
        layout.use_property_decorate = False  # No animation.

        settings = UnifiedPaintPanel.paint_settings(context)
        brush = settings.brush

        brush_settings_advanced(layout.column(), context, settings, brush, self.is_popover)


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
        elif context.sculpt_object:
            capabilities = brush.sculpt_capabilities
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


class SelectPaintSlotHelper:
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'

    canvas_source_attr_name = "canvas_source"
    canvas_image_attr_name = "canvas_image"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        settings = context.tool_settings.image_paint
        mode_settings = self.get_mode_settings(context)

        ob = context.active_object

        layout.prop(mode_settings, self.canvas_source_attr_name, text="Mode")
        layout.separator()

        have_image = False

        match getattr(mode_settings, self.canvas_source_attr_name):
            case 'MATERIAL':
                if len(ob.material_slots) > 1:
                    layout.template_list(
                        "MATERIAL_UL_matslots", "layers",
                        ob, "material_slots",
                        ob, "active_material_index", rows=2,
                    )
                mat = ob.active_material
                if mat and mat.texture_paint_images:
                    row = layout.row()
                    row.template_list(
                        "TEXTURE_UL_texpaintslots", "",
                        mat, "texture_paint_slots",
                        mat, "paint_active_slot", rows=2,
                    )

                    if mat.texture_paint_slots:
                        slot = mat.texture_paint_slots[mat.paint_active_slot]
                    else:
                        slot = None

                    have_image = slot is not None
                else:
                    row = layout.row()

                    box = row.box()
                    box.label(text="No Textures")

                sub = row.column(align=True)
                sub.operator_menu_enum("paint.add_texture_paint_slot", "type", icon='ADD', text="")

            case 'IMAGE':
                mesh = ob.data
                uv_text = mesh.uv_layers.active.name if mesh.uv_layers.active else ""
                layout.template_ID(mode_settings, self.canvas_image_attr_name, new="image.new", open="image.open")
                if settings.missing_uvs:
                    layout.operator("paint.add_simple_uvs", icon='ADD', text="Add UVs")
                else:
                    layout.menu("VIEW3D_MT_tools_projectpaint_uvlayer", text=uv_text, translate=False)
                have_image = getattr(settings, self.canvas_image_attr_name) is not None

                self.draw_image_interpolation(layout=layout, mode_settings=mode_settings)

            case 'COLOR_ATTRIBUTE':
                mesh = ob.data

                row = layout.row()
                col = row.column()
                col.template_list(
                    "MESH_UL_color_attributes_selector",
                    "color_attributes",
                    mesh,
                    "color_attributes",
                    mesh.color_attributes,
                    "active_color_index",
                    rows=3,
                )

                col = row.column(align=True)
                col.operator("geometry.color_attribute_add", icon='ADD', text="")
                col.operator("geometry.color_attribute_remove", icon='REMOVE', text="")

        if settings.missing_uvs:
            layout.separator()
            split = layout.split()
            split.label(text="UV Map Needed", icon='INFO')
            split.operator("paint.add_simple_uvs", icon='ADD', text="Add Simple UVs")
        elif have_image:
            layout.separator()
            layout.operator("image.save_all_modified", text="Save All Images", icon='FILE_TICK')


class VIEW3D_PT_slots_projectpaint(SelectPaintSlotHelper, View3DPanel, Panel):
    bl_label = "Texture Slots"

    canvas_source_attr_name = "mode"
    canvas_image_attr_name = "canvas"

    @classmethod
    def poll(cls, context):
        brush = context.tool_settings.image_paint.brush
        return (brush is not None and context.active_object is not None)

    def get_mode_settings(self, context):
        return context.tool_settings.image_paint

    def draw_image_interpolation(self, layout, mode_settings):
        layout.prop(mode_settings, "interpolation", text="")

    def draw_header(self, context):
        tool = context.tool_settings.image_paint
        ob = context.object
        mat = ob.active_material

        label = iface_("Texture Slots")

        if tool.mode == 'MATERIAL':
            if mat and mat.texture_paint_images and mat.texture_paint_slots:
                label = mat.texture_paint_slots[mat.paint_active_slot].name
        elif tool.canvas:
            label = tool.canvas.name

        self.bl_label = label


class VIEW3D_PT_slots_paint_canvas(SelectPaintSlotHelper, View3DPanel, Panel):
    bl_label = "Canvas"

    @classmethod
    def poll(cls, context):
        if not context.preferences.experimental.use_sculpt_texture_paint:
            return False

        from bl_ui.space_toolsystem_common import ToolSelectPanelHelper
        tool = ToolSelectPanelHelper.tool_active_from_context(context)
        if tool is None:
            return False

        is_paint_tool = False
        if tool.use_brushes:
            brush = context.tool_settings.sculpt.brush
            if brush:
                is_paint_tool = brush.sculpt_brush_type in {'PAINT', 'SMEAR'}
        else:
            # TODO: The property use_paint_canvas doesn't work anymore since its associated
            # C++ function 'rna_WorkSpaceTool_use_paint_canvas_get' passes in a nullptr for
            # the bContext. This property should be fixed in the future, but will require
            # some extensive refactoring. For now, use the workaround above.
            is_paint_tool = tool.use_paint_canvas

        return is_paint_tool

    def get_mode_settings(self, context):
        return context.tool_settings.paint_mode

    def draw_image_interpolation(self, **_kwargs):
        pass

    def draw_header(self, context):
        paint = context.tool_settings.paint_mode
        ob = context.object
        me = ob.data
        mat = ob.active_material

        label = iface_("Canvas")

        if paint.canvas_source == 'MATERIAL':
            if mat and mat.texture_paint_images and mat.texture_paint_slots:
                label = mat.texture_paint_slots[mat.paint_active_slot].name
        elif paint.canvas_source == 'COLOR_ATTRIBUTE':
            active_color = me.color_attributes.active_color
            label = (
                active_color.name if active_color else
                iface_("Color Attribute")
            )
        elif paint.canvas_image:
            label = paint.canvas_image.name

        self.bl_label = label


class VIEW3D_PT_slots_color_attributes(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'
    bl_label = "Color Attributes"
    bl_ui_units_x = 12

    def draw_header(self, context):
        me = context.object.data
        active_color = me.color_attributes.active_color
        self.bl_label = (
            active_color.name if active_color else
            iface_("Color Attributes")
        )

    def draw(self, context):
        ob = context.object
        mesh = ob.data

        layout = self.layout
        row = layout.row()

        col = row.column()
        col.template_list(
            "MESH_UL_color_attributes",
            "color_attributes",
            mesh,
            "color_attributes",
            mesh.color_attributes,
            "active_color_index",
            rows=3,
        )

        col = row.column(align=True)
        col.operator("geometry.color_attribute_add", icon='ADD', text="")
        col.operator("geometry.color_attribute_remove", icon='REMOVE', text="")

        col.separator()

        col.menu("MESH_MT_color_attribute_context_menu", icon='DOWNARROW_HLT', text="")


class VIEW3D_PT_slots_vertex_groups(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'
    bl_label = "Vertex Groups"
    bl_ui_units_x = 12

    def draw_header(self, context):
        ob = context.object
        groups = ob.vertex_groups
        self.bl_label = (
            groups.active.name if groups and groups.active else
            iface_("Vertex Groups")
        )

    def draw(self, context):
        layout = self.layout
        row = layout.row()
        col = row.column()

        ob = context.object
        group = ob.vertex_groups.active

        rows = 3
        if group:
            rows = 5

        row = layout.row()
        row.template_list("MESH_UL_vgroups", "", ob, "vertex_groups", ob.vertex_groups, "active_index", rows=rows)

        col = row.column(align=True)

        col.operator("object.vertex_group_add", icon='ADD', text="")
        props = col.operator("object.vertex_group_remove", icon='REMOVE', text="")
        props.all_unlocked = props.all = False

        col.separator()

        col.menu("MESH_MT_vertex_group_context_menu", icon='DOWNARROW_HLT', text="")

        if group:
            col.separator()
            col.operator("object.vertex_group_move", icon='TRIA_UP', text="").direction = 'UP'
            col.operator("object.vertex_group_move", icon='TRIA_DOWN', text="").direction = 'DOWN'


class VIEW3D_PT_mask(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'
    bl_label = "Masking"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        pass


class VIEW3D_PT_stencil_projectpaint(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'
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
        self.layout.prop(ipaint, "use_stencil_layer", text=self.bl_label if self.is_popover else "")

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


class VIEW3D_PT_tools_brush_display(Panel, View3DPaintBrushPanel, DisplayPanel):
    bl_context = ".paint_common"
    bl_parent_id = "VIEW3D_PT_tools_brush_settings"
    bl_label = "Cursor"
    bl_options = {'DEFAULT_CLOSED'}
    bl_ui_units_x = 15


class VIEW3D_PT_tools_brush_texture(Panel, View3DPaintPanel):
    bl_context = ".paint_common"
    bl_parent_id = "VIEW3D_PT_tools_brush_settings"
    bl_label = "Texture"
    bl_options = {'DEFAULT_CLOSED'}
    bl_ui_units_x = 13

    @classmethod
    def poll(cls, context):
        if (
                (settings := cls.paint_settings(context)) and
                (brush := settings.brush)
        ):
            if context.sculpt_object or context.vertex_paint_object:
                return True
            elif context.image_paint_object:
                return (brush.image_brush_type == 'DRAW')
        return False

    def draw(self, context):
        layout = self.layout

        settings = self.paint_settings(context)
        brush = settings.brush
        tex_slot = brush.texture_slot

        col = layout.column()
        col.template_ID_preview(tex_slot, "texture", new="texture.new", rows=3, cols=8)

        brush_texture_settings(col, brush, context.sculpt_object)


class VIEW3D_PT_tools_mask_texture(Panel, View3DPaintPanel, TextureMaskPanel):
    bl_category = "Tool"
    bl_context = ".imagepaint"  # dot on purpose (access from topbar)
    bl_parent_id = "VIEW3D_PT_tools_brush_settings"
    bl_label = "Texture Mask"
    bl_options = {'DEFAULT_CLOSED'}
    bl_ui_units_x = 13

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


class VIEW3D_PT_tools_brush_stroke(Panel, View3DPaintPanel, StrokePanel):
    bl_context = ".paint_common"  # dot on purpose (access from topbar)
    bl_label = "Stroke"
    bl_parent_id = "VIEW3D_PT_tools_brush_settings"
    bl_options = {'DEFAULT_CLOSED'}
    bl_ui_units_x = 14


class VIEW3D_PT_tools_brush_stroke_smooth_stroke(Panel, View3DPaintPanel, SmoothStrokePanel):
    bl_context = ".paint_common"  # dot on purpose (access from topbar)
    bl_label = "Stabilize Stroke"
    bl_parent_id = "VIEW3D_PT_tools_brush_stroke"
    bl_options = {'DEFAULT_CLOSED'}


class VIEW3D_PT_tools_weight_gradient(Panel, View3DPaintPanel):
    # Don't give context on purpose to not show this in the generic header tool-settings
    # this is added only in the gradient tool's ToolDef
    # `bl_context = ".weightpaint"` # dot on purpose (access from top-bar)
    bl_label = "Falloff"
    bl_options = {'DEFAULT_CLOSED'}
    # Also don't draw as an extra panel in the sidebar (already included in the Brush settings).
    bl_space_type = 'TOPBAR'
    bl_region_type = 'HEADER'

    @classmethod
    def poll(cls, context):
        # since we don't give context above, check mode here (to not show in other modes like sculpt).
        if context.mode != 'PAINT_WEIGHT':
            return False
        settings = context.tool_settings.weight_paint
        if settings is None:
            return False
        brush = settings.brush
        return brush is not None

    def draw(self, context):
        layout = self.layout
        settings = context.tool_settings.weight_paint
        brush = settings.brush

        col = layout.column(align=True)
        col.prop(brush, "curve_distance_falloff_preset", expand=True)

        if brush.curve_distance_falloff_preset == 'CUSTOM':
            layout.template_curve_mapping(
                brush, "curve_distance_falloff",
                brush=True,
                use_negative_slope=True,
                show_presets=True,
            )


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

        self.layout.prop(brush, "use_frontface_falloff", text=self.bl_label if self.is_popover else "")

    def draw(self, context):
        settings = self.paint_settings(context)
        brush = settings.brush

        layout = self.layout

        layout.use_property_split = True
        layout.use_property_decorate = False

        row = layout.row()
        row.active = brush.use_frontface_falloff
        row.prop(brush, "falloff_angle", text="Angle")


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

        self.layout.prop(ipaint, "use_normal_falloff", text=self.bl_label if self.is_popover else "")

    def draw(self, context):
        tool_settings = context.tool_settings
        ipaint = tool_settings.image_paint

        layout = self.layout

        layout.use_property_split = True
        layout.use_property_decorate = False

        row = layout.row()
        row.active = ipaint.use_normal_falloff
        row.prop(ipaint, "normal_angle", text="Angle")


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
        sub.active = (brush and brush.sculpt_capabilities.has_dyntopo) or sculpt.detail_type_method == 'MANUAL'
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

        col = layout.column(heading="Preserve", align=True)
        col.prop(mesh, "use_remesh_preserve_volume", text="Volume")
        col.prop(mesh, "use_remesh_preserve_attributes", text="Attributes")

        layout.operator("object.voxel_remesh", text="Remesh")


class VIEW3D_PT_sculpt_options(Panel, View3DPaintPanel):
    bl_context = ".sculpt_mode"  # dot on purpose (access from topbar)
    bl_label = "Options"
    bl_options = {'DEFAULT_CLOSED'}
    bl_ui_units_x = 13

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

        ob = context.object
        mesh = ob.data
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
        layout.prop(mesh, "radial_symmetry", text="Radial")
        layout.prop(sculpt, "tile_offset", text="Tile Offset")

        layout.separator()

        layout.label(text="Symmetrize")
        layout.prop(sculpt, "symmetrize_direction")
        layout.prop(WindowManager.operator_properties_last("sculpt.symmetrize"), "merge_tolerance")
        layout.operator("sculpt.symmetrize")


class VIEW3D_PT_sculpt_symmetry_for_topbar(Panel):
    bl_space_type = 'TOPBAR'
    bl_region_type = 'HEADER'
    bl_label = "Symmetry"
    bl_ui_units_x = 13

    draw = VIEW3D_PT_sculpt_symmetry.draw


class VIEW3D_PT_curves_sculpt_symmetry(Panel, View3DPaintPanel):
    bl_context = ".curves_sculpt"  # dot on purpose (access from topbar)
    bl_label = "Symmetry"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        ob = context.object
        return ob and ob.type == 'CURVES'

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        ob = context.object
        curves = ob.data

        row = layout.row(align=True, heading="Mirror")
        row.prop(curves, "use_mirror_x", text="X", toggle=True)
        row.prop(curves, "use_mirror_y", text="Y", toggle=True)
        row.prop(curves, "use_mirror_z", text="Z", toggle=True)


class VIEW3D_PT_curves_sculpt_symmetry_for_topbar(Panel):
    bl_space_type = 'TOPBAR'
    bl_region_type = 'HEADER'
    bl_label = "Symmetry"

    draw = VIEW3D_PT_curves_sculpt_symmetry.draw


# ********** default tools for weight-paint ****************


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

        ob = context.object
        mesh = ob.data

        layout.prop(mesh, "use_mirror_vertex_groups")

        draw_vpaint_symmetry(layout, ob)

        row = layout.row()
        row.active = mesh.use_mirror_vertex_groups
        row.prop(mesh, "use_mirror_topology")


class VIEW3D_PT_tools_weightpaint_symmetry_for_topbar(Panel):
    bl_space_type = 'TOPBAR'
    bl_region_type = 'HEADER'
    bl_label = "Symmetry"

    draw = VIEW3D_PT_tools_weightpaint_symmetry.draw


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


class VIEW3D_PT_tools_vertexpaint_options(Panel, View3DPaintPanel):
    bl_context = ".vertexpaint"  # dot on purpose (access from topbar)
    bl_label = "Options"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, _context):
        # This is currently unused, since there aren't any Vertex Paint mode specific options.
        return False

    def draw(self, _context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False


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

        ob = context.object

        draw_vpaint_symmetry(layout, ob)


class VIEW3D_PT_tools_vertexpaint_symmetry_for_topbar(Panel):
    bl_space_type = 'TOPBAR'
    bl_region_type = 'HEADER'
    bl_label = "Symmetry"

    draw = VIEW3D_PT_tools_vertexpaint_symmetry.draw


# ********** default tools for texture-paint ****************


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
        ob = context.object
        mesh = ob.data
        row.prop(mesh, "use_mirror_x", text="X", toggle=True)
        row.prop(mesh, "use_mirror_y", text="Y", toggle=True)
        row.prop(mesh, "use_mirror_z", text="Z", toggle=True)


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


class VIEW3D_PT_tools_imagepaint_options_cavity(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'
    bl_label = "Cavity Mask"
    bl_parent_id = "VIEW3D_PT_mask"
    bl_options = {'DEFAULT_CLOSED'}

    def draw_header(self, context):
        tool_settings = context.tool_settings
        ipaint = tool_settings.image_paint

        self.layout.prop(ipaint, "use_cavity", text=self.bl_label if self.is_popover else "")

    def draw(self, context):
        layout = self.layout

        tool_settings = context.tool_settings
        ipaint = tool_settings.image_paint

        col = layout.column()
        col.active = ipaint.use_cavity
        col.template_curve_mapping(ipaint, "cavity_curve", brush=True)


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
                    layout.template_list(
                        "UI_UL_list", "particle_systems", ob, "particle_systems",
                        ob.particle_systems, "active_index", rows=2, maxrows=3,
                    )

                ptcache = ob.particle_systems.active.point_cache
        else:
            for md in ob.modifiers:
                if md.type == pe.type:
                    ptcache = md.point_cache

        if ptcache and len(ptcache.point_caches) > 1:
            layout.template_list(
                "UI_UL_list", "particles_point_caches", ptcache, "point_caches",
                ptcache.point_caches, "active_index", rows=2, maxrows=3,
            )

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

def tool_use_brush(context):
    from bl_ui.space_toolsystem_common import ToolSelectPanelHelper
    tool = ToolSelectPanelHelper.tool_active_from_context(context)
    if tool and tool.use_brushes is False:
        return False

    return True


class VIEW3D_PT_tools_grease_pencil_sculpt_brush_popover(GreasePencilSculptAdvancedPanel, View3DPanel, Panel):
    bl_context = ".grease_pencil_sculpt"
    bl_label = "Brush"
    bl_category = "Tool"

    @classmethod
    def poll(cls, context):
        if context.region.type != 'TOOL_HEADER':
            return False

        brush = context.tool_settings.gpencil_sculpt_paint.brush
        if brush is None:
            return False

        tool = brush.gpencil_sculpt_brush_type
        return tool in {'SMOOTH', 'RANDOMIZE'}


# Grease Pencil weight painting tools
class GreasePencilWeightPanel:
    bl_context = ".greasepencil_weight"
    bl_category = "Tool"


class VIEW3D_PT_tools_grease_pencil_weight_paint_select(View3DPanel, Panel, GreasePencilWeightPanel, BrushSelectPanel):
    bl_label = "Brush Asset"


class VIEW3D_PT_tools_grease_pencil_weight_paint_settings(Panel, View3DPanel, GreasePencilWeightPanel):
    bl_label = "Brush Settings"

    def draw(self, context):
        if self.is_popover:
            return

        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        tool_settings = context.scene.tool_settings
        settings = tool_settings.gpencil_weight_paint
        brush = settings.brush

        # Grease Pencil
        from bl_ui.properties_paint_common import (
            brush_basic_grease_pencil_weight_settings,
        )
        brush_basic_grease_pencil_weight_settings(layout, context, brush)


class VIEW3D_PT_tools_grease_pencil_brush_weight_falloff(GreasePencilBrushFalloff, Panel, View3DPaintPanel):
    bl_context = ".greasepencil_weight"
    bl_parent_id = "VIEW3D_PT_tools_grease_pencil_weight_paint_settings"
    bl_label = "Falloff"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        tool_settings = context.tool_settings
        settings = tool_settings.gpencil_weight_paint
        brush = settings.brush
        return (brush and brush.curve_distance_falloff)


class VIEW3D_PT_tools_grease_pencil_weight_options(Panel, View3DPanel, GreasePencilWeightPanel):
    bl_label = "Options"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False
        tool_settings = context.scene.tool_settings

        col = layout.column()
        col.prop(tool_settings, "use_auto_normalize", text="Auto Normalize")


# Grease Pencil vertex painting tools
class GreasePencilVertexPanel:
    bl_context = ".greasepencil_vertex"
    bl_category = "Tool"


class VIEW3D_PT_tools_grease_pencil_vertex_paint_select(View3DPanel, Panel, GreasePencilVertexPanel, BrushSelectPanel):
    bl_label = "Brush Asset"


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
                brush_basic_grease_pencil_vertex_settings,
            )
            brush_basic_grease_pencil_vertex_settings(layout, context, brush)


class VIEW3D_PT_tools_grease_pencil_brush_vertex_color(View3DPanel, Panel):
    bl_context = ".greasepencil_vertex"
    bl_label = "Color"
    bl_category = "Tool"

    @classmethod
    def poll(cls, context):
        ob = context.object
        tool_settings = context.tool_settings
        settings = tool_settings.gpencil_vertex_paint
        brush = settings.brush

        if ob is None or brush is None:
            return False

        if context.region.type == 'TOOL_HEADER' or brush.gpencil_vertex_brush_type in {'BLUR', 'AVERAGE', 'SMEAR'}:
            return False

        return True

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False
        tool_settings = context.tool_settings
        settings = tool_settings.gpencil_vertex_paint
        brush = settings.brush
        use_unified_paint = (context.object.mode != 'PAINT_GREASE_PENCIL')
        ups = settings.unified_paint_settings
        prop_owner = ups if use_unified_paint and ups.use_unified_color else brush

        col = layout.column()

        col.template_color_picker(prop_owner, "color", value_slider=True)

        sub_row = col.row(align=True)
        if use_unified_paint:
            UnifiedPaintPanel.prop_unified_color(sub_row, context, brush, "color", text="")
            UnifiedPaintPanel.prop_unified_color(sub_row, context, brush, "secondary_color", text="")
        else:
            sub_row.prop(brush, "color", text="")
            sub_row.prop(brush, "secondary_color", text="")

        sub_row.operator("paint.brush_colors_flip", icon='FILE_REFRESH', text="")
        if use_unified_paint:
            sub_row.prop(ups, "use_unified_color", text="", icon='BRUSHES_ALL')


class VIEW3D_PT_tools_grease_pencil_brush_vertex_falloff(GreasePencilBrushFalloff, Panel, View3DPaintPanel):
    bl_context = ".greasepencil_vertex"
    bl_label = "Falloff"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        tool_settings = context.tool_settings
        settings = tool_settings.gpencil_vertex_paint
        return (settings and settings.brush and settings.brush.curve_distance_falloff)


class VIEW3D_PT_tools_grease_pencil_brush_vertex_palette(View3DPanel, Panel):
    bl_context = ".greasepencil_vertex"
    bl_label = "Palette"
    bl_category = "Tool"
    bl_parent_id = "VIEW3D_PT_tools_grease_pencil_brush_vertex_color"

    @classmethod
    def poll(cls, context):
        ob = context.object
        tool_settings = context.tool_settings
        settings = tool_settings.gpencil_vertex_paint
        brush = settings.brush

        if ob is None or brush is None:
            return False

        if brush.gpencil_vertex_brush_type in {'BLUR', 'AVERAGE', 'SMEAR'}:
            return False

        return True

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False
        tool_settings = context.tool_settings
        settings = tool_settings.gpencil_vertex_paint

        col = layout.column()

        row = col.row(align=True)
        row.template_ID(settings, "palette", new="palette.new")
        if settings.palette:
            col.template_palette(settings, "palette", color=True)


class VIEW3D_PT_tools_grease_pencil_paint_appearance(GreasePencilDisplayPanel, Panel, View3DPanel):
    bl_context = ".grease_pencil_paint"
    bl_parent_id = "VIEW3D_PT_tools_grease_pencil_v3_brush_settings"
    bl_label = "Cursor"
    bl_category = "Tool"
    bl_ui_units_x = 16


class VIEW3D_PT_tools_grease_pencil_sculpt_appearance(GreasePencilDisplayPanel, Panel, View3DPanel):
    bl_context = ".grease_pencil_sculpt"
    bl_parent_id = "VIEW3D_PT_tools_grease_pencil_v3_brush_settings"
    bl_label = "Cursor"
    bl_category = "Tool"

    @classmethod
    def poll(cls, context):
        return context.mode == 'SCULPT_GREASE_PENCIL'


class VIEW3D_PT_tools_grease_pencil_weight_appearance(GreasePencilDisplayPanel, Panel, View3DPanel):
    bl_context = ".greasepencil_weight"
    bl_parent_id = "VIEW3D_PT_tools_grease_pencil_weight_paint_settings"
    bl_category = "Tool"
    bl_label = "Cursor"


class VIEW3D_PT_tools_grease_pencil_vertex_appearance(GreasePencilDisplayPanel, Panel, View3DPanel):
    bl_context = ".greasepencil_vertex"
    bl_parent_id = "VIEW3D_PT_tools_grease_pencil_vertex_paint_settings"
    bl_category = "Tool"
    bl_label = "Cursor"


class VIEW3D_PT_gpencil_brush_presets(Panel, PresetPanel):
    """Brush settings"""
    bl_label = "Brush Presets"
    preset_subdir = "gpencil_brush"
    preset_operator = "script.execute_preset"
    preset_add_operator = "scene.gpencil_brush_preset_add"


class GreasePencilV3PaintPanel:
    bl_context = ".grease_pencil_paint"
    bl_category = "Tool"

    @classmethod
    def poll(cls, context):
        if context.space_data.type in {'VIEW_3D', 'PROPERTIES'}:
            # Hide for tools not using brushes.
            if tool_use_brush(context) is False:
                return False

            return True
        else:
            return True


class VIEW3D_PT_tools_grease_pencil_v3_brush_select(Panel, View3DPanel, GreasePencilV3PaintPanel, BrushSelectPanel):
    bl_label = "Brush Asset"


class VIEW3D_PT_tools_grease_pencil_v3_brush_settings(Panel, View3DPanel, GreasePencilV3PaintPanel):
    bl_label = "Brush Settings"
    bl_options = {'DEFAULT_CLOSED'}

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

            if brush.gpencil_brush_type in {'DRAW', 'FILL'}:
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
                    brush_basic_grease_pencil_paint_settings,
                )
                brush_basic_grease_pencil_paint_settings(layout, context, brush, None, compact=False)


class VIEW3D_PT_tools_grease_pencil_v3_brush_advanced(View3DPanel, Panel):
    bl_context = ".grease_pencil_paint"
    bl_label = "Advanced"
    bl_parent_id = "VIEW3D_PT_tools_grease_pencil_v3_brush_settings"
    bl_category = "Tool"
    bl_options = {'DEFAULT_CLOSED'}
    bl_ui_units_x = 13

    @classmethod
    def poll(cls, context):
        ob = context.active_object
        if ob.type != 'GREASEPENCIL':
            return False
        brush = context.tool_settings.gpencil_paint.brush
        return brush is not None and brush.gpencil_brush_type not in {'ERASE', 'TINT', 'FILL'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        tool_settings = context.scene.tool_settings
        gpencil_paint = tool_settings.gpencil_paint
        brush = gpencil_paint.brush
        gp_settings = brush.gpencil_settings

        col = layout.column(align=True)
        if brush is None:
            return

        row = col.row(align=True)
        row.prop(brush, "use_locked_size", expand=True)
        col.separator()

        col.prop(brush, "spacing", slider=True)
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


class VIEW3D_PT_tools_grease_pencil_v3_brush_fill_advanced(View3DPanel, Panel):
    bl_context = ".grease_pencil_paint"
    bl_label = "Advanced"
    bl_parent_id = "VIEW3D_PT_tools_grease_pencil_v3_brush_settings"
    bl_category = "Tool"
    bl_options = {'DEFAULT_CLOSED'}
    bl_ui_units_x = 14

    @classmethod
    def poll(cls, context):
        ob = context.active_object
        if ob.type != 'GREASEPENCIL':
            return False
        brush = context.tool_settings.gpencil_paint.brush
        return brush is not None and brush.gpencil_brush_type == 'FILL'

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        tool_settings = context.scene.tool_settings
        gpencil_paint = tool_settings.gpencil_paint
        brush = gpencil_paint.brush
        gp_settings = brush.gpencil_settings

        col = layout.column(align=True)
        if brush is None:
            return

        row = col.row(align=True)
        row.prop(gp_settings, "fill_draw_mode", text="Boundary", text_ctxt=i18n_contexts.id_gpencil)
        row.prop(
            gp_settings,
            "show_fill_boundary",
            icon='HIDE_OFF' if gp_settings.show_fill_boundary else 'HIDE_ON',
            text="",
        )

        col.separator()
        row = col.row(align=True)
        row.prop(gp_settings, "fill_layer_mode", text="Layers")

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
        row = col.row(align=True)
        row.prop(gp_settings, "use_auto_remove_fill_guides")


class VIEW3D_PT_tools_grease_pencil_v3_brush_stroke(Panel, View3DPanel):
    bl_context = ".grease_pencil_paint"
    bl_parent_id = "VIEW3D_PT_tools_grease_pencil_v3_brush_settings"
    bl_label = "Stroke"
    bl_category = "Tool"
    bl_options = {'DEFAULT_CLOSED'}
    bl_ui_units_x = 12

    @classmethod
    def poll(cls, context):
        ob = context.active_object
        if ob.type != 'GREASEPENCIL':
            return False
        brush = context.tool_settings.gpencil_paint.brush
        return brush is not None and brush.gpencil_brush_type == 'DRAW'

    def draw(self, _context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False


class VIEW3D_PT_tools_grease_pencil_v3_brush_post_processing(View3DPanel, Panel):
    bl_context = ".grease_pencil_paint"
    bl_parent_id = "VIEW3D_PT_tools_grease_pencil_v3_brush_stroke"
    bl_label = "Post-Processing"
    bl_category = "Tool"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        ob = context.active_object
        if ob.type != 'GREASEPENCIL':
            return False
        brush = context.tool_settings.gpencil_paint.brush
        return brush is not None and brush.gpencil_brush_type not in {'ERASE', 'FILL', 'TINT'}

    def draw_header(self, context):
        brush = context.tool_settings.gpencil_paint.brush
        gp_settings = brush.gpencil_settings
        self.layout.use_property_split = False
        self.layout.prop(gp_settings, "use_settings_postprocess", text=self.bl_label if self.is_popover else "")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        brush = context.tool_settings.gpencil_paint.brush
        gp_settings = brush.gpencil_settings

        col = layout.column()
        col.active = gp_settings.use_settings_postprocess

        col1 = col.column(align=True)
        col1.prop(gp_settings, "pen_smooth_factor")
        col1.prop(gp_settings, "pen_smooth_steps")

        col1 = col.column(align=True)
        col1.prop(gp_settings, "pen_subdivision_steps", text="Subdivisions")

        col1 = col.column(align=True)
        col1.prop(gp_settings, "simplify_pixel_threshold", slider=True)

        col1 = col.column(align=True)
        col1.prop(gp_settings, "use_trim")

        col.separator()

        row = col.row(heading="Outline", align=True)
        row.prop(gp_settings, "use_settings_outline", text="")
        row2 = row.row(align=True)
        row2.enabled = gp_settings.use_settings_outline
        row2.prop(gp_settings, "material_alt", text="")

        row2 = col.row(align=True)
        row2.enabled = gp_settings.use_settings_outline
        row2.prop(gp_settings, "outline_thickness_factor")


class VIEW3D_PT_tools_grease_pencil_v3_brush_random(View3DPanel, Panel):
    bl_context = ".grease_pencil_paint"
    bl_parent_id = "VIEW3D_PT_tools_grease_pencil_v3_brush_stroke"
    bl_label = "Randomize"
    bl_category = "Tool"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        ob = context.active_object
        if ob.type != 'GREASEPENCIL':
            return False
        brush = context.tool_settings.gpencil_paint.brush
        return brush is not None and brush.gpencil_brush_type not in {'ERASE', 'FILL', 'TINT'}

    def draw_header(self, context):
        brush = context.tool_settings.gpencil_paint.brush
        gp_settings = brush.gpencil_settings
        self.layout.use_property_split = False
        self.layout.prop(gp_settings, "use_settings_random", text=self.bl_label if self.is_popover else "")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        tool_settings = context.tool_settings
        paint = tool_settings.gpencil_paint
        brush = tool_settings.gpencil_paint.brush
        mode = tool_settings.gpencil_paint.color_mode
        gp_settings = brush.gpencil_settings

        col = layout.column()
        col.enabled = gp_settings.use_settings_random

        row = col.row(align=True)
        row.prop(gp_settings, "random_pressure", text="Radius", slider=True)
        row.prop(gp_settings, "use_stroke_random_radius", text="", icon='GP_SELECT_STROKES')
        row.prop(gp_settings, "use_random_press_radius", text="", icon='STYLUS_PRESSURE')
        if gp_settings.use_random_press_radius and self.is_popover is False:
            col.template_curve_mapping(gp_settings, "curve_random_pressure", brush=True)

        row = col.row(align=True)
        row.prop(gp_settings, "random_strength", text="Strength", slider=True, text_ctxt=i18n_contexts.id_gpencil)
        row.prop(gp_settings, "use_stroke_random_strength", text="", icon='GP_SELECT_STROKES')
        row.prop(gp_settings, "use_random_press_strength", text="", icon='STYLUS_PRESSURE')
        if gp_settings.use_random_press_strength and self.is_popover is False:
            col.template_curve_mapping(gp_settings, "curve_random_strength", brush=True)

        row = col.row(align=True)
        row.prop(gp_settings, "uv_random", text="Rotation", slider=True)
        row.prop(gp_settings, "use_stroke_random_uv", text="", icon='GP_SELECT_STROKES')
        row.prop(gp_settings, "use_random_press_uv", text="", icon='STYLUS_PRESSURE')
        if gp_settings.use_random_press_uv and self.is_popover is False:
            col.template_curve_mapping(gp_settings, "curve_random_uv", brush=True)

        col.separator()

        col1 = col.column(align=True)
        col1.enabled = mode == 'VERTEXCOLOR' and gp_settings.use_settings_random
        row = col1.row(align=True)
        row.prop(brush, "hue_jitter", slider=True)
        row.prop(brush, "use_stroke_random_hue", text="", icon='GP_SELECT_STROKES')
        row.prop(brush, "use_random_press_hue", text="", icon='STYLUS_PRESSURE')
        if brush.use_random_press_hue and self.is_popover is False:
            col1.template_curve_mapping(brush, "curve_random_hue", brush=True)

        row = col1.row(align=True)
        row.prop(brush, "saturation_jitter", slider=True)
        row.prop(brush, "use_stroke_random_sat", text="", icon='GP_SELECT_STROKES')
        row.prop(brush, "use_random_press_sat", text="", icon='STYLUS_PRESSURE')
        if brush.use_random_press_sat and self.is_popover is False:
            col1.template_curve_mapping(brush, "curve_random_saturation", brush=True)

        row = col1.row(align=True)
        row.prop(brush, "value_jitter", slider=True)
        row.prop(brush, "use_stroke_random_val", text="", icon='GP_SELECT_STROKES')
        row.prop(brush, "use_random_press_val", text="", icon='STYLUS_PRESSURE')
        if brush.use_random_press_val and self.is_popover is False:
            col1.template_curve_mapping(brush, "curve_random_value", brush=True)

        col.separator()

        row = col.row(align=True)
        row.prop(gp_settings, "pen_jitter", slider=True)
        row.prop(gp_settings, "use_jitter_pressure", text="", icon='STYLUS_PRESSURE')
        if self.is_popover is False:
            row.prop(
                paint,
                "show_jitter_curve",
                text="",
                icon='DOWNARROW_HLT' if paint.show_jitter_curve else 'RIGHTARROW',
                emboss=False,
            )
            if paint.show_jitter_curve:
                col.active = gp_settings.use_jitter_pressure
                col.template_curve_mapping(gp_settings, "curve_jitter", brush=True, show_presets=True)


class VIEW3D_PT_tools_grease_pencil_v3_brush_stabilizer(Panel, View3DPanel):
    bl_context = ".grease_pencil_paint"
    bl_parent_id = "VIEW3D_PT_tools_grease_pencil_v3_brush_stroke"
    bl_label = "Stabilize Stroke"
    bl_category = "Tool"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        ob = context.active_object
        if ob.type != 'GREASEPENCIL':
            return False
        brush = context.tool_settings.gpencil_paint.brush
        return brush is not None and brush.gpencil_brush_type == 'DRAW'

    def draw_header(self, context):
        brush = context.tool_settings.gpencil_paint.brush
        self.layout.use_property_split = False
        self.layout.prop(brush, "use_smooth_stroke", text=self.bl_label if self.is_popover else "")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        brush = context.tool_settings.gpencil_paint.brush

        col = layout.column()
        col.active = brush.use_smooth_stroke

        col.prop(brush, "smooth_stroke_radius", text="Radius", slider=True)
        col.prop(brush, "smooth_stroke_factor", text="Factor", slider=True)


class VIEW3D_PT_tools_grease_pencil_v3_brush_mixcolor(View3DPanel, Panel):
    bl_context = ".grease_pencil_paint"
    bl_label = "Color"
    bl_category = "Tool"

    @classmethod
    def poll(cls, context):
        ob = context.object
        tool_settings = context.tool_settings
        settings = tool_settings.gpencil_paint
        brush = settings.brush

        if ob is None or brush is None:
            return False

        if context.region.type == 'TOOL_HEADER':
            return False

        from bl_ui.space_toolsystem_common import ToolSelectPanelHelper
        tool = ToolSelectPanelHelper.tool_active_from_context(context)
        if tool and tool.idname in {"builtin.cutter", "builtin.eyedropper", "builtin.interpolate"}:
            return False

        if brush.gpencil_brush_type == 'TINT':
            return True

        if brush.gpencil_brush_type not in {'DRAW', 'FILL'}:
            return False

        return True

    def draw(self, context):
        layout = self.layout
        tool_settings = context.tool_settings
        settings = tool_settings.gpencil_paint
        brush = settings.brush
        gp_settings = brush.gpencil_settings
        use_unified_paint = (context.object.mode != 'PAINT_GREASE_PENCIL')
        ups = settings.unified_paint_settings
        prop_owner = ups if use_unified_paint and ups.use_unified_color else brush

        row = layout.row()
        row.prop(settings, "color_mode", expand=True)

        layout.use_property_split = True
        layout.use_property_decorate = False
        col = layout.column()
        col.enabled = settings.color_mode == 'VERTEXCOLOR'

        # This panel is only used for Draw mode, which does not use unified paint settings.
        col.template_color_picker(prop_owner, "color", value_slider=True)

        sub_row = col.row(align=True)
        if use_unified_paint:
            UnifiedPaintPanel.prop_unified_color(sub_row, context, brush, "color", text="")
            UnifiedPaintPanel.prop_unified_color(sub_row, context, brush, "secondary_color", text="")
        else:
            sub_row.prop(brush, "color", text="")
            sub_row.prop(brush, "secondary_color", text="")

        sub_row.operator("paint.brush_colors_flip", icon='FILE_REFRESH', text="")

        if brush.gpencil_brush_type in {'DRAW', 'FILL'}:
            col.prop(gp_settings, "vertex_mode", text="Mode")
            col.prop(gp_settings, "vertex_color_factor", slider=True, text="Mix Factor")


class VIEW3D_PT_tools_grease_pencil_v3_brush_mix_palette(View3DPanel, Panel):
    bl_context = ".grease_pencil_paint"
    bl_label = "Palette"
    bl_category = "Tool"
    bl_parent_id = "VIEW3D_PT_tools_grease_pencil_v3_brush_mixcolor"

    @classmethod
    def poll(cls, context):
        ob = context.object
        tool_settings = context.tool_settings
        settings = tool_settings.gpencil_paint
        brush = settings.brush

        if ob is None or brush is None:
            return False

        from bl_ui.space_toolsystem_common import ToolSelectPanelHelper
        tool = ToolSelectPanelHelper.tool_active_from_context(context)
        if tool and tool.idname in {"builtin.cutter", "builtin.eyedropper", "builtin.interpolate"}:
            return False

        if brush.gpencil_brush_type == 'TINT':
            return True

        if brush.gpencil_brush_type not in {'DRAW', 'FILL'}:
            return False

        return True

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False
        tool_settings = context.tool_settings
        settings = tool_settings.gpencil_paint

        col = layout.column()
        col.enabled = settings.color_mode == 'VERTEXCOLOR'

        row = col.row(align=True)
        row.template_ID(settings, "palette", new="palette.new")
        if settings.palette:
            col.template_palette(settings, "palette", color=True)


class VIEW3D_PT_tools_grease_pencil_v3_brush_eraser(View3DPanel, Panel):
    bl_context = ".grease_pencil_paint"
    bl_label = "Eraser"
    bl_category = "Tool"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        if context.region.type == 'TOOL_HEADER':
            return False

        from bl_ui.space_toolsystem_common import ToolSelectPanelHelper
        tool = ToolSelectPanelHelper.tool_active_from_context(context)
        return (tool and tool.idname == "builtin.brush")

    def draw(self, context):
        layout = self.layout
        tool_settings = context.tool_settings
        settings = tool_settings.gpencil_paint

        layout.use_property_split = True
        layout.use_property_decorate = False

        col = layout.column()
        col.prop_search(settings, "eraser_brush", bpy.data, "brushes")


class VIEW3D_PT_tools_grease_pencil_v3_brush_gap_closure(View3DPanel, Panel):
    bl_context = ".grease_pencil_paint"
    bl_parent_id = "VIEW3D_PT_tools_grease_pencil_v3_brush_fill_advanced"
    bl_label = "Gap Closure"
    bl_category = "Tool"

    @classmethod
    def poll(cls, context):
        brush = context.tool_settings.gpencil_paint.brush
        return brush is not None and brush.gpencil_brush_type == 'FILL'

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        tool_settings = context.tool_settings
        brush = tool_settings.gpencil_paint.brush
        gp_settings = brush.gpencil_settings

        col = layout.column()

        col.prop(gp_settings, "extend_stroke_factor", text="Size")
        row = col.row(align=True)
        row.prop(gp_settings, "fill_extend_mode", text="Mode")
        row = col.row(align=True)
        row.prop(gp_settings, "show_fill_extend", text="Visual Aids")

        if gp_settings.fill_extend_mode == 'EXTEND':
            row = col.row(align=True)
            row.prop(gp_settings, "use_collide_strokes")


classes = (
    VIEW3D_MT_brush_context_menu,
    VIEW3D_PT_tools_object_options,
    VIEW3D_PT_tools_object_options_transform,
    VIEW3D_PT_tools_meshedit_options,
    VIEW3D_PT_tools_meshedit_options_transform,
    VIEW3D_PT_tools_meshedit_options_uvs,
    VIEW3D_PT_tools_armatureedit_options,
    VIEW3D_PT_tools_posemode_options,

    VIEW3D_PT_slots_projectpaint,
    VIEW3D_PT_slots_paint_canvas,
    VIEW3D_PT_slots_color_attributes,
    VIEW3D_PT_slots_vertex_groups,
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
    VIEW3D_PT_tools_weight_gradient,

    VIEW3D_PT_sculpt_dyntopo,
    VIEW3D_PT_sculpt_voxel_remesh,
    VIEW3D_PT_sculpt_symmetry,
    VIEW3D_PT_sculpt_symmetry_for_topbar,
    VIEW3D_PT_sculpt_options,
    VIEW3D_PT_sculpt_options_gravity,

    VIEW3D_PT_curves_sculpt_symmetry,
    VIEW3D_PT_curves_sculpt_symmetry_for_topbar,

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
    VIEW3D_PT_tools_grease_pencil_sculpt_brush_popover,
    VIEW3D_PT_tools_grease_pencil_weight_paint_select,
    VIEW3D_PT_tools_grease_pencil_weight_paint_settings,
    VIEW3D_PT_tools_grease_pencil_weight_options,
    VIEW3D_PT_tools_grease_pencil_weight_appearance,
    VIEW3D_PT_tools_grease_pencil_vertex_paint_select,
    VIEW3D_PT_tools_grease_pencil_vertex_paint_settings,
    VIEW3D_PT_tools_grease_pencil_vertex_appearance,

    VIEW3D_PT_tools_grease_pencil_v3_brush_select,
    VIEW3D_PT_tools_grease_pencil_v3_brush_settings,
    VIEW3D_PT_tools_grease_pencil_v3_brush_eraser,
    VIEW3D_PT_tools_grease_pencil_v3_brush_advanced,
    VIEW3D_PT_tools_grease_pencil_v3_brush_fill_advanced,
    VIEW3D_PT_tools_grease_pencil_v3_brush_stroke,
    VIEW3D_PT_tools_grease_pencil_v3_brush_post_processing,
    VIEW3D_PT_tools_grease_pencil_v3_brush_random,
    VIEW3D_PT_tools_grease_pencil_v3_brush_stabilizer,
    VIEW3D_PT_tools_grease_pencil_v3_brush_mixcolor,
    VIEW3D_PT_tools_grease_pencil_v3_brush_mix_palette,
    VIEW3D_PT_tools_grease_pencil_v3_brush_gap_closure,
    VIEW3D_PT_tools_grease_pencil_paint_appearance,
    VIEW3D_PT_tools_grease_pencil_sculpt_appearance,

    VIEW3D_PT_tools_grease_pencil_brush_weight_falloff,
    VIEW3D_PT_tools_grease_pencil_brush_vertex_color,
    VIEW3D_PT_tools_grease_pencil_brush_vertex_palette,
    VIEW3D_PT_tools_grease_pencil_brush_vertex_falloff,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
