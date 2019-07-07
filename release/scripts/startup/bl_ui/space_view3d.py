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
from bpy.types import (
    Header,
    Menu,
    Panel,
)
from bl_ui.properties_paint_common import (
    UnifiedPaintPanel,
)
from bl_ui.properties_grease_pencil_common import (
    AnnotationDataPanel,
    AnnotationOnionSkin,
    GreasePencilMaterialsPanel,
)
from bl_ui.space_toolsystem_common import (
    ToolActivePanelHelper,
)
from bpy.app.translations import contexts as i18n_contexts


class VIEW3D_HT_tool_header(Header):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'TOOL_HEADER'

    def draw(self, context):
        layout = self.layout

        layout.row(align=True).template_header()

        self.draw_tool_settings(context)

        layout.separator_spacer()

        VIEW3D_HT_header.draw_xform_template(layout, context)

        layout.separator_spacer()

        self.draw_mode_settings(context)

    def draw_tool_settings(self, context):
        layout = self.layout
        tool_mode = context.mode

        # Active Tool
        # -----------
        from bl_ui.space_toolsystem_common import ToolSelectPanelHelper
        tool = ToolSelectPanelHelper.draw_active_tool_header(
            context, layout,
            tool_key=('VIEW_3D', tool_mode),
        )

        # Object Mode Options
        # -------------------

        # Example of how tool_settings can be accessed as pop-overs.

        # TODO(campbell): editing options should be after active tool options
        # (obviously separated for from the users POV)
        draw_fn = getattr(_draw_tool_settings_context_mode, tool_mode, None)
        if draw_fn is not None:
            draw_fn(context, layout, tool)

        popover_kw = {"space_type": 'VIEW_3D', "region_type": 'UI', "category": "Tool"}

        # Note: general mode options should be added to 'draw_mode_settings'.
        if tool_mode == 'SCULPT':
            if (tool is not None) and tool.has_datablock:
                layout.popover_group(context=".paint_common", **popover_kw)
        elif tool_mode == 'PAINT_VERTEX':
            if (tool is not None) and tool.has_datablock:
                layout.popover_group(context=".paint_common", **popover_kw)
        elif tool_mode == 'PAINT_WEIGHT':
            if (tool is not None) and tool.has_datablock:
                layout.popover_group(context=".paint_common", **popover_kw)
        elif tool_mode == 'PAINT_TEXTURE':
            if (tool is not None) and tool.has_datablock:
                layout.popover_group(context=".paint_common", **popover_kw)
        elif tool_mode == 'EDIT_ARMATURE':
            pass
        elif tool_mode == 'EDIT_CURVE':
            pass
        elif tool_mode == 'EDIT_MESH':
            pass
        elif tool_mode == 'POSE':
            pass
        elif tool_mode == 'PARTICLE':
            # Disable, only shows "Brush" panel, which is already in the top-bar.
            # if tool.has_datablock:
            #     layout.popover_group(context=".paint_common", **popover_kw)
            pass
        elif tool_mode == 'PAINT_GPENCIL':
            if (tool is not None) and tool.has_datablock:
                layout.popover_group(context=".greasepencil_paint", **popover_kw)
        elif tool_mode == 'SCULPT_GPENCIL':
            layout.popover_group(context=".greasepencil_sculpt", **popover_kw)
        elif tool_mode == 'WEIGHT_GPENCIL':
            layout.popover_group(context=".greasepencil_weight", **popover_kw)

    def draw_mode_settings(self, context):
        layout = self.layout
        mode_string = context.mode

        def row_for_mirror():
            row = layout.row(align=True)
            row.label(icon='MOD_MIRROR')
            sub = row.row(align=True)
            sub.scale_x = 0.6
            return row, sub

        if mode_string == 'EDIT_MESH':
            _row, sub = row_for_mirror()
            sub.prop(context.object.data, "use_mirror_x", text="X", toggle=True)
            tool_settings = context.tool_settings
            layout.prop(tool_settings, "use_mesh_automerge", text="")
        elif mode_string == 'EDIT_ARMATURE':
            _row, sub = row_for_mirror()
            sub.prop(context.object.data, "use_mirror_x", text="X", toggle=True)
        elif mode_string == 'POSE':
            _row, sub = row_for_mirror()
            sub.prop(context.object.pose, "use_mirror_x", text="X", toggle=True)
        elif mode_string == 'PAINT_WEIGHT':
            row, sub = row_for_mirror()
            sub.prop(context.object.data, "use_mirror_x", text="X", toggle=True)
            row.popover(panel="VIEW3D_PT_tools_weightpaint_symmetry_for_topbar", text="")
        elif mode_string == 'SCULPT':
            row, sub = row_for_mirror()
            sculpt = context.tool_settings.sculpt
            sub.prop(sculpt, "use_symmetry_x", text="X", toggle=True)
            sub.prop(sculpt, "use_symmetry_y", text="Y", toggle=True)
            sub.prop(sculpt, "use_symmetry_z", text="Z", toggle=True)
            row.popover(panel="VIEW3D_PT_sculpt_symmetry_for_topbar", text="")
        elif mode_string == 'PAINT_TEXTURE':
            _row, sub = row_for_mirror()
            ipaint = context.tool_settings.image_paint
            sub.prop(ipaint, "use_symmetry_x", text="X", toggle=True)
            sub.prop(ipaint, "use_symmetry_y", text="Y", toggle=True)
            sub.prop(ipaint, "use_symmetry_z", text="Z", toggle=True)
            # No need for a popover, the panel only has these options.
        elif mode_string == 'PAINT_VERTEX':
            row, sub = row_for_mirror()
            vpaint = context.tool_settings.vertex_paint
            sub.prop(vpaint, "use_symmetry_x", text="X", toggle=True)
            sub.prop(vpaint, "use_symmetry_y", text="Y", toggle=True)
            sub.prop(vpaint, "use_symmetry_z", text="Z", toggle=True)
            row.popover(panel="VIEW3D_PT_tools_vertexpaint_symmetry_for_topbar", text="")

        # Expand panels from the side-bar as popovers.
        popover_kw = {"space_type": 'VIEW_3D', "region_type": 'UI', "category": "Tool"}

        if mode_string == 'SCULPT':
            layout.popover_group(context=".sculpt_mode", **popover_kw)
        elif mode_string == 'PAINT_VERTEX':
            layout.popover_group(context=".vertexpaint", **popover_kw)
        elif mode_string == 'PAINT_WEIGHT':
            layout.popover_group(context=".weightpaint", **popover_kw)
        elif mode_string == 'PAINT_TEXTURE':
            layout.popover_group(context=".imagepaint", **popover_kw)
        elif mode_string == 'EDIT_TEXT':
            layout.popover_group(context=".text_edit", **popover_kw)
        elif mode_string == 'EDIT_ARMATURE':
            layout.popover_group(context=".armature_edit", **popover_kw)
        elif mode_string == 'EDIT_METABALL':
            layout.popover_group(context=".mball_edit", **popover_kw)
        elif mode_string == 'EDIT_LATTICE':
            layout.popover_group(context=".lattice_edit", **popover_kw)
        elif mode_string == 'EDIT_CURVE':
            layout.popover_group(context=".curve_edit", **popover_kw)
        elif mode_string == 'EDIT_MESH':
            layout.popover_group(context=".mesh_edit", **popover_kw)
        elif mode_string == 'POSE':
            layout.popover_group(context=".posemode", **popover_kw)
        elif mode_string == 'PARTICLE':
            layout.popover_group(context=".particlemode", **popover_kw)
        elif mode_string == 'OBJECT':
            layout.popover_group(context=".objectmode", **popover_kw)
        elif mode_string in {'PAINT_GPENCIL', 'EDIT_GPENCIL', 'SCULPT_GPENCIL', 'WEIGHT_GPENCIL'}:
            # Grease pencil layer.
            gpl = context.active_gpencil_layer
            if gpl and gpl.info is not None:
                text = gpl.info
                maxw = 25
                if len(text) > maxw:
                    text = text[:maxw - 5] + '..' + text[-3:]
            else:
                text = ""

            layout.label(text="Layer:")
            sub = layout.row()
            sub.ui_units_x = 8
            sub.popover(
                panel="TOPBAR_PT_gpencil_layers",
                text=text,
            )


class _draw_tool_settings_context_mode:
    @staticmethod
    def SCULPT(context, layout, tool):
        if (tool is None) or (not tool.has_datablock):
            return

        paint = context.tool_settings.sculpt
        layout.template_ID_preview(paint, "brush", rows=3, cols=8, hide_buttons=True)

        brush = paint.brush
        if brush is None:
            return

        from bl_ui.properties_paint_common import (
            brush_basic_sculpt_settings,
        )
        brush_basic_sculpt_settings(layout, context, brush, compact=True)

    @staticmethod
    def PAINT_TEXTURE(context, layout, tool):
        if (tool is None) or (not tool.has_datablock):
            return

        paint = context.tool_settings.image_paint
        layout.template_ID_preview(paint, "brush", rows=3, cols=8, hide_buttons=True)

        brush = paint.brush
        if brush is None:
            return

        from bl_ui.properties_paint_common import (
            UnifiedPaintPanel,
            brush_basic_texpaint_settings,
        )
        capabilities = brush.image_paint_capabilities
        if capabilities.has_color:
            UnifiedPaintPanel.prop_unified_color(layout, context, brush, "color", text="")
        brush_basic_texpaint_settings(layout, context, brush, compact=True)

    @staticmethod
    def PAINT_VERTEX(context, layout, tool):
        if (tool is None) or (not tool.has_datablock):
            return

        paint = context.tool_settings.vertex_paint
        layout.template_ID_preview(paint, "brush", rows=3, cols=8, hide_buttons=True)

        brush = paint.brush
        if brush is None:
            return

        from bl_ui.properties_paint_common import (
            UnifiedPaintPanel,
            brush_basic_vpaint_settings,
        )
        capabilities = brush.vertex_paint_capabilities
        if capabilities.has_color:
            UnifiedPaintPanel.prop_unified_color(layout, context, brush, "color", text="")
        brush_basic_vpaint_settings(layout, context, brush, compact=True)

    @staticmethod
    def PAINT_WEIGHT(context, layout, tool):
        if (tool is None) or (not tool.has_datablock):
            return

        paint = context.tool_settings.weight_paint
        layout.template_ID_preview(paint, "brush", rows=3, cols=8, hide_buttons=True)
        brush = paint.brush
        if brush is None:
            return

        from bl_ui.properties_paint_common import brush_basic_wpaint_settings
        brush_basic_wpaint_settings(layout, context, brush, compact=True)

    @staticmethod
    def PAINT_GPENCIL(context, layout, tool):
        if tool is None:
            return

        # is_paint = True
        # FIXME: tools must use their own UI drawing!
        if tool.idname in {"builtin.line", "builtin.box", "builtin.circle", "builtin.arc", "builtin.curve"}:
            # is_paint = False
            pass
        elif tool.idname == "Cutter":
            row = layout.row(align=True)
            row.prop(context.tool_settings.gpencil_sculpt, "intersection_threshold")
            return
        elif not tool.has_datablock:
            return

        paint = context.tool_settings.gpencil_paint
        brush = paint.brush
        if brush is None:
            return

        gp_settings = brush.gpencil_settings

        def draw_color_selector():
            ma = gp_settings.material
            row = layout.row(align=True)
            if not gp_settings.use_material_pin:
                ma = context.object.active_material
            icon_id = 0
            if ma:
                icon_id = ma.id_data.preview.icon_id
                txt_ma = ma.name
                maxw = 25
                if len(txt_ma) > maxw:
                    txt_ma = txt_ma[:maxw - 5] + '..' + txt_ma[-3:]
            else:
                txt_ma = ""

            sub = row.row()
            sub.ui_units_x = 8
            sub.popover(
                panel="TOPBAR_PT_gpencil_materials",
                text=txt_ma,
                icon_value=icon_id,
            )

            row.prop(gp_settings, "use_material_pin", text="")

        row = layout.row(align=True)
        tool_settings = context.scene.tool_settings
        settings = tool_settings.gpencil_paint
        row.template_ID_preview(settings, "brush", rows=3, cols=8, hide_buttons=True)

        if context.object and brush.gpencil_tool in {'FILL', 'DRAW'}:
            draw_color_selector()

        from bl_ui.properties_paint_common import (
            brush_basic_gpencil_paint_settings,
        )
        brush_basic_gpencil_paint_settings(layout, context, brush, compact=True)

        # FIXME: tools must use their own UI drawing!
        if tool.idname in {"builtin.arc", "builtin.curve", "builtin.line", "builtin.box", "builtin.circle"}:
            settings = context.tool_settings.gpencil_sculpt
            row = layout.row(align=True)
            row.prop(settings, "use_thickness_curve", text="", icon='CURVE_DATA')
            sub = row.row(align=True)
            sub.active = settings.use_thickness_curve
            sub.popover(
                panel="TOPBAR_PT_gpencil_primitive",
                text="Thickness Profile",
            )

        if brush.gpencil_tool == 'FILL':
            settings = context.tool_settings.gpencil_sculpt
            row = layout.row(align=True)
            sub = row.row(align=True)
            sub.popover(
                panel="TOPBAR_PT_gpencil_fill",
                text="Fill Options",
            )

    @staticmethod
    def SCULPT_GPENCIL(context, layout, tool):
        if (tool is None) or (not tool.has_datablock):
            return
        tool_settings = context.tool_settings
        settings = tool_settings.gpencil_sculpt
        brush = settings.brush

        from bl_ui.properties_paint_common import (
            brush_basic_gpencil_sculpt_settings,
        )
        brush_basic_gpencil_sculpt_settings(layout, context, brush, compact=True)

    @staticmethod
    def WEIGHT_GPENCIL(context, layout, tool):
        if (tool is None) or (not tool.has_datablock):
            return
        tool_settings = context.tool_settings
        settings = tool_settings.gpencil_sculpt
        brush = settings.brush

        from bl_ui.properties_paint_common import (
            brush_basic_gpencil_weight_settings,
        )
        brush_basic_gpencil_weight_settings(layout, context, brush, compact=True)

    @staticmethod
    def PARTICLE(context, layout, tool):
        if (tool is None) or (not tool.has_datablock):
            return

        # See: 'VIEW3D_PT_tools_brush', basically a duplicate
        settings = context.tool_settings.particle_edit
        brush = settings.brush
        tool = settings.tool
        if tool != 'NONE':
            layout.prop(brush, "size", slider=True)
            if tool == 'ADD':
                layout.prop(brush, "count")

                layout.prop(settings, "use_default_interpolate")
                layout.prop(brush, "steps", slider=True)
                layout.prop(settings, "default_key_count", slider=True)
            else:
                layout.prop(brush, "strength", slider=True)

                if tool == 'LENGTH':
                    layout.row().prop(brush, "length_mode", expand=True)
                elif tool == 'PUFF':
                    layout.row().prop(brush, "puff_mode", expand=True)
                    layout.prop(brush, "use_puff_volume")
                elif tool == 'COMB':
                    row = layout.row()
                    row.active = settings.is_editable
                    row.prop(settings, "use_emitter_deflect", text="Deflect Emitter")
                    sub = row.row(align=True)
                    sub.active = settings.use_emitter_deflect
                    sub.prop(settings, "emitter_distance", text="Distance")


class VIEW3D_HT_header(Header):
    bl_space_type = 'VIEW_3D'

    @staticmethod
    def draw_xform_template(layout, context):
        obj = context.active_object
        object_mode = 'OBJECT' if obj is None else obj.mode
        has_pose_mode = (
            (object_mode == 'POSE') or
            (object_mode == 'WEIGHT_PAINT' and context.pose_object is not None)
        )

        tool_settings = context.tool_settings

        # Mode & Transform Settings
        scene = context.scene

        # Orientation
        if object_mode in {'OBJECT', 'EDIT', 'EDIT_GPENCIL'} or has_pose_mode:
            orient_slot = scene.transform_orientation_slots[0]
            row = layout.row(align=True)

            sub = row.row()
            sub.ui_units_x = 4
            sub.prop_with_popover(
                orient_slot,
                "type",
                text="",
                panel="VIEW3D_PT_transform_orientations",
            )

        # Pivot
        if object_mode in {'OBJECT', 'EDIT', 'EDIT_GPENCIL', 'SCULPT_GPENCIL'} or has_pose_mode:
            layout.prop_with_popover(
                tool_settings,
                "transform_pivot_point",
                text="",
                icon_only=True,
                panel="VIEW3D_PT_pivot_point",
            )

        # Snap
        show_snap = False
        if obj is None:
            show_snap = True
        else:
            if (object_mode not in {
                    'SCULPT', 'VERTEX_PAINT', 'WEIGHT_PAINT', 'TEXTURE_PAINT',
                    'PAINT_GPENCIL', 'SCULPT_GPENCIL', 'WEIGHT_GPENCIL'
            }) or has_pose_mode:
                show_snap = True
            else:

                from bl_ui.properties_paint_common import UnifiedPaintPanel
                paint_settings = UnifiedPaintPanel.paint_settings(context)

                if paint_settings:
                    brush = paint_settings.brush
                    if brush and brush.stroke_method == 'CURVE':
                        show_snap = True

        if show_snap:
            snap_items = bpy.types.ToolSettings.bl_rna.properties["snap_elements"].enum_items
            snap_elements = tool_settings.snap_elements
            if len(snap_elements) == 1:
                text = ""
                for elem in snap_elements:
                    icon = snap_items[elem].icon
                    break
            else:
                text = "Mix"
                icon = 'NONE'
            del snap_items, snap_elements

            row = layout.row(align=True)
            row.prop(tool_settings, "use_snap", text="")

            sub = row.row(align=True)
            sub.popover(
                panel="VIEW3D_PT_snapping",
                icon=icon,
                text=text,
            )

        # Proportional editing
        if object_mode in {'EDIT', 'PARTICLE_EDIT', 'SCULPT_GPENCIL', 'EDIT_GPENCIL', 'OBJECT'}:
            row = layout.row(align=True)
            kw = {}
            if object_mode == 'OBJECT':
                attr = "use_proportional_edit_objects"
            else:
                attr = "use_proportional_edit"

                if tool_settings.use_proportional_edit:
                    if tool_settings.use_proportional_connected:
                        kw["icon"] = 'PROP_CON'
                    elif tool_settings.use_proportional_projected:
                        kw["icon"] = 'PROP_PROJECTED'
                    else:
                        kw["icon"] = 'PROP_ON'
                else:
                    kw["icon"] = 'PROP_OFF'

            row.prop(tool_settings, attr, icon_only=True, **kw)
            sub = row.row(align=True)
            sub.active = getattr(tool_settings, attr)
            sub.prop_with_popover(
                tool_settings,
                "proportional_edit_falloff",
                text="",
                icon_only=True,
                panel="VIEW3D_PT_proportional_edit",
            )

    def draw(self, context):
        layout = self.layout

        tool_settings = context.tool_settings
        view = context.space_data
        shading = view.shading
        # mode_string = context.mode
        obj = context.active_object
        show_region_tool_header = view.show_region_tool_header

        if not show_region_tool_header:
            layout.row(align=True).template_header()

        row = layout.row(align=True)
        object_mode = 'OBJECT' if obj is None else obj.mode

        # Note: This is actually deadly in case enum_items have to be dynamically generated
        #       (because internal RNA array iterator will free everything immediately...).
        # XXX This is an RNA internal issue, not sure how to fix it.
        # Note: Tried to add an accessor to get translated UI strings instead of manual call
        #       to pgettext_iface below, but this fails because translated enumitems
        #       are always dynamically allocated.
        act_mode_item = bpy.types.Object.bl_rna.properties["mode"].enum_items[object_mode]
        act_mode_i18n_context = bpy.types.Object.bl_rna.properties["mode"].translation_context

        sub = row.row(align=True)
        sub.ui_units_x = 5.5
        sub.operator_menu_enum(
            "object.mode_set", "mode",
            text=bpy.app.translations.pgettext_iface(act_mode_item.name, act_mode_i18n_context),
            icon=act_mode_item.icon,
        )
        del act_mode_item

        layout.template_header_3D_mode()

        # Contains buttons like Mode, Pivot, Layer, Mesh Select Mode...
        if obj:
            # Particle edit
            if object_mode == 'PARTICLE_EDIT':
                row = layout.row()
                row.prop(tool_settings.particle_edit, "select_mode", text="", expand=True)

        # Grease Pencil
        if obj and obj.type == 'GPENCIL' and context.gpencil_data:
            gpd = context.gpencil_data

            if gpd.is_stroke_paint_mode:
                row = layout.row()
                sub = row.row(align=True)
                sub.prop(tool_settings, "use_gpencil_draw_onback", text="", icon='MOD_OPACITY')
                sub.separator(factor=0.4)
                sub.prop(tool_settings, "use_gpencil_weight_data_add", text="", icon='WPAINT_HLT')
                sub.separator(factor=0.4)
                sub.prop(tool_settings, "use_gpencil_draw_additive", text="", icon='FREEZE')

            if gpd.use_stroke_edit_mode:
                row = layout.row(align=True)
                row.prop(tool_settings, "gpencil_selectmode", text="", expand=True)

            if gpd.use_stroke_edit_mode or gpd.is_stroke_sculpt_mode or gpd.is_stroke_weight_mode:
                row = layout.row(align=True)

                if gpd.is_stroke_sculpt_mode:
                    row.prop(tool_settings.gpencil_sculpt, "use_select_mask", text="")
                    row.separator()

                row.prop(gpd, "use_multiedit", text="", icon='GP_MULTIFRAME_EDITING')

                sub = row.row(align=True)
                sub.active = gpd.use_multiedit
                sub.popover(
                    panel="VIEW3D_PT_gpencil_multi_frame",
                    text="Multiframe",
                )

            if gpd.use_stroke_edit_mode:
                row = layout.row(align=True)
                row.prop(tool_settings.gpencil_sculpt, "use_select_mask", text="")

                row.popover(
                    panel="VIEW3D_PT_tools_grease_pencil_interpolate",
                    text="Interpolate",
                )

        overlay = view.overlay

        VIEW3D_MT_editor_menus.draw_collapsible(context, layout)

        layout.separator_spacer()

        if object_mode in {'PAINT_GPENCIL', 'SCULPT_GPENCIL'}:
            # Grease pencil
            if object_mode == 'PAINT_GPENCIL':
                layout.prop_with_popover(
                    tool_settings,
                    "gpencil_stroke_placement_view3d",
                    text="",
                    panel="VIEW3D_PT_gpencil_origin",
                )

            if object_mode in {'PAINT_GPENCIL', 'SCULPT_GPENCIL'}:
                layout.prop_with_popover(
                    tool_settings.gpencil_sculpt,
                    "lock_axis",
                    text="",
                    panel="VIEW3D_PT_gpencil_lock",
                )

            if object_mode == 'PAINT_GPENCIL':
                # FIXME: this is bad practice!
                # Tool options are to be displayed in the topbar.
                if context.workspace.tools.from_space_view3d_mode(object_mode).idname == "builtin_brush.Draw":
                    settings = tool_settings.gpencil_sculpt.guide
                    row = layout.row(align=True)
                    row.prop(settings, "use_guide", text="", icon='GRID')
                    sub = row.row(align=True)
                    sub.active = settings.use_guide
                    sub.popover(
                        panel="VIEW3D_PT_gpencil_guide",
                        text="Guides",
                    )

            layout.separator_spacer()
        elif not show_region_tool_header:
            # Transform settings depending on tool header visibility
            VIEW3D_HT_header.draw_xform_template(layout, context)

            layout.separator_spacer()

        # Viewport Settings
        layout.popover(
            panel="VIEW3D_PT_object_type_visibility",
            icon_value=view.icon_from_show_object_viewport,
            text="",
        )

        # Gizmo toggle & popover.
        row = layout.row(align=True)
        # FIXME: place-holder icon.
        row.prop(view, "show_gizmo", text="", toggle=True, icon='GIZMO')
        sub = row.row(align=True)
        sub.active = view.show_gizmo
        sub.popover(
            panel="VIEW3D_PT_gizmo_display",
            text="",
        )

        # Overlay toggle & popover.
        row = layout.row(align=True)
        row.prop(overlay, "show_overlays", icon='OVERLAY', text="")
        sub = row.row(align=True)
        sub.active = overlay.show_overlays
        sub.popover(panel="VIEW3D_PT_overlay", text="")

        row = layout.row()
        row.active = (object_mode == 'EDIT') or (shading.type in {'WIREFRAME', 'SOLID'})

        if shading.type == 'WIREFRAME':
            row.prop(shading, "show_xray_wireframe", text="", icon='XRAY')
        else:
            row.prop(shading, "show_xray", text="", icon='XRAY')

        row = layout.row(align=True)
        row.prop(shading, "type", text="", expand=True)
        sub = row.row(align=True)
        # TODO, currently render shading type ignores mesh two-side, until it's supported
        # show the shading popover which shows double-sided option.

        # sub.enabled = shading.type != 'RENDERED'
        sub.popover(panel="VIEW3D_PT_shading", text="")


class VIEW3D_MT_editor_menus(Menu):
    bl_label = ""

    def draw(self, context):
        layout = self.layout
        obj = context.active_object
        mode_string = context.mode
        edit_object = context.edit_object
        gp_edit = obj and obj.mode in {'EDIT_GPENCIL', 'PAINT_GPENCIL', 'SCULPT_GPENCIL', 'WEIGHT_GPENCIL'}

        layout.menu("VIEW3D_MT_view")

        # Select Menu
        if gp_edit:
            if mode_string not in {'PAINT_GPENCIL', 'WEIGHT_GPENCIL'}:
                layout.menu("VIEW3D_MT_select_gpencil")
        elif mode_string in {'PAINT_WEIGHT', 'PAINT_VERTEX', 'PAINT_TEXTURE'}:
            mesh = obj.data
            if mesh.use_paint_mask:
                layout.menu("VIEW3D_MT_select_paint_mask")
            elif mesh.use_paint_mask_vertex and mode_string in {'PAINT_WEIGHT', 'PAINT_VERTEX'}:
                layout.menu("VIEW3D_MT_select_paint_mask_vertex")
        elif mode_string != 'SCULPT':
            layout.menu("VIEW3D_MT_select_%s" % mode_string.lower())

        if gp_edit:
            pass
        elif mode_string == 'OBJECT':
            layout.menu("VIEW3D_MT_add", text="Add", text_ctxt=i18n_contexts.operator_default)
        elif mode_string == 'EDIT_MESH':
            layout.menu("VIEW3D_MT_mesh_add", text="Add", text_ctxt=i18n_contexts.operator_default)
        elif mode_string == 'EDIT_CURVE':
            layout.menu("VIEW3D_MT_curve_add", text="Add", text_ctxt=i18n_contexts.operator_default)
        elif mode_string == 'EDIT_SURFACE':
            layout.menu("VIEW3D_MT_surface_add", text="Add", text_ctxt=i18n_contexts.operator_default)
        elif mode_string == 'EDIT_METABALL':
            layout.menu("VIEW3D_MT_metaball_add", text="Add", text_ctxt=i18n_contexts.operator_default)
        elif mode_string == 'EDIT_ARMATURE':
            layout.menu("TOPBAR_MT_edit_armature_add", text="Add", text_ctxt=i18n_contexts.operator_default)

        if gp_edit:
            if obj and obj.mode == 'PAINT_GPENCIL':
                layout.menu("VIEW3D_MT_paint_gpencil")
            elif obj and obj.mode == 'EDIT_GPENCIL':
                layout.menu("VIEW3D_MT_edit_gpencil")
            elif obj and obj.mode == 'WEIGHT_GPENCIL':
                layout.menu("VIEW3D_MT_weight_gpencil")

        elif edit_object:
            layout.menu("VIEW3D_MT_edit_%s" % edit_object.type.lower())

            if mode_string == 'EDIT_MESH':
                layout.menu("VIEW3D_MT_edit_mesh_vertices")
                layout.menu("VIEW3D_MT_edit_mesh_edges")
                layout.menu("VIEW3D_MT_edit_mesh_faces")
                layout.menu("VIEW3D_MT_uv_map", text="UV")
            elif mode_string in {'EDIT_CURVE', 'EDIT_SURFACE'}:
                layout.menu("VIEW3D_MT_edit_curve_ctrlpoints")
                layout.menu("VIEW3D_MT_edit_curve_segments")

        elif obj:
            if mode_string != 'PAINT_TEXTURE':
                layout.menu("VIEW3D_MT_%s" % mode_string.lower())
            if mode_string in {'SCULPT', 'PAINT_VERTEX', 'PAINT_WEIGHT', 'PAINT_TEXTURE'}:
                layout.menu("VIEW3D_MT_brush")
            if mode_string == 'SCULPT':
                layout.menu("VIEW3D_MT_hide_mask")
        else:
            layout.menu("VIEW3D_MT_object")


# ********** Menu **********


# ********** Utilities **********


class ShowHideMenu:
    bl_label = "Show/Hide"
    _operator_name = ""

    def draw(self, _context):
        layout = self.layout

        layout.operator("%s.reveal" % self._operator_name)
        layout.operator("%s.hide" % self._operator_name, text="Hide Selected").unselected = False
        layout.operator("%s.hide" % self._operator_name, text="Hide Unselected").unselected = True


# Standard transforms which apply to all cases
# NOTE: this doesn't seem to be able to be used directly
class VIEW3D_MT_transform_base(Menu):
    bl_label = "Transform"
    bl_category = "View"

    # TODO: get rid of the custom text strings?
    def draw(self, context):
        layout = self.layout

        layout.operator("transform.translate")
        layout.operator("transform.rotate")
        layout.operator("transform.resize", text="Scale")

        layout.separator()

        layout.operator("transform.tosphere", text="To Sphere")
        layout.operator("transform.shear", text="Shear")
        layout.operator("transform.bend", text="Bend")
        layout.operator("transform.push_pull", text="Push/Pull")

        if context.mode != 'OBJECT':
            layout.operator("transform.vertex_warp", text="Warp")
            layout.operator("transform.vertex_random", text="Randomize")


# Generic transform menu - geometry types
class VIEW3D_MT_transform(VIEW3D_MT_transform_base):
    def draw(self, context):
        # base menu
        VIEW3D_MT_transform_base.draw(self, context)

        # generic...
        layout = self.layout
        if context.mode == 'EDIT_MESH':
            layout.operator("transform.shrink_fatten", text="Shrink Fatten")
        elif context.mode == 'EDIT_CURVE':
            layout.operator("transform.transform", text="Radius").mode = 'CURVE_SHRINKFATTEN'

        layout.separator()

        layout.operator("transform.translate", text="Move Texture Space").texture_space = True
        layout.operator("transform.resize", text="Scale Texture Space").texture_space = True


# Object-specific extensions to Transform menu
class VIEW3D_MT_transform_object(VIEW3D_MT_transform_base):
    def draw(self, context):
        layout = self.layout

        # base menu
        VIEW3D_MT_transform_base.draw(self, context)

        # object-specific option follow...
        layout.separator()

        layout.operator("transform.translate", text="Move Texture Space").texture_space = True
        layout.operator("transform.resize", text="Scale Texture Space").texture_space = True

        layout.separator()

        layout.operator_context = 'EXEC_REGION_WIN'
        # XXX see alignmenu() in edit.c of b2.4x to get this working
        layout.operator("transform.transform", text="Align to Transform Orientation").mode = 'ALIGN'

        layout.separator()

        layout.operator("object.randomize_transform")
        layout.operator("object.align")

        # TODO: there is a strange context bug here.
        """
        layout.operator_context = 'INVOKE_REGION_WIN'
        layout.operator("object.transform_axis_target")
        """


# Armature EditMode extensions to Transform menu
class VIEW3D_MT_transform_armature(VIEW3D_MT_transform_base):
    def draw(self, context):
        layout = self.layout

        # base menu
        VIEW3D_MT_transform_base.draw(self, context)

        # armature specific extensions follow...
        obj = context.object
        if obj.type == 'ARMATURE' and obj.mode in {'EDIT', 'POSE'}:
            if obj.data.display_type == 'BBONE':
                layout.separator()

                layout.operator("transform.transform", text="Scale BBone").mode = 'BONE_SIZE'
            elif obj.data.display_type == 'ENVELOPE':
                layout.separator()

                layout.operator("transform.transform", text="Scale Envelope Distance").mode = 'BONE_SIZE'
                layout.operator("transform.transform", text="Scale Radius").mode = 'BONE_ENVELOPE'

        if context.edit_object and context.edit_object.type == 'ARMATURE':
            layout.separator()

            layout.operator("armature.align")


class VIEW3D_MT_mirror(Menu):
    bl_label = "Mirror"

    def draw(self, _context):
        layout = self.layout

        layout.operator("transform.mirror", text="Interactive Mirror")

        layout.separator()

        layout.operator_context = 'EXEC_REGION_WIN'

        for (space_name, space_id) in (("Global", 'GLOBAL'), ("Local", 'LOCAL')):
            for axis_index, axis_name in enumerate("XYZ"):
                props = layout.operator("transform.mirror", text=f"{axis_name!s} {space_name!s}")
                props.constraint_axis[axis_index] = True
                props.orient_type = 'GLOBAL'

            if space_id == 'GLOBAL':
                layout.separator()


class VIEW3D_MT_snap(Menu):
    bl_label = "Snap"

    def draw(self, _context):
        layout = self.layout

        layout.operator("view3d.snap_selected_to_grid", text="Selection to Grid")
        layout.operator("view3d.snap_selected_to_cursor", text="Selection to Cursor").use_offset = False
        layout.operator("view3d.snap_selected_to_cursor", text="Selection to Cursor (Keep Offset)").use_offset = True
        layout.operator("view3d.snap_selected_to_active", text="Selection to Active")

        layout.separator()

        layout.operator("view3d.snap_cursor_to_selected", text="Cursor to Selected")
        layout.operator("view3d.snap_cursor_to_center", text="Cursor to World Origin")
        layout.operator("view3d.snap_cursor_to_grid", text="Cursor to Grid")
        layout.operator("view3d.snap_cursor_to_active", text="Cursor to Active")


class VIEW3D_MT_uv_map(Menu):
    bl_label = "UV Mapping"

    def draw(self, context):
        layout = self.layout

        tool_settings = context.tool_settings

        layout.operator("uv.unwrap")
        layout.prop(tool_settings, "use_edge_path_live_unwrap")

        layout.separator()

        layout.operator_context = 'INVOKE_DEFAULT'
        layout.operator("uv.smart_project")
        layout.operator("uv.lightmap_pack")
        layout.operator("uv.follow_active_quads")

        layout.separator()

        layout.operator_context = 'EXEC_REGION_WIN'
        layout.operator("uv.cube_project")
        layout.operator("uv.cylinder_project")
        layout.operator("uv.sphere_project")

        layout.separator()

        layout.operator_context = 'INVOKE_REGION_WIN'
        layout.operator("uv.project_from_view").scale_to_bounds = False
        layout.operator("uv.project_from_view", text="Project from View (Bounds)").scale_to_bounds = True

        layout.separator()

        layout.operator("mesh.mark_seam").clear = False
        layout.operator("mesh.mark_seam", text="Clear Seam").clear = True

        layout.separator()

        layout.operator("uv.reset")


# ********** View menus **********


class VIEW3D_MT_view(Menu):
    bl_label = "View"

    def draw(self, context):
        layout = self.layout
        view = context.space_data

        layout.prop(view, "show_region_toolbar")
        layout.prop(view, "show_region_ui")
        layout.prop(view, "show_region_tool_header")
        layout.prop(view, "show_region_hud")

        layout.separator()

        layout.operator("view3d.view_selected", text="Frame Selected").use_all_regions = False
        if view.region_quadviews:
            layout.operator("view3d.view_selected", text="Frame Selected (Quad View)").use_all_regions = True

        layout.operator("view3d.view_all", text="Frame All").center = False
        layout.operator("view3d.view_persportho", text="Perspective/Orthographic")
        layout.menu("VIEW3D_MT_view_local")

        layout.separator()

        layout.menu("VIEW3D_MT_view_cameras", text="Cameras")

        layout.separator()
        layout.menu("VIEW3D_MT_view_viewpoint")
        layout.menu("VIEW3D_MT_view_navigation")
        layout.menu("VIEW3D_MT_view_align")

        layout.separator()

        layout.operator_context = 'INVOKE_REGION_WIN'
        layout.menu("VIEW3D_MT_view_regions", text="View Regions")

        layout.separator()

        layout.operator("screen.animation_play", text="Play Animation")

        layout.separator()

        layout.operator("render.opengl", text="Viewport Render Image", icon='RENDER_STILL')
        layout.operator("render.opengl", text="Viewport Render Animation", icon='RENDER_ANIMATION').animation = True

        layout.separator()

        layout.menu("INFO_MT_area")


class VIEW3D_MT_view_local(Menu):
    bl_label = "Local View"

    def draw(self, _context):
        layout = self.layout

        layout.operator("view3d.localview", text="Toggle Local View")
        layout.operator("view3d.localview_remove_from")


class VIEW3D_MT_view_cameras(Menu):
    bl_label = "Cameras"

    def draw(self, _context):
        layout = self.layout

        layout.operator("view3d.object_as_camera")
        layout.operator("view3d.view_camera", text="Active Camera")


class VIEW3D_MT_view_viewpoint(Menu):
    bl_label = "Viewpoint"

    def draw(self, _context):
        layout = self.layout

        layout.operator("view3d.view_camera", text="Camera")

        layout.separator()

        layout.operator("view3d.view_axis", text="Top").type = 'TOP'
        layout.operator("view3d.view_axis", text="Bottom").type = 'BOTTOM'

        layout.separator()

        layout.operator("view3d.view_axis", text="Front").type = 'FRONT'
        layout.operator("view3d.view_axis", text="Back").type = 'BACK'

        layout.separator()

        layout.operator("view3d.view_axis", text="Right").type = 'RIGHT'
        layout.operator("view3d.view_axis", text="Left").type = 'LEFT'


class VIEW3D_MT_view_navigation(Menu):
    bl_label = "Navigation"

    def draw(self, _context):
        from math import pi
        layout = self.layout

        layout.operator_enum("view3d.view_orbit", "type")
        props = layout.operator("view3d.view_orbit", text="Orbit Opposite")
        props.type = 'ORBITRIGHT'
        props.angle = pi

        layout.separator()

        layout.operator("view3d.view_roll", text="Roll Left").type = 'LEFT'
        layout.operator("view3d.view_roll", text="Roll Right").type = 'RIGHT'

        layout.separator()

        layout.operator_enum("view3d.view_pan", "type")

        layout.separator()

        layout.operator("view3d.zoom", text="Zoom In").delta = 1
        layout.operator("view3d.zoom", text="Zoom Out").delta = -1
        layout.operator("view3d.zoom_border", text="Zoom Region...")
        layout.operator("view3d.zoom_camera_1_to_1", text="Zoom Camera 1:1")

        layout.separator()

        layout.operator("view3d.fly")
        layout.operator("view3d.walk")


class VIEW3D_MT_view_align(Menu):
    bl_label = "Align View"

    def draw(self, _context):
        layout = self.layout

        layout.menu("VIEW3D_MT_view_align_selected")

        layout.separator()

        layout.operator("view3d.camera_to_view", text="Align Active Camera to View")
        layout.operator("view3d.camera_to_view_selected", text="Align Active Camera to Selected")

        layout.separator()

        layout.operator("view3d.view_all", text="Center Cursor and View All").center = True
        layout.operator("view3d.view_center_cursor")

        layout.separator()

        layout.operator("view3d.view_lock_to_active")
        layout.operator("view3d.view_lock_clear")


class VIEW3D_MT_view_align_selected(Menu):
    bl_label = "Align View to Active"

    def draw(self, _context):
        layout = self.layout

        props = layout.operator("view3d.view_axis", text="Top")
        props.align_active = True
        props.type = 'TOP'

        props = layout.operator("view3d.view_axis", text="Bottom")
        props.align_active = True
        props.type = 'BOTTOM'

        layout.separator()

        props = layout.operator("view3d.view_axis", text="Front")
        props.align_active = True
        props.type = 'FRONT'

        props = layout.operator("view3d.view_axis", text="Back")
        props.align_active = True
        props.type = 'BACK'

        layout.separator()

        props = layout.operator("view3d.view_axis", text="Right")
        props.align_active = True
        props.type = 'RIGHT'

        props = layout.operator("view3d.view_axis", text="Left")
        props.align_active = True
        props.type = 'LEFT'


class VIEW3D_MT_view_regions(Menu):
    bl_label = "View Regions"

    def draw(self, _context):
        layout = self.layout
        layout.operator("view3d.clip_border", text="Clipping Region...")
        layout.operator("view3d.render_border", text="Render Region...")

        layout.separator()

        layout.operator("view3d.clear_render_border")


# ********** Select menus, suffix from context.mode **********

class VIEW3D_MT_select_object_more_less(Menu):
    bl_label = "Select More/Less"

    def draw(self, _context):
        layout = self.layout

        layout = self.layout

        layout.operator("object.select_more", text="More")
        layout.operator("object.select_less", text="Less")

        layout.separator()

        props = layout.operator("object.select_hierarchy", text="Parent")
        props.extend = False
        props.direction = 'PARENT'

        props = layout.operator("object.select_hierarchy", text="Child")
        props.extend = False
        props.direction = 'CHILD'

        layout.separator()

        props = layout.operator("object.select_hierarchy", text="Extend Parent")
        props.extend = True
        props.direction = 'PARENT'

        props = layout.operator("object.select_hierarchy", text="Extend Child")
        props.extend = True
        props.direction = 'CHILD'


class VIEW3D_MT_select_object(Menu):
    bl_label = "Select"

    def draw(self, _context):
        layout = self.layout

        layout.operator("object.select_all", text="All").action = 'SELECT'
        layout.operator("object.select_all", text="None").action = 'DESELECT'
        layout.operator("object.select_all", text="Invert").action = 'INVERT'

        layout.separator()

        layout.operator("view3d.select_box")
        layout.operator("view3d.select_circle")

        layout.separator()

        layout.operator_menu_enum("object.select_by_type", "type", text="Select All by Type...")
        layout.operator("object.select_camera", text="Select Active Camera")
        layout.operator("object.select_mirror", text="Mirror Selection")
        layout.operator("object.select_random", text="Select Random")

        layout.separator()

        layout.menu("VIEW3D_MT_select_object_more_less")

        layout.separator()

        layout.operator_menu_enum("object.select_grouped", "type", text="Select Grouped")
        layout.operator_menu_enum("object.select_linked", "type", text="Select Linked")
        layout.operator("object.select_pattern", text="Select Pattern...")


class VIEW3D_MT_select_pose_more_less(Menu):
    bl_label = "Select More/Less"

    def draw(self, _context):
        layout = self.layout

        layout = self.layout

        props = layout.operator("pose.select_hierarchy", text="Parent")
        props.extend = False
        props.direction = 'PARENT'

        props = layout.operator("pose.select_hierarchy", text="Child")
        props.extend = False
        props.direction = 'CHILD'

        layout.separator()

        props = layout.operator("pose.select_hierarchy", text="Extend Parent")
        props.extend = True
        props.direction = 'PARENT'

        props = layout.operator("pose.select_hierarchy", text="Extend Child")
        props.extend = True
        props.direction = 'CHILD'


class VIEW3D_MT_select_pose(Menu):
    bl_label = "Select"

    def draw(self, _context):
        layout = self.layout

        layout.operator("pose.select_all", text="All").action = 'SELECT'
        layout.operator("pose.select_all", text="None").action = 'DESELECT'
        layout.operator("pose.select_all", text="Invert").action = 'INVERT'

        layout.separator()

        layout.operator("view3d.select_box")
        layout.operator("view3d.select_circle")

        layout.separator()

        layout.operator("pose.select_mirror", text="Flip Active")

        layout.separator()

        layout.operator("pose.select_constraint_target", text="Constraint Target")
        layout.operator("pose.select_linked", text="Linked")

        layout.separator()

        layout.menu("VIEW3D_MT_select_pose_more_less")

        layout.separator()

        layout.operator_menu_enum("pose.select_grouped", "type", text="Grouped")
        layout.operator("object.select_pattern", text="Select Pattern...")


class VIEW3D_MT_select_particle(Menu):
    bl_label = "Select"

    def draw(self, _context):
        layout = self.layout

        layout.operator("particle.select_all", text="All").action = 'SELECT'
        layout.operator("particle.select_all", text="None").action = 'DESELECT'
        layout.operator("particle.select_all", text="Invert").action = 'INVERT'

        layout.separator()

        layout.operator("view3d.select_box")
        layout.operator("view3d.select_circle")

        layout.separator()

        layout.operator("particle.select_linked")

        layout.separator()

        layout.operator("particle.select_more")
        layout.operator("particle.select_less")

        layout.separator()

        layout.operator("particle.select_random")

        layout.separator()

        layout.operator("particle.select_roots", text="Roots")
        layout.operator("particle.select_tips", text="Tips")


class VIEW3D_MT_edit_mesh_select_similar(Menu):
    bl_label = "Select Similar"

    def draw(self, _context):
        layout = self.layout

        layout.operator_enum("mesh.select_similar", "type")

        layout.separator()

        layout.operator("mesh.select_similar_region", text="Face Regions")


class VIEW3D_MT_edit_mesh_select_by_trait(Menu):
    bl_label = "Select All by Trait"

    def draw(self, context):
        layout = self.layout
        tool_settings = context.tool_settings
        if tool_settings.mesh_select_mode[2] is False:
            layout.operator("mesh.select_non_manifold", text="Non Manifold")
        layout.operator("mesh.select_loose", text="Loose Geometry")
        layout.operator("mesh.select_interior_faces", text="Interior Faces")
        layout.operator("mesh.select_face_by_sides", text="Faces by Sides")

        layout.separator()

        layout.operator("mesh.select_ungrouped", text="Ungrouped Verts")


class VIEW3D_MT_edit_mesh_select_more_less(Menu):
    bl_label = "Select More/Less"

    def draw(self, _context):
        layout = self.layout

        layout.operator("mesh.select_more", text="More")
        layout.operator("mesh.select_less", text="Less")

        layout.separator()

        layout.operator("mesh.select_next_item", text="Next Active")
        layout.operator("mesh.select_prev_item", text="Previous Active")


class VIEW3D_MT_edit_mesh_select_linked(Menu):
    bl_label = "Select Linked"

    def draw(self, _context):
        layout = self.layout

        layout.operator("mesh.select_linked", text="Linked")
        layout.operator("mesh.shortest_path_select", text="Shortest Path")
        layout.operator("mesh.faces_select_linked_flat", text="Linked Flat Faces")


class VIEW3D_MT_edit_mesh_select_loops(Menu):
    bl_label = "Select Loops"

    def draw(self, _context):
        layout = self.layout

        layout.operator("mesh.loop_multi_select", text="Edge Loops").ring = False
        layout.operator("mesh.loop_multi_select", text="Edge Rings").ring = True

        layout.separator()

        layout.operator("mesh.loop_to_region")
        layout.operator("mesh.region_to_loop")


class VIEW3D_MT_select_edit_mesh(Menu):
    bl_label = "Select"

    def draw(self, _context):
        layout = self.layout

        # primitive
        layout.operator("mesh.select_all", text="All").action = 'SELECT'
        layout.operator("mesh.select_all", text="None").action = 'DESELECT'
        layout.operator("mesh.select_all", text="Invert").action = 'INVERT'

        layout.separator()

        layout.operator("view3d.select_box")
        layout.operator("view3d.select_circle")

        layout.separator()

        # numeric
        layout.operator("mesh.select_random", text="Select Random")
        layout.operator("mesh.select_nth")

        layout.separator()

        # geometric
        layout.operator("mesh.edges_select_sharp", text="Select Sharp Edges")

        layout.separator()

        # other ...
        layout.menu("VIEW3D_MT_edit_mesh_select_similar")

        layout.separator()

        layout.menu("VIEW3D_MT_edit_mesh_select_by_trait")

        layout.separator()

        layout.menu("VIEW3D_MT_edit_mesh_select_more_less")

        layout.separator()

        layout.menu("VIEW3D_MT_edit_mesh_select_loops")

        layout.separator()

        layout.menu("VIEW3D_MT_edit_mesh_select_linked")

        layout.separator()

        layout.operator("mesh.select_axis", text="Side of Active")
        layout.operator("mesh.select_mirror", text="Mirror Selection")


class VIEW3D_MT_select_edit_curve(Menu):
    bl_label = "Select"

    def draw(self, _context):
        layout = self.layout

        layout.operator("curve.select_all", text="All").action = 'SELECT'
        layout.operator("curve.select_all", text="None").action = 'DESELECT'
        layout.operator("curve.select_all", text="Invert").action = 'INVERT'

        layout.separator()

        layout.operator("view3d.select_box")
        layout.operator("view3d.select_circle")

        layout.separator()

        layout.operator("curve.select_random")
        layout.operator("curve.select_nth")
        layout.operator("curve.select_linked", text="Select Linked")
        layout.operator("curve.select_similar", text="Select Similar")

        layout.separator()

        layout.operator("curve.de_select_first")
        layout.operator("curve.de_select_last")
        layout.operator("curve.select_next")
        layout.operator("curve.select_previous")

        layout.separator()

        layout.operator("curve.select_more")
        layout.operator("curve.select_less")


class VIEW3D_MT_select_edit_surface(Menu):
    bl_label = "Select"

    def draw(self, _context):
        layout = self.layout

        layout.operator("curve.select_all", text="All").action = 'SELECT'
        layout.operator("curve.select_all", text="None").action = 'DESELECT'
        layout.operator("curve.select_all", text="Invert").action = 'INVERT'

        layout.separator()

        layout.operator("view3d.select_box")
        layout.operator("view3d.select_circle")

        layout.separator()

        layout.operator("curve.select_random")
        layout.operator("curve.select_nth")
        layout.operator("curve.select_linked", text="Select Linked")
        layout.operator("curve.select_similar", text="Select Similar")

        layout.separator()

        layout.operator("curve.select_row")

        layout.separator()

        layout.operator("curve.select_more")
        layout.operator("curve.select_less")


class VIEW3D_MT_edit_text_context_menu(Menu):
    bl_label = "Text Context Menu"

    def draw(self, _context):
        layout = self.layout

        layout.operator_context = 'INVOKE_DEFAULT'

        layout.operator("font.text_cut", text="Cut")
        layout.operator("font.text_copy", text="Copy", icon='COPYDOWN')
        layout.operator("font.text_paste", text="Paste", icon='PASTEDOWN')

        layout.separator()

        layout.operator("font.select_all")

        layout.separator()

        layout.menu("VIEW3D_MT_edit_font")


class VIEW3D_MT_select_edit_text(Menu):
    # intentional name mismatch
    # select menu for 3d-text doesn't make sense
    bl_label = "Edit"

    def draw(self, _context):
        layout = self.layout

        layout.operator("ed.undo")
        layout.operator("ed.redo")

        layout.separator()

        layout.operator("font.text_cut", text="Cut")
        layout.operator("font.text_copy", text="Copy", icon='COPYDOWN')
        layout.operator("font.text_paste", text="Paste", icon='PASTEDOWN')

        layout.separator()

        layout.operator("font.text_paste_from_file")

        layout.separator()

        layout.operator("font.select_all")

        layout.separator()

        layout.operator("font.case_set", text="To Uppercase").case = 'UPPER'
        layout.operator("font.case_set", text="To Lowercase").case = 'LOWER'

        layout.separator()

        layout.menu("VIEW3D_MT_edit_text_chars")


class VIEW3D_MT_select_edit_metaball(Menu):
    bl_label = "Select"

    def draw(self, _context):
        layout = self.layout

        layout.operator("mball.select_all", text="All").action = 'SELECT'
        layout.operator("mball.select_all", text="None").action = 'DESELECT'
        layout.operator("mball.select_all", text="Invert").action = 'INVERT'

        layout.separator()

        layout.operator("view3d.select_box")
        layout.operator("view3d.select_circle")

        layout.separator()

        layout.operator("mball.select_random_metaelems")

        layout.separator()

        layout.operator_menu_enum("mball.select_similar", "type", text="Similar")


class VIEW3D_MT_edit_lattice_context_menu(Menu):
    bl_label = "Lattice Context Menu"

    def draw(self, _context):
        layout = self.layout

        layout = self.layout

        layout.menu("VIEW3D_MT_mirror")
        layout.operator_menu_enum("lattice.flip", "axis")
        layout.menu("VIEW3D_MT_snap")

        layout.separator()

        layout.operator("lattice.make_regular")


class VIEW3D_MT_select_edit_lattice(Menu):
    bl_label = "Select"

    def draw(self, _context):
        layout = self.layout

        layout.operator("lattice.select_all", text="All").action = 'SELECT'
        layout.operator("lattice.select_all", text="None").action = 'DESELECT'
        layout.operator("lattice.select_all", text="Invert").action = 'INVERT'

        layout.separator()

        layout.operator("view3d.select_box")
        layout.operator("view3d.select_circle")

        layout.separator()

        layout.operator("lattice.select_mirror")
        layout.operator("lattice.select_random")

        layout.separator()

        layout.operator("lattice.select_more")
        layout.operator("lattice.select_less")

        layout.separator()

        layout.operator("lattice.select_ungrouped", text="Ungrouped Verts")


class VIEW3D_MT_select_edit_armature(Menu):
    bl_label = "Select"

    def draw(self, _context):
        layout = self.layout

        layout.operator("armature.select_all", text="All").action = 'SELECT'
        layout.operator("armature.select_all", text="None").action = 'DESELECT'
        layout.operator("armature.select_all", text="Invert").action = 'INVERT'

        layout.separator()

        layout.operator("view3d.select_box")
        layout.operator("view3d.select_circle")

        layout.separator()

        layout.operator("armature.select_mirror", text="Mirror").extend = False

        layout.separator()

        layout.operator("armature.select_more", text="More")
        layout.operator("armature.select_less", text="Less")

        layout.separator()

        props = layout.operator("armature.select_hierarchy", text="Parent")
        props.extend = False
        props.direction = 'PARENT'

        props = layout.operator("armature.select_hierarchy", text="Child")
        props.extend = False
        props.direction = 'CHILD'

        layout.separator()

        props = layout.operator("armature.select_hierarchy", text="Extend Parent")
        props.extend = True
        props.direction = 'PARENT'

        props = layout.operator("armature.select_hierarchy", text="Extend Child")
        props.extend = True
        props.direction = 'CHILD'

        layout.operator_menu_enum("armature.select_similar", "type", text="Similar")
        layout.operator("object.select_pattern", text="Select Pattern...")


class VIEW3D_MT_select_gpencil(Menu):
    bl_label = "Select"

    def draw(self, _context):
        layout = self.layout

        layout.operator("gpencil.select_all", text="All").action = 'SELECT'
        layout.operator("gpencil.select_all", text="None").action = 'DESELECT'
        layout.operator("gpencil.select_all", text="Invert").action = 'INVERT'

        layout.separator()

        layout.operator("gpencil.select_box")
        layout.operator("gpencil.select_circle")

        layout.separator()

        layout.operator("gpencil.select_linked", text="Linked")
        layout.operator("gpencil.select_alternate")
        layout.operator_menu_enum("gpencil.select_grouped", "type", text="Grouped")

        layout.separator()

        layout.operator("gpencil.select_first")
        layout.operator("gpencil.select_last")

        layout.separator()

        layout.operator("gpencil.select_more")
        layout.operator("gpencil.select_less")


class VIEW3D_MT_select_paint_mask(Menu):
    bl_label = "Select"

    def draw(self, _context):
        layout = self.layout

        layout.operator("paint.face_select_all", text="All").action = 'SELECT'
        layout.operator("paint.face_select_all", text="None").action = 'DESELECT'
        layout.operator("paint.face_select_all", text="Invert").action = 'INVERT'

        layout.separator()

        layout.operator("view3d.select_box")
        layout.operator("view3d.select_circle")

        layout.separator()

        layout.operator("paint.face_select_linked", text="Linked")


class VIEW3D_MT_select_paint_mask_vertex(Menu):
    bl_label = "Select"

    def draw(self, _context):
        layout = self.layout

        layout.operator("paint.vert_select_all", text="All").action = 'SELECT'
        layout.operator("paint.vert_select_all", text="None").action = 'DESELECT'
        layout.operator("paint.vert_select_all", text="Invert").action = 'INVERT'

        layout.separator()

        layout.operator("view3d.select_box")
        layout.operator("view3d.select_circle")

        layout.separator()

        layout.operator("paint.vert_select_ungrouped", text="Ungrouped Verts")


class VIEW3D_MT_angle_control(Menu):
    bl_label = "Angle Control"

    @classmethod
    def poll(cls, context):
        settings = UnifiedPaintPanel.paint_settings(context)
        if not settings:
            return False

        brush = settings.brush
        tex_slot = brush.texture_slot

        return tex_slot.has_texture_angle and tex_slot.has_texture_angle_source

    def draw(self, context):
        layout = self.layout

        settings = UnifiedPaintPanel.paint_settings(context)
        brush = settings.brush

        sculpt = (context.sculpt_object is not None)

        tex_slot = brush.texture_slot

        layout.prop(tex_slot, "use_rake", text="Rake")

        if brush.brush_capabilities.has_random_texture_angle and tex_slot.has_random_texture_angle:
            if sculpt:
                if brush.sculpt_capabilities.has_random_texture_angle:
                    layout.prop(tex_slot, "use_random", text="Random")
            else:
                layout.prop(tex_slot, "use_random", text="Random")


class VIEW3D_MT_mesh_add(Menu):
    bl_idname = "VIEW3D_MT_mesh_add"
    bl_label = "Mesh"

    def draw(self, _context):
        layout = self.layout

        layout.operator_context = 'INVOKE_REGION_WIN'

        layout.operator("mesh.primitive_plane_add", text="Plane", icon='MESH_PLANE')
        layout.operator("mesh.primitive_cube_add", text="Cube", icon='MESH_CUBE')
        layout.operator("mesh.primitive_circle_add", text="Circle", icon='MESH_CIRCLE')
        layout.operator("mesh.primitive_uv_sphere_add", text="UV Sphere", icon='MESH_UVSPHERE')
        layout.operator("mesh.primitive_ico_sphere_add", text="Ico Sphere", icon='MESH_ICOSPHERE')
        layout.operator("mesh.primitive_cylinder_add", text="Cylinder", icon='MESH_CYLINDER')
        layout.operator("mesh.primitive_cone_add", text="Cone", icon='MESH_CONE')
        layout.operator("mesh.primitive_torus_add", text="Torus", icon='MESH_TORUS')

        layout.separator()

        layout.operator("mesh.primitive_grid_add", text="Grid", icon='MESH_GRID')
        layout.operator("mesh.primitive_monkey_add", text="Monkey", icon='MESH_MONKEY')


class VIEW3D_MT_curve_add(Menu):
    bl_idname = "VIEW3D_MT_curve_add"
    bl_label = "Curve"

    def draw(self, _context):
        layout = self.layout

        layout.operator_context = 'INVOKE_REGION_WIN'

        layout.operator("curve.primitive_bezier_curve_add", text="Bezier", icon='CURVE_BEZCURVE')
        layout.operator("curve.primitive_bezier_circle_add", text="Circle", icon='CURVE_BEZCIRCLE')

        layout.separator()

        layout.operator("curve.primitive_nurbs_curve_add", text="Nurbs Curve", icon='CURVE_NCURVE')
        layout.operator("curve.primitive_nurbs_circle_add", text="Nurbs Circle", icon='CURVE_NCIRCLE')
        layout.operator("curve.primitive_nurbs_path_add", text="Path", icon='CURVE_PATH')


class VIEW3D_MT_surface_add(Menu):
    bl_idname = "VIEW3D_MT_surface_add"
    bl_label = "Surface"

    def draw(self, _context):
        layout = self.layout

        layout.operator_context = 'INVOKE_REGION_WIN'

        layout.operator("surface.primitive_nurbs_surface_curve_add", text="Nurbs Curve", icon='SURFACE_NCURVE')
        layout.operator("surface.primitive_nurbs_surface_circle_add", text="Nurbs Circle", icon='SURFACE_NCIRCLE')
        layout.operator("surface.primitive_nurbs_surface_surface_add", text="Nurbs Surface", icon='SURFACE_NSURFACE')
        layout.operator("surface.primitive_nurbs_surface_cylinder_add",
                        text="Nurbs Cylinder", icon='SURFACE_NCYLINDER')
        layout.operator("surface.primitive_nurbs_surface_sphere_add", text="Nurbs Sphere", icon='SURFACE_NSPHERE')
        layout.operator("surface.primitive_nurbs_surface_torus_add", text="Nurbs Torus", icon='SURFACE_NTORUS')


class VIEW3D_MT_edit_metaball_context_menu(Menu):
    bl_label = "Metaball Context Menu"

    def draw(self, _context):
        layout = self.layout

        layout.operator_context = 'INVOKE_REGION_WIN'

        # Add
        layout.operator("mball.duplicate_move")

        layout.separator()

        # Modify
        layout.menu("VIEW3D_MT_mirror")
        layout.menu("VIEW3D_MT_snap")

        layout.separator()

        # Remove
        layout.operator_context = 'EXEC_DEFAULT'
        layout.operator("mball.delete_metaelems", text="Delete")


class VIEW3D_MT_metaball_add(Menu):
    bl_idname = "VIEW3D_MT_metaball_add"
    bl_label = "Metaball"

    def draw(self, _context):
        layout = self.layout

        layout.operator_context = 'INVOKE_REGION_WIN'
        layout.operator_enum("object.metaball_add", "type")


class TOPBAR_MT_edit_curve_add(Menu):
    bl_idname = "TOPBAR_MT_edit_curve_add"
    bl_label = "Add"
    bl_translation_context = i18n_contexts.operator_default

    def draw(self, context):
        is_surf = context.active_object.type == 'SURFACE'

        layout = self.layout
        layout.operator_context = 'EXEC_REGION_WIN'

        if is_surf:
            VIEW3D_MT_surface_add.draw(self, context)
        else:
            VIEW3D_MT_curve_add.draw(self, context)


class TOPBAR_MT_edit_armature_add(Menu):
    bl_idname = "TOPBAR_MT_edit_armature_add"
    bl_label = "Armature"

    def draw(self, _context):
        layout = self.layout

        layout.operator_context = 'EXEC_REGION_WIN'
        layout.operator("armature.bone_primitive_add", text="Single Bone", icon='BONE_DATA')


class VIEW3D_MT_armature_add(Menu):
    bl_idname = "VIEW3D_MT_armature_add"
    bl_label = "Armature"

    def draw(self, _context):
        layout = self.layout

        layout.operator_context = 'EXEC_REGION_WIN'
        layout.operator("object.armature_add", text="Single Bone", icon='BONE_DATA')


class VIEW3D_MT_light_add(Menu):
    bl_idname = "VIEW3D_MT_light_add"
    bl_label = "Light"

    def draw(self, _context):
        layout = self.layout

        layout.operator_context = 'INVOKE_REGION_WIN'
        layout.operator_enum("object.light_add", "type")


class VIEW3D_MT_lightprobe_add(Menu):
    bl_idname = "VIEW3D_MT_lightprobe_add"
    bl_label = "Light Probe"

    def draw(self, _context):
        layout = self.layout

        layout.operator_context = 'INVOKE_REGION_WIN'
        layout.operator_enum("object.lightprobe_add", "type")


class VIEW3D_MT_camera_add(Menu):
    bl_idname = "VIEW3D_MT_camera_add"
    bl_label = "Camera"

    def draw(self, _context):
        layout = self.layout
        layout.operator_context = 'EXEC_REGION_WIN'
        layout.operator("object.camera_add", text="Camera", icon='OUTLINER_OB_CAMERA')


class VIEW3D_MT_add(Menu):
    bl_label = "Add"
    bl_translation_context = i18n_contexts.operator_default

    def draw(self, context):
        layout = self.layout

        # note, don't use 'EXEC_SCREEN' or operators won't get the 'v3d' context.

        # Note: was EXEC_AREA, but this context does not have the 'rv3d', which prevents
        #       "align_view" to work on first call (see [#32719]).
        layout.operator_context = 'EXEC_REGION_WIN'

        # layout.operator_menu_enum("object.mesh_add", "type", text="Mesh", icon='OUTLINER_OB_MESH')
        layout.menu("VIEW3D_MT_mesh_add", icon='OUTLINER_OB_MESH')

        # layout.operator_menu_enum("object.curve_add", "type", text="Curve", icon='OUTLINER_OB_CURVE')
        layout.menu("VIEW3D_MT_curve_add", icon='OUTLINER_OB_CURVE')
        # layout.operator_menu_enum("object.surface_add", "type", text="Surface", icon='OUTLINER_OB_SURFACE')
        layout.menu("VIEW3D_MT_surface_add", icon='OUTLINER_OB_SURFACE')
        layout.menu("VIEW3D_MT_metaball_add", text="Metaball", icon='OUTLINER_OB_META')
        layout.operator("object.text_add", text="Text", icon='OUTLINER_OB_FONT')
        layout.operator_menu_enum("object.gpencil_add", "type", text="Grease Pencil", icon='OUTLINER_OB_GREASEPENCIL')

        layout.separator()

        if VIEW3D_MT_armature_add.is_extended():
            layout.menu("VIEW3D_MT_armature_add", icon='OUTLINER_OB_ARMATURE')
        else:
            layout.operator("object.armature_add", text="Armature", icon='OUTLINER_OB_ARMATURE')

        layout.operator("object.add", text="Lattice", icon='OUTLINER_OB_LATTICE').type = 'LATTICE'

        layout.separator()

        layout.operator_menu_enum("object.empty_add", "type", text="Empty", icon='OUTLINER_OB_EMPTY')
        layout.menu("VIEW3D_MT_image_add", text="Image", icon='OUTLINER_OB_IMAGE')

        layout.separator()

        layout.menu("VIEW3D_MT_light_add", icon='OUTLINER_OB_LIGHT')
        layout.menu("VIEW3D_MT_lightprobe_add", icon='OUTLINER_OB_LIGHTPROBE')

        layout.separator()

        if VIEW3D_MT_camera_add.is_extended():
            layout.menu("VIEW3D_MT_camera_add", icon='OUTLINER_OB_CAMERA')
        else:
            VIEW3D_MT_camera_add.draw(self, context)

        layout.separator()

        layout.operator("object.speaker_add", text="Speaker", icon='OUTLINER_OB_SPEAKER')

        layout.separator()

        layout.operator_menu_enum("object.effector_add", "type", text="Force Field", icon='OUTLINER_OB_FORCE_FIELD')

        layout.separator()

        has_collections = bool(bpy.data.collections)
        col = layout.column()
        col.enabled = has_collections

        if not has_collections or len(bpy.data.collections) > 10:
            col.operator_context = 'INVOKE_REGION_WIN'
            col.operator(
                "object.collection_instance_add",
                text="Collection Instance..." if has_collections else "No Collections to Instance",
                icon='OUTLINER_OB_GROUP_INSTANCE',
            )
        else:
            col.operator_menu_enum(
                "object.collection_instance_add",
                "collection",
                text="Collection Instance",
                icon='OUTLINER_OB_GROUP_INSTANCE',
            )


class VIEW3D_MT_image_add(Menu):
    bl_label = "Add Image"

    def draw(self, _context):
        layout = self.layout
        layout.operator("object.load_reference_image", text="Reference", icon='IMAGE_REFERENCE')
        layout.operator("object.load_background_image", text="Background", icon='IMAGE_BACKGROUND')


class VIEW3D_MT_object_relations(Menu):
    bl_label = "Relations"

    def draw(self, _context):
        layout = self.layout

        layout.operator("object.proxy_make", text="Make Proxy...")

        layout.operator("object.make_dupli_face")

        layout.separator()

        layout.operator_menu_enum("object.make_local", "type", text="Make Local...")
        layout.menu("VIEW3D_MT_make_single_user")

        layout.separator()

        layout.operator("object.data_transfer")
        layout.operator("object.datalayout_transfer")


class VIEW3D_MT_object(Menu):
    bl_context = "objectmode"
    bl_label = "Object"

    def draw(self, context):
        layout = self.layout

        layout.menu("VIEW3D_MT_transform_object")
        layout.operator_menu_enum("object.origin_set", text="Set Origin", property="type")
        layout.menu("VIEW3D_MT_mirror")
        layout.menu("VIEW3D_MT_object_clear")
        layout.menu("VIEW3D_MT_object_apply")
        layout.menu("VIEW3D_MT_snap")

        layout.separator()

        layout.operator("object.duplicate_move")
        layout.operator("object.duplicate_move_linked")
        layout.operator("object.join")

        layout.separator()

        layout.operator("view3d.copybuffer", text="Copy Objects", icon='COPYDOWN')
        layout.operator("view3d.pastebuffer", text="Paste Objects", icon='PASTEDOWN')

        layout.separator()

        layout.menu("VIEW3D_MT_object_parent")
        layout.menu("VIEW3D_MT_object_collection")
        layout.menu("VIEW3D_MT_object_relations")
        layout.menu("VIEW3D_MT_object_constraints")
        layout.menu("VIEW3D_MT_object_track")
        layout.menu("VIEW3D_MT_make_links", text="Make Links")

        layout.separator()

        layout.operator("object.shade_smooth")
        layout.operator("object.shade_flat")

        layout.separator()

        layout.menu("VIEW3D_MT_object_animation")
        layout.menu("VIEW3D_MT_object_rigid_body")

        layout.separator()

        layout.menu("VIEW3D_MT_object_quick_effects")

        layout.separator()

        ob = context.active_object
        if ob and ob.type == 'GPENCIL' and context.gpencil_data:
            layout.operator_menu_enum("gpencil.convert", "type", text="Convert to")
        else:
            layout.operator_menu_enum("object.convert", "target")

        layout.separator()

        layout.menu("VIEW3D_MT_object_showhide")

        layout.separator()

        layout.operator_context = 'EXEC_DEFAULT'
        layout.operator("object.delete", text="Delete").use_global = False
        layout.operator("object.delete", text="Delete Global").use_global = True


class VIEW3D_MT_object_animation(Menu):
    bl_label = "Animation"

    def draw(self, _context):
        layout = self.layout

        layout.operator("anim.keyframe_insert_menu", text="Insert Keyframe...")
        layout.operator("anim.keyframe_delete_v3d", text="Delete Keyframes...")
        layout.operator("anim.keyframe_clear_v3d", text="Clear Keyframes...")
        layout.operator("anim.keying_set_active_set", text="Change Keying Set...")

        layout.separator()

        layout.operator("nla.bake", text="Bake Action...")


class VIEW3D_MT_object_rigid_body(Menu):
    bl_label = "Rigid Body"

    def draw(self, _context):
        layout = self.layout

        layout.operator("rigidbody.objects_add", text="Add Active").type = 'ACTIVE'
        layout.operator("rigidbody.objects_add", text="Add Passive").type = 'PASSIVE'

        layout.separator()

        layout.operator("rigidbody.objects_remove", text="Remove")

        layout.separator()

        layout.operator("rigidbody.shape_change", text="Change Shape")
        layout.operator("rigidbody.mass_calculate", text="Calculate Mass")
        layout.operator("rigidbody.object_settings_copy", text="Copy from Active")
        layout.operator("object.visual_transform_apply", text="Apply Transformation")
        layout.operator("rigidbody.bake_to_keyframes", text="Bake To Keyframes")

        layout.separator()

        layout.operator("rigidbody.connect", text="Connect")


class VIEW3D_MT_object_clear(Menu):
    bl_label = "Clear"

    def draw(self, _context):
        layout = self.layout

        layout.operator("object.location_clear", text="Location").clear_delta = False
        layout.operator("object.rotation_clear", text="Rotation").clear_delta = False
        layout.operator("object.scale_clear", text="Scale").clear_delta = False

        layout.separator()

        layout.operator("object.origin_clear", text="Origin")


class VIEW3D_MT_object_context_menu(Menu):
    bl_label = "Object Context Menu"

    def draw(self, context):

        layout = self.layout
        view = context.space_data

        obj = context.object

        selected_objects_len = len(context.selected_objects)

        # If nothing is selected
        # (disabled for now until it can be made more useful).
        '''
        if selected_objects_len == 0:

            layout.menu("VIEW3D_MT_add", text="Add", text_ctxt=i18n_contexts.operator_default)
            layout.operator("view3d.pastebuffer", text="Paste Objects", icon='PASTEDOWN')

            return
        '''

        # If something is selected
        if obj is not None and obj.type in {'MESH', 'CURVE', 'SURFACE'}:
            layout.operator("object.shade_smooth", text="Shade Smooth")
            layout.operator("object.shade_flat", text="Shade Flat")

            layout.separator()

        if obj is None:
            pass
        elif obj.type == 'MESH':
            layout.operator_context = 'INVOKE_REGION_WIN'
            layout.operator_menu_enum("object.origin_set", text="Set Origin", property="type")

            layout.operator_context = 'INVOKE_DEFAULT'
            # If more than one object is selected
            if selected_objects_len > 1:
                layout.operator("object.join")

            layout.separator()

        elif obj.type == 'CAMERA':
            layout.operator_context = 'INVOKE_REGION_WIN'

            if obj.data.type == 'PERSP':
                props = layout.operator("wm.context_modal_mouse", text="Camera Lens Angle")
                props.data_path_iter = "selected_editable_objects"
                props.data_path_item = "data.lens"
                props.input_scale = 0.1
                if obj.data.lens_unit == 'MILLIMETERS':
                    props.header_text = "Camera Lens Angle: %.1fmm"
                else:
                    props.header_text = "Camera Lens Angle: %.1f\u00B0"

            else:
                props = layout.operator("wm.context_modal_mouse", text="Camera Lens Scale")
                props.data_path_iter = "selected_editable_objects"
                props.data_path_item = "data.ortho_scale"
                props.input_scale = 0.01
                props.header_text = "Camera Lens Scale: %.3f"

            if not obj.data.dof.focus_object:
                if view and view.camera == obj and view.region_3d.view_perspective == 'CAMERA':
                    props = layout.operator("ui.eyedropper_depth", text="DOF Distance (Pick)")
                else:
                    props = layout.operator("wm.context_modal_mouse", text="DOF Distance")
                    props.data_path_iter = "selected_editable_objects"
                    props.data_path_item = "data.dof.focus_distance"
                    props.input_scale = 0.02
                    props.header_text = "DOF Distance: %.3f"

            layout.separator()

        elif obj.type in {'CURVE', 'FONT'}:
            layout.operator_context = 'INVOKE_REGION_WIN'

            props = layout.operator("wm.context_modal_mouse", text="Extrude Size")
            props.data_path_iter = "selected_editable_objects"
            props.data_path_item = "data.extrude"
            props.input_scale = 0.01
            props.header_text = "Extrude Size: %.3f"

            props = layout.operator("wm.context_modal_mouse", text="Width Size")
            props.data_path_iter = "selected_editable_objects"
            props.data_path_item = "data.offset"
            props.input_scale = 0.01
            props.header_text = "Width Size: %.3f"

            layout.separator()

            layout.operator("object.convert", text="Convert to Mesh").target = 'MESH'
            layout.operator_menu_enum("object.origin_set", text="Set Origin", property="type")

            layout.separator()

        elif obj.type == 'GPENCIL':
            layout.operator("gpencil.convert", text="Convert to Path").type = 'PATH'
            layout.operator("gpencil.convert", text="Convert to Bezier Curves").type = 'CURVE'
            layout.operator("gpencil.convert", text="Convert to Mesh").type = 'POLY'

            layout.operator_menu_enum("object.origin_set", text="Set Origin", property="type")

            layout.separator()

        elif obj.type == 'EMPTY':
            layout.operator_context = 'INVOKE_REGION_WIN'

            props = layout.operator("wm.context_modal_mouse", text="Empty Draw Size")
            props.data_path_iter = "selected_editable_objects"
            props.data_path_item = "empty_display_size"
            props.input_scale = 0.01
            props.header_text = "Empty Draw Size: %.3f"

            layout.separator()

        elif obj.type == 'LIGHT':
            light = obj.data

            layout.operator_context = 'INVOKE_REGION_WIN'

            props = layout.operator("wm.context_modal_mouse", text="Energy")
            props.data_path_iter = "selected_editable_objects"
            props.data_path_item = "data.energy"
            props.header_text = "Light Energy: %.3f"

            if light.type == 'AREA':
                props = layout.operator("wm.context_modal_mouse", text="Size X")
                props.data_path_iter = "selected_editable_objects"
                props.data_path_item = "data.size"
                props.header_text = "Light Size X: %.3f"

                if light.shape in {'RECTANGLE', 'ELLIPSE'}:
                    props = layout.operator("wm.context_modal_mouse", text="Size Y")
                    props.data_path_iter = "selected_editable_objects"
                    props.data_path_item = "data.size_y"
                    props.header_text = "Light Size Y: %.3f"

            elif light.type in {'SPOT', 'POINT'}:
                props = layout.operator("wm.context_modal_mouse", text="Radius")
                props.data_path_iter = "selected_editable_objects"
                props.data_path_item = "data.shadow_soft_size"
                props.header_text = "Light Radius: %.3f"

            elif light.type == 'SUN':
                props = layout.operator("wm.context_modal_mouse", text="Angle")
                props.data_path_iter = "selected_editable_objects"
                props.data_path_item = "data.angle"
                props.header_text = "Light Angle: %.3f"

            if light.type == 'SPOT':
                layout.separator()

                props = layout.operator("wm.context_modal_mouse", text="Spot Size")
                props.data_path_iter = "selected_editable_objects"
                props.data_path_item = "data.spot_size"
                props.input_scale = 0.01
                props.header_text = "Spot Size: %.2f"

                props = layout.operator("wm.context_modal_mouse", text="Spot Blend")
                props.data_path_iter = "selected_editable_objects"
                props.data_path_item = "data.spot_blend"
                props.input_scale = -0.01
                props.header_text = "Spot Blend: %.2f"

            layout.separator()

        layout.operator("view3d.copybuffer", text="Copy Objects", icon='COPYDOWN')
        layout.operator("view3d.pastebuffer", text="Paste Objects", icon='PASTEDOWN')

        layout.separator()

        layout.operator("object.duplicate_move", icon='DUPLICATE')
        layout.operator("object.duplicate_move_linked")

        layout.separator()

        props = layout.operator("wm.call_panel", text="Rename Active Object...")
        props.name = "TOPBAR_PT_name"
        props.keep_open = False

        layout.separator()

        layout.menu("VIEW3D_MT_mirror")
        layout.menu("VIEW3D_MT_snap")
        layout.menu("VIEW3D_MT_object_parent")
        layout.operator_context = 'INVOKE_REGION_WIN'

        if view and view.local_view:
            layout.operator("view3d.localview_remove_from")
        else:
            layout.operator("object.move_to_collection")

        layout.separator()

        layout.operator("anim.keyframe_insert_menu", text="Insert Keyframe...")

        layout.separator()

        layout.operator_context = 'EXEC_DEFAULT'
        layout.operator("object.delete", text="Delete").use_global = False


class VIEW3D_MT_object_shading(Menu):
    # XXX, this menu is a place to store shading operator in object mode
    bl_label = "Shading"

    def draw(self, _context):
        layout = self.layout
        layout.operator("object.shade_smooth", text="Smooth")
        layout.operator("object.shade_flat", text="Flat")


class VIEW3D_MT_object_apply(Menu):
    bl_label = "Apply"

    def draw(self, _context):
        layout = self.layout

        props = layout.operator("object.transform_apply", text="Location", text_ctxt=i18n_contexts.default)
        props.location, props.rotation, props.scale = True, False, False

        props = layout.operator("object.transform_apply", text="Rotation", text_ctxt=i18n_contexts.default)
        props.location, props.rotation, props.scale = False, True, False

        props = layout.operator("object.transform_apply", text="Scale", text_ctxt=i18n_contexts.default)
        props.location, props.rotation, props.scale = False, False, True

        props = layout.operator("object.transform_apply", text="All Transforms", text_ctxt=i18n_contexts.default)
        props.location, props.rotation, props.scale = True, True, True

        props = layout.operator("object.transform_apply", text="Rotation & Scale", text_ctxt=i18n_contexts.default)
        props.location, props.rotation, props.scale = False, True, True

        layout.separator()

        layout.operator(
            "object.transforms_to_deltas",
            text="Location to Deltas",
            text_ctxt=i18n_contexts.default,
        ).mode = 'LOC'
        layout.operator(
            "object.transforms_to_deltas",
            text="Rotation to Deltas",
            text_ctxt=i18n_contexts.default,
        ).mode = 'ROT'
        layout.operator(
            "object.transforms_to_deltas",
            text="Scale to Deltas",
            text_ctxt=i18n_contexts.default,
        ).mode = 'SCALE'

        layout.operator(
            "object.transforms_to_deltas",
            text="All Transforms to Deltas",
            text_ctxt=i18n_contexts.default,
        ).mode = 'ALL'
        layout.operator("object.anim_transforms_to_deltas")

        layout.separator()

        layout.operator(
            "object.visual_transform_apply",
            text="Visual Transform",
            text_ctxt=i18n_contexts.default,
        )
        layout.operator(
            "object.convert",
            text="Visual Geometry to Mesh",
            text_ctxt=i18n_contexts.default,
        ).target = 'MESH'
        layout.operator("object.duplicates_make_real")


class VIEW3D_MT_object_parent(Menu):
    bl_label = "Parent"

    def draw(self, _context):
        layout = self.layout

        layout.operator_enum("object.parent_set", "type")

        layout.separator()

        layout.operator_enum("object.parent_clear", "type")


class VIEW3D_MT_object_track(Menu):
    bl_label = "Track"

    def draw(self, _context):
        layout = self.layout

        layout.operator_enum("object.track_set", "type")

        layout.separator()

        layout.operator_enum("object.track_clear", "type")


class VIEW3D_MT_object_collection(Menu):
    bl_label = "Collection"

    def draw(self, _context):
        layout = self.layout

        layout.operator("object.move_to_collection")
        layout.operator("object.link_to_collection")

        layout.separator()

        layout.operator("collection.create")
        # layout.operator_menu_enum("collection.objects_remove", "collection")  # BUGGY
        layout.operator("collection.objects_remove")
        layout.operator("collection.objects_remove_all")

        layout.separator()

        layout.operator("collection.objects_add_active")
        layout.operator("collection.objects_remove_active")


class VIEW3D_MT_object_constraints(Menu):
    bl_label = "Constraints"

    def draw(self, _context):
        layout = self.layout

        layout.operator("object.constraint_add_with_targets")
        layout.operator("object.constraints_copy")

        layout.separator()

        layout.operator("object.constraints_clear")


class VIEW3D_MT_object_quick_effects(Menu):
    bl_label = "Quick Effects"

    def draw(self, _context):
        layout = self.layout

        layout.operator("object.quick_fur")
        layout.operator("object.quick_explode")
        layout.operator("object.quick_smoke")
        layout.operator("object.quick_fluid")


class VIEW3D_MT_object_showhide(Menu):
    bl_label = "Show/Hide"

    def draw(self, _context):
        layout = self.layout

        layout.operator("object.hide_view_clear")

        layout.separator()

        layout.operator("object.hide_view_set", text="Hide Selected").unselected = False
        layout.operator("object.hide_view_set", text="Hide Unselected").unselected = True


class VIEW3D_MT_make_single_user(Menu):
    bl_label = "Make Single User"

    def draw(self, _context):
        layout = self.layout

        props = layout.operator("object.make_single_user", text="Object")
        props.object = True
        props.obdata = props.material = props.animation = False

        props = layout.operator("object.make_single_user", text="Object & Data")
        props.object = props.obdata = True
        props.material = props.animation = False

        props = layout.operator("object.make_single_user", text="Object & Data & Materials")
        props.object = props.obdata = props.material = True
        props.animation = False

        props = layout.operator("object.make_single_user", text="Materials")
        props.material = True
        props.object = props.obdata = props.animation = False

        props = layout.operator("object.make_single_user", text="Object Animation")
        props.animation = True
        props.object = props.obdata = props.material = False


class VIEW3D_MT_make_links(Menu):
    bl_label = "Make Links"

    def draw(self, _context):
        layout = self.layout
        operator_context_default = layout.operator_context

        if len(bpy.data.scenes) > 10:
            layout.operator_context = 'INVOKE_REGION_WIN'
            layout.operator("object.make_links_scene", text="Objects to Scene...", icon='OUTLINER_OB_EMPTY')
        else:
            layout.operator_context = 'EXEC_REGION_WIN'
            layout.operator_menu_enum("object.make_links_scene", "scene", text="Objects to Scene")

        layout.separator()

        layout.operator_context = operator_context_default

        layout.operator_enum("object.make_links_data", "type")  # inline

        layout.operator("object.join_uvs")  # stupid place to add this!


class VIEW3D_MT_brush(Menu):
    bl_label = "Brush"

    def draw(self, context):
        layout = self.layout

        tool_settings = context.tool_settings
        settings = UnifiedPaintPanel.paint_settings(context)
        brush = getattr(settings, "brush", None)

        ups = tool_settings.unified_paint_settings
        layout.prop(ups, "use_unified_size", text="Unified Size")
        layout.prop(ups, "use_unified_strength", text="Unified Strength")
        if context.image_paint_object or context.vertex_paint_object:
            layout.prop(ups, "use_unified_color", text="Unified Color")
        layout.separator()

        # skip if no active brush
        if not brush:
            layout.label(text="No Brushes currently available", icon='INFO')
            return

        # brush paint modes
        layout.menu("VIEW3D_MT_brush_paint_modes")

        # brush tool
        if context.sculpt_object:
            layout.operator("brush.reset")
            layout.prop_menu_enum(brush, "sculpt_tool")
        elif context.image_paint_object:
            layout.prop_menu_enum(brush, "image_tool")
        elif context.vertex_paint_object:
            layout.prop_menu_enum(brush, "vertex_tool")
        elif context.weight_paint_object:
            layout.prop_menu_enum(brush, "weight_tool")

        # TODO: still missing a lot of brush options here

        # sculpt options
        if context.sculpt_object:

            sculpt_tool = brush.sculpt_tool

            layout.separator()
            layout.operator_menu_enum("brush.curve_preset", "shape", text="Curve Preset")
            layout.separator()

            if sculpt_tool != 'GRAB':
                layout.prop_menu_enum(brush, "stroke_method")

                if sculpt_tool in {'DRAW', 'PINCH', 'INFLATE', 'LAYER', 'CLAY'}:
                    layout.prop_menu_enum(brush, "direction")

                if sculpt_tool == 'LAYER':
                    layout.prop(brush, "use_persistent")
                    layout.operator("sculpt.set_persistent_base")


class VIEW3D_MT_brush_paint_modes(Menu):
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


class VIEW3D_MT_paint_vertex(Menu):
    bl_label = "Paint"

    def draw(self, _context):
        layout = self.layout

        layout.operator("paint.vertex_color_set")
        layout.operator("paint.vertex_color_smooth")
        layout.operator("paint.vertex_color_dirt")
        layout.operator("paint.vertex_color_from_weight")

        layout.separator()

        layout.operator("paint.vertex_color_invert", text="Invert")
        layout.operator("paint.vertex_color_levels", text="Levels")
        layout.operator("paint.vertex_color_hsv", text="Hue Saturation Value")
        layout.operator("paint.vertex_color_brightness_contrast", text="Bright/Contrast")


class VIEW3D_MT_hook(Menu):
    bl_label = "Hooks"

    def draw(self, context):
        layout = self.layout
        layout.operator_context = 'EXEC_AREA'
        layout.operator("object.hook_add_newob")
        layout.operator("object.hook_add_selob").use_bone = False
        layout.operator("object.hook_add_selob", text="Hook to Selected Object Bone").use_bone = True

        if [mod.type == 'HOOK' for mod in context.active_object.modifiers]:
            layout.separator()

            layout.operator_menu_enum("object.hook_assign", "modifier")
            layout.operator_menu_enum("object.hook_remove", "modifier")

            layout.separator()

            layout.operator_menu_enum("object.hook_select", "modifier")
            layout.operator_menu_enum("object.hook_reset", "modifier")
            layout.operator_menu_enum("object.hook_recenter", "modifier")


class VIEW3D_MT_vertex_group(Menu):
    bl_label = "Vertex Groups"

    def draw(self, context):
        layout = self.layout

        layout.operator_context = 'EXEC_AREA'
        layout.operator("object.vertex_group_assign_new")

        ob = context.active_object
        if ob.mode == 'EDIT' or (ob.mode == 'WEIGHT_PAINT' and ob.type == 'MESH' and ob.data.use_paint_mask_vertex):
            if ob.vertex_groups.active:
                layout.separator()

                layout.operator("object.vertex_group_assign", text="Assign to Active Group")
                layout.operator(
                    "object.vertex_group_remove_from",
                    text="Remove from Active Group",
                ).use_all_groups = False
                layout.operator("object.vertex_group_remove_from", text="Remove from All").use_all_groups = True

        if ob.vertex_groups.active:
            layout.separator()

            layout.operator_menu_enum("object.vertex_group_set_active", "group", text="Set Active Group")
            layout.operator("object.vertex_group_remove", text="Remove Active Group").all = False
            layout.operator("object.vertex_group_remove", text="Remove All Groups").all = True


class VIEW3D_MT_paint_weight(Menu):
    bl_label = "Weights"

    @staticmethod
    def draw_generic(layout, is_editmode=False):

        if not is_editmode:

            layout.operator("paint.weight_from_bones", text="Assign Automatic From Bones").type = 'AUTOMATIC'
            layout.operator("paint.weight_from_bones", text="Assign From Bone Envelopes").type = 'ENVELOPES'

            layout.separator()

        layout.operator("object.vertex_group_normalize_all", text="Normalize All")
        layout.operator("object.vertex_group_normalize", text="Normalize")

        layout.separator()

        layout.operator("object.vertex_group_mirror", text="Mirror")
        layout.operator("object.vertex_group_invert", text="Invert")
        layout.operator("object.vertex_group_clean", text="Clean")

        layout.separator()

        layout.operator("object.vertex_group_quantize", text="Quantize")
        layout.operator("object.vertex_group_levels", text="Levels")
        layout.operator("object.vertex_group_smooth", text="Smooth")

        if not is_editmode:
            props = layout.operator("object.data_transfer", text="Transfer Weights")
            props.use_reverse_transfer = True
            props.data_type = 'VGROUP_WEIGHTS'

        layout.operator("object.vertex_group_limit_total", text="Limit Total")
        layout.operator("object.vertex_group_fix", text="Fix Deforms")

        if not is_editmode:
            layout.separator()

            layout.operator("paint.weight_set")

    def draw(self, _context):
        self.draw_generic(self.layout, is_editmode=False)


class VIEW3D_MT_sculpt(Menu):
    bl_label = "Sculpt"

    def draw(self, context):
        layout = self.layout

        tool_settings = context.tool_settings
        sculpt = tool_settings.sculpt

        layout.operator("sculpt.dynamic_topology_toggle", text="Toggle Dynamic Topology")

        layout.separator()

        layout.prop(sculpt, "use_symmetry_x")
        layout.prop(sculpt, "use_symmetry_y")
        layout.prop(sculpt, "use_symmetry_z")

        layout.separator()

        layout.prop(sculpt, "lock_x")
        layout.prop(sculpt, "lock_y")
        layout.prop(sculpt, "lock_z")

        layout.separator()

        layout.prop(sculpt, "use_threaded", text="Threaded Sculpt")
        layout.prop(sculpt, "show_low_resolution")
        layout.prop(sculpt, "show_brush")
        layout.prop(sculpt, "use_deform_only")
        layout.prop(sculpt, "show_diffuse_color")
        layout.prop(sculpt, "show_mask")


class VIEW3D_MT_hide_mask(Menu):
    bl_label = "Hide/Mask"

    def draw(self, _context):
        layout = self.layout

        props = layout.operator("paint.hide_show", text="Show All")
        props.action = 'SHOW'
        props.area = 'ALL'

        props = layout.operator("paint.hide_show", text="Hide Bounding Box")
        props.action = 'HIDE'
        props.area = 'INSIDE'

        props = layout.operator("paint.hide_show", text="Show Bounding Box")
        props.action = 'SHOW'
        props.area = 'INSIDE'

        props = layout.operator("paint.hide_show", text="Hide Masked")
        props.area = 'MASKED'
        props.action = 'HIDE'

        layout.separator()

        props = layout.operator("paint.mask_flood_fill", text="Invert Mask")
        props.mode = 'INVERT'

        props = layout.operator("paint.mask_flood_fill", text="Fill Mask")
        props.mode = 'VALUE'
        props.value = 1

        props = layout.operator("paint.mask_flood_fill", text="Clear Mask")
        props.mode = 'VALUE'
        props.value = 0

        props = layout.operator("view3d.select_box", text="Box Mask")
        props = layout.operator("paint.mask_lasso_gesture", text="Lasso Mask")


class VIEW3D_MT_particle(Menu):
    bl_label = "Particle"

    def draw(self, context):
        layout = self.layout
        tool_settings = context.tool_settings

        particle_edit = tool_settings.particle_edit

        layout.operator("particle.mirror")

        layout.operator("particle.remove_doubles")

        layout.separator()

        if particle_edit.select_mode == 'POINT':
            layout.operator("particle.subdivide")

        layout.operator("particle.unify_length")
        layout.operator("particle.rekey")
        layout.operator("particle.weight_set")

        layout.separator()

        layout.menu("VIEW3D_MT_particle_showhide")

        layout.separator()

        layout.operator("particle.delete")


class VIEW3D_MT_particle_context_menu(Menu):
    bl_label = "Particle Context Menu"

    def draw(self, context):
        layout = self.layout
        tool_settings = context.tool_settings

        particle_edit = tool_settings.particle_edit

        layout.operator("particle.rekey")

        layout.separator()

        layout.operator("particle.delete")

        layout.separator()

        layout.operator("particle.remove_doubles")
        layout.operator("particle.unify_length")

        if particle_edit.select_mode == 'POINT':
            layout.operator("particle.subdivide")

        layout.operator("particle.weight_set")

        layout.separator()

        layout.operator("particle.mirror")

        if particle_edit.select_mode == 'POINT':
            layout.separator()

            layout.operator("particle.select_all", text="All").action = 'SELECT'
            layout.operator("particle.select_all", text="None").action = 'DESELECT'
            layout.operator("particle.select_all", text="Invert").action = 'INVERT'

            layout.separator()

            layout.operator("particle.select_roots")
            layout.operator("particle.select_tips")

            layout.separator()

            layout.operator("particle.select_random")

            layout.separator()

            layout.operator("particle.select_more")
            layout.operator("particle.select_less")

            layout.separator()

            layout.operator("particle.select_linked")


class VIEW3D_MT_particle_showhide(ShowHideMenu, Menu):
    _operator_name = "particle"


class VIEW3D_MT_pose(Menu):
    bl_label = "Pose"

    def draw(self, _context):
        layout = self.layout

        layout.menu("VIEW3D_MT_transform_armature")

        layout.menu("VIEW3D_MT_pose_transform")
        layout.menu("VIEW3D_MT_pose_apply")

        layout.menu("VIEW3D_MT_snap")

        layout.separator()

        layout.menu("VIEW3D_MT_object_animation")

        layout.separator()

        layout.menu("VIEW3D_MT_pose_slide")
        layout.menu("VIEW3D_MT_pose_propagate")

        layout.separator()

        layout.operator("pose.copy", icon='COPYDOWN')
        layout.operator("pose.paste", icon='PASTEDOWN').flipped = False
        layout.operator("pose.paste", icon='PASTEFLIPDOWN', text="Paste Pose Flipped").flipped = True

        layout.separator()

        layout.menu("VIEW3D_MT_pose_library")
        layout.menu("VIEW3D_MT_pose_motion")
        layout.menu("VIEW3D_MT_pose_group")

        layout.separator()

        layout.menu("VIEW3D_MT_object_parent")
        layout.menu("VIEW3D_MT_pose_ik")
        layout.menu("VIEW3D_MT_pose_constraints")

        layout.separator()

        layout.operator_context = 'EXEC_AREA'
        layout.operator("pose.autoside_names", text="AutoName Left/Right").axis = 'XAXIS'
        layout.operator("pose.autoside_names", text="AutoName Front/Back").axis = 'YAXIS'
        layout.operator("pose.autoside_names", text="AutoName Top/Bottom").axis = 'ZAXIS'

        layout.operator("pose.flip_names")

        layout.operator("pose.quaternions_flip")

        layout.separator()

        layout.operator_context = 'INVOKE_AREA'
        layout.operator("armature.armature_layers", text="Change Armature Layers...")
        layout.operator("pose.bone_layers", text="Change Bone Layers...")

        layout.separator()

        layout.menu("VIEW3D_MT_pose_showhide")
        layout.menu("VIEW3D_MT_bone_options_toggle", text="Bone Settings")


class VIEW3D_MT_pose_transform(Menu):
    bl_label = "Clear Transform"

    def draw(self, _context):
        layout = self.layout

        layout.operator("pose.transforms_clear", text="All")

        layout.separator()

        layout.operator("pose.loc_clear", text="Location")
        layout.operator("pose.rot_clear", text="Rotation")
        layout.operator("pose.scale_clear", text="Scale")

        layout.separator()

        layout.operator("pose.user_transforms_clear", text="Reset Unkeyed")


class VIEW3D_MT_pose_slide(Menu):
    bl_label = "In-Betweens"

    def draw(self, _context):
        layout = self.layout

        layout.operator("pose.push_rest")
        layout.operator("pose.relax_rest")
        layout.operator("pose.push")
        layout.operator("pose.relax")
        layout.operator("pose.breakdown")


class VIEW3D_MT_pose_propagate(Menu):
    bl_label = "Propagate"

    def draw(self, _context):
        layout = self.layout

        layout.operator("pose.propagate").mode = 'WHILE_HELD'

        layout.separator()

        layout.operator("pose.propagate", text="To Next Keyframe").mode = 'NEXT_KEY'
        layout.operator("pose.propagate", text="To Last Keyframe (Make Cyclic)").mode = 'LAST_KEY'

        layout.separator()

        layout.operator("pose.propagate", text="On Selected Keyframes").mode = 'SELECTED_KEYS'

        layout.separator()

        layout.operator("pose.propagate", text="On Selected Markers").mode = 'SELECTED_MARKERS'


class VIEW3D_MT_pose_library(Menu):
    bl_label = "Pose Library"

    def draw(self, _context):
        layout = self.layout

        layout.operator("poselib.browse_interactive", text="Browse Poses...")

        layout.separator()

        layout.operator("poselib.pose_add", text="Add Pose...")
        layout.operator("poselib.pose_rename", text="Rename Pose...")
        layout.operator("poselib.pose_remove", text="Remove Pose...")


class VIEW3D_MT_pose_motion(Menu):
    bl_label = "Motion Paths"

    def draw(self, _context):
        layout = self.layout

        layout.operator("pose.paths_calculate", text="Calculate")
        layout.operator("pose.paths_clear", text="Clear")


class VIEW3D_MT_pose_group(Menu):
    bl_label = "Bone Groups"

    def draw(self, context):
        layout = self.layout

        pose = context.active_object.pose

        layout.operator_context = 'EXEC_AREA'
        layout.operator("pose.group_assign", text="Assign to New Group").type = 0

        if pose.bone_groups:
            active_group = pose.bone_groups.active_index + 1
            layout.operator("pose.group_assign", text="Assign to Group").type = active_group

            layout.separator()

            # layout.operator_context = 'INVOKE_AREA'
            layout.operator("pose.group_unassign")
            layout.operator("pose.group_remove")


class VIEW3D_MT_pose_ik(Menu):
    bl_label = "Inverse Kinematics"

    def draw(self, _context):
        layout = self.layout

        layout.operator("pose.ik_add")
        layout.operator("pose.ik_clear")


class VIEW3D_MT_pose_constraints(Menu):
    bl_label = "Constraints"

    def draw(self, _context):
        layout = self.layout

        layout.operator("pose.constraint_add_with_targets", text="Add (With Targets)...")
        layout.operator("pose.constraints_copy")
        layout.operator("pose.constraints_clear")


class VIEW3D_MT_pose_showhide(ShowHideMenu, Menu):
    _operator_name = "pose"


class VIEW3D_MT_pose_apply(Menu):
    bl_label = "Apply"

    def draw(self, _context):
        layout = self.layout

        layout.operator("pose.armature_apply").selected = False
        layout.operator("pose.armature_apply", text="Apply Selected as Rest Pose").selected = True
        layout.operator("pose.visual_transform_apply")

        layout.separator()

        props = layout.operator("object.assign_property_defaults")
        props.process_bones = True


class VIEW3D_MT_pose_context_menu(Menu):
    bl_label = "Pose Context Menu"

    def draw(self, _context):
        layout = self.layout

        layout.operator_context = 'INVOKE_REGION_WIN'

        layout.operator("anim.keyframe_insert_menu", text="Insert Keyframe...")

        layout.separator()

        layout.operator("pose.copy", icon='COPYDOWN')
        layout.operator("pose.paste", icon='PASTEDOWN').flipped = False
        layout.operator("pose.paste", icon='PASTEFLIPDOWN', text="Paste X-Flipped Pose").flipped = True

        layout.separator()

        props = layout.operator("wm.call_panel", text="Rename Active Bone...")
        props.name = "TOPBAR_PT_name"
        props.keep_open = False

        layout.separator()

        layout.operator("pose.paths_calculate", text="Calculate")
        layout.operator("pose.paths_clear", text="Clear")

        layout.separator()

        layout.operator("pose.push")
        layout.operator("pose.relax")
        layout.operator("pose.breakdown")

        layout.separator()

        layout.operator("pose.paths_calculate", text="Calculate Motion Paths")
        layout.operator("pose.paths_clear", text="Clear Motion Paths")

        layout.separator()

        layout.operator("pose.hide").unselected = False
        layout.operator("pose.reveal")

        layout.separator()

        layout.operator("pose.user_transforms_clear")


class BoneOptions:
    def draw(self, context):
        layout = self.layout

        options = [
            "show_wire",
            "use_deform",
            "use_envelope_multiply",
            "use_inherit_rotation",
            "use_inherit_scale",
        ]

        if context.mode == 'EDIT_ARMATURE':
            bone_props = bpy.types.EditBone.bl_rna.properties
            data_path_iter = "selected_bones"
            opt_suffix = ""
            options.append("lock")
        else:  # pose-mode
            bone_props = bpy.types.Bone.bl_rna.properties
            data_path_iter = "selected_pose_bones"
            opt_suffix = "bone."

        for opt in options:
            props = layout.operator("wm.context_collection_boolean_set", text=bone_props[opt].name,
                                    text_ctxt=i18n_contexts.default)
            props.data_path_iter = data_path_iter
            props.data_path_item = opt_suffix + opt
            props.type = self.type


class VIEW3D_MT_bone_options_toggle(Menu, BoneOptions):
    bl_label = "Toggle Bone Options"
    type = 'TOGGLE'


class VIEW3D_MT_bone_options_enable(Menu, BoneOptions):
    bl_label = "Enable Bone Options"
    type = 'ENABLE'


class VIEW3D_MT_bone_options_disable(Menu, BoneOptions):
    bl_label = "Disable Bone Options"
    type = 'DISABLE'


# ********** Edit Menus, suffix from ob.type **********


class VIEW3D_MT_edit_mesh(Menu):
    bl_label = "Mesh"

    def draw(self, _context):
        layout = self.layout

        with_bullet = bpy.app.build_options.bullet

        layout.menu("VIEW3D_MT_transform")
        layout.menu("VIEW3D_MT_mirror")
        layout.menu("VIEW3D_MT_snap")

        layout.separator()

        layout.operator("mesh.duplicate_move", text="Duplicate")
        layout.menu("VIEW3D_MT_edit_mesh_extrude")
        layout.operator("mesh.split")
        layout.operator("mesh.bisect")
        layout.operator("mesh.knife_project")

        if with_bullet:
            layout.operator("mesh.convex_hull")

        layout.separator()

        layout.operator("mesh.symmetrize")
        layout.operator("mesh.symmetry_snap")

        layout.separator()

        layout.menu("VIEW3D_MT_edit_mesh_normals")
        layout.menu("VIEW3D_MT_edit_mesh_shading")
        layout.menu("VIEW3D_MT_edit_mesh_weights")
        layout.operator_menu_enum("mesh.sort_elements", "type", text="Sort Elements...")

        layout.separator()

        layout.menu("VIEW3D_MT_edit_mesh_showhide")
        layout.operator_menu_enum("mesh.separate", "type")
        layout.menu("VIEW3D_MT_edit_mesh_clean")

        layout.separator()

        layout.menu("VIEW3D_MT_edit_mesh_delete")


class VIEW3D_MT_edit_mesh_context_menu(Menu):
    bl_label = ""

    def draw(self, context):

        def count_selected_items_for_objects_in_mode():
            selected_verts_len = 0
            selected_edges_len = 0
            selected_faces_len = 0
            for ob in context.objects_in_mode_unique_data:
                v, e, f = ob.data.count_selected_items()
                selected_verts_len += v
                selected_edges_len += e
                selected_faces_len += f
            return (selected_verts_len, selected_edges_len, selected_faces_len)

        is_vert_mode, is_edge_mode, is_face_mode = context.tool_settings.mesh_select_mode
        selected_verts_len, selected_edges_len, selected_faces_len = count_selected_items_for_objects_in_mode()

        del count_selected_items_for_objects_in_mode

        layout = self.layout

        layout.operator_context = 'INVOKE_REGION_WIN'

        # If nothing is selected
        # (disabled for now until it can be made more useful).
        '''
        # If nothing is selected
        if not (selected_verts_len or selected_edges_len or selected_faces_len):
            layout.menu("VIEW3D_MT_mesh_add", text="Add", text_ctxt=i18n_contexts.operator_default)

            return
        '''

        # Else something is selected

        row = layout.row()

        if is_vert_mode:
            col = row.column()

            col.label(text="Vertex Context Menu", icon='VERTEXSEL')
            col.separator()

            # Additive Operators
            col.operator("mesh.subdivide", text="Subdivide")

            col.separator()

            col.operator("mesh.extrude_vertices_move", text="Extrude Vertices")
            col.operator("mesh.bevel", text="Bevel Vertices").vertex_only = True

            if selected_verts_len > 1:
                col.separator()
                col.operator("mesh.edge_face_add", text="New Edge/Face from Vertices")
                col.operator("mesh.vert_connect_path", text="Connect Vertex Path")
                col.operator("mesh.vert_connect", text="Connect Vertex Pairs")

            col.separator()

            # Deform Operators
            col.operator("transform.push_pull", text="Push/Pull")
            col.operator("transform.shrink_fatten", text="Shrink/Fatten")
            col.operator("transform.shear", text="Shear")
            col.operator("transform.vert_slide", text="Slide Vertices")
            col.operator("transform.vertex_random", text="Randomize Vertices")
            col.operator("mesh.vertices_smooth", text="Smooth Vertices")
            col.operator("mesh.vertices_smooth_laplacian", text="Smooth Laplacian")

            col.separator()

            col.menu("VIEW3D_MT_mirror", text="Mirror Vertices")
            col.menu("VIEW3D_MT_snap", text="Snap Vertices")

            col.separator()

            # Removal Operators
            if selected_verts_len > 1:
                col.menu("VIEW3D_MT_edit_mesh_merge", text="Merge Vertices")
            col.operator("mesh.split")
            col.operator_menu_enum("mesh.separate", "type")
            col.operator("mesh.dissolve_verts")
            col.operator("mesh.delete", text="Delete Vertices").type = 'VERT'

        if is_edge_mode:
            render = context.scene.render

            col = row.column()
            col.label(text="Edge Context Menu", icon='EDGESEL')
            col.separator()

            # Additive Operators
            col.operator("mesh.subdivide", text="Subdivide")

            col.separator()

            col.operator("mesh.extrude_edges_move", text="Extrude Edges")
            col.operator("mesh.bevel", text="Bevel Edges").vertex_only = False
            if selected_edges_len >= 2:
                col.operator("mesh.bridge_edge_loops")
            if selected_edges_len >= 1:
                col.operator("mesh.edge_face_add", text="New Face from Edges")
            if selected_edges_len >= 2:
                col.operator("mesh.fill")

            col.separator()

            col.operator("mesh.loopcut_slide")
            col.operator("mesh.offset_edge_loops_slide")
            col.operator("mesh.knife_tool")
            col.operator("mesh.bisect")
            col.operator("mesh.bridge_edge_loops", text="Bridge Edge Loops")

            col.separator()

            # Deform Operators
            col.operator("mesh.edge_rotate", text="Rotate Edge CW").use_ccw = False
            col.operator("transform.edge_slide")
            col.operator("mesh.edge_split")

            col.separator()

            # Edge Flags
            col.operator("transform.edge_crease")
            col.operator("transform.edge_bevelweight")

            col.separator()

            col.operator("mesh.mark_seam").clear = False
            col.operator("mesh.mark_seam", text="Clear Seam").clear = True

            col.separator()

            col.operator("mesh.mark_sharp")
            col.operator("mesh.mark_sharp", text="Clear Sharp").clear = True

            if render.use_freestyle:
                col.separator()

                col.operator("mesh.mark_freestyle_edge").clear = False
                col.operator("mesh.mark_freestyle_edge", text="Clear Freestyle Edge").clear = True

            col.separator()

            # Removal Operators
            col.operator("mesh.unsubdivide")
            col.operator("mesh.split")
            col.operator_menu_enum("mesh.separate", "type")
            col.operator("mesh.dissolve_edges")
            col.operator("mesh.delete", text="Delete Edges").type = 'EDGE'

        if is_face_mode:
            col = row.column()

            col.label(text="Face Context Menu", icon='FACESEL')
            col.separator()

            # Additive Operators
            col.operator("mesh.subdivide", text="Subdivide")

            col.separator()

            col.operator("view3d.edit_mesh_extrude_move_normal", text="Extrude Faces")
            col.operator("view3d.edit_mesh_extrude_move_shrink_fatten", text="Extrude Faces Along Normals")
            col.operator("mesh.extrude_faces_move", text="Extrude Individual Faces")

            col.operator("mesh.inset")
            col.operator("mesh.poke")

            if selected_faces_len >= 2:
                col.operator("mesh.bridge_edge_loops", text="Bridge Faces")

            col.separator()

            # Modify Operators
            col.menu("VIEW3D_MT_uv_map", text="UV Unwrap Faces")

            col.separator()

            props = col.operator("mesh.quads_convert_to_tris")
            props.quad_method = props.ngon_method = 'BEAUTY'
            col.operator("mesh.tris_convert_to_quads")

            col.separator()

            col.operator("mesh.faces_shade_smooth")
            col.operator("mesh.faces_shade_flat")

            col.separator()

            # Removal Operators
            col.operator("mesh.unsubdivide")
            col.operator("mesh.split")
            col.operator_menu_enum("mesh.separate", "type")
            col.operator("mesh.dissolve_faces")
            col.operator("mesh.delete", text="Delete Faces").type = 'FACE'


class VIEW3D_MT_edit_mesh_select_mode(Menu):
    bl_label = "Mesh Select Mode"

    def draw(self, _context):
        layout = self.layout

        layout.operator_context = 'INVOKE_REGION_WIN'
        layout.operator("mesh.select_mode", text="Vertex", icon='VERTEXSEL').type = 'VERT'
        layout.operator("mesh.select_mode", text="Edge", icon='EDGESEL').type = 'EDGE'
        layout.operator("mesh.select_mode", text="Face", icon='FACESEL').type = 'FACE'


class VIEW3D_MT_edit_mesh_extrude(Menu):
    bl_label = "Extrude"

    _extrude_funcs = {
        'VERT': lambda layout:
        layout.operator("mesh.extrude_vertices_move", text="Extrude Vertices"),
        'EDGE': lambda layout:
        layout.operator("mesh.extrude_edges_move", text="Extrude Edges"),
        'REGION': lambda layout:
        layout.operator("view3d.edit_mesh_extrude_move_normal", text="Extrude Faces"),
        'REGION_VERT_NORMAL': lambda layout:
        layout.operator("view3d.edit_mesh_extrude_move_shrink_fatten", text="Extrude Faces Along Normals"),
        'FACE': lambda layout:
        layout.operator("mesh.extrude_faces_move", text="Extrude Individual Faces"),
    }

    @staticmethod
    def extrude_options(context):
        tool_settings = context.tool_settings
        select_mode = tool_settings.mesh_select_mode
        mesh = context.object.data

        menu = []
        if mesh.total_face_sel:
            menu += ['REGION', 'REGION_VERT_NORMAL', 'FACE']
        if mesh.total_edge_sel and (select_mode[0] or select_mode[1]):
            menu += ['EDGE']
        if mesh.total_vert_sel and select_mode[0]:
            menu += ['VERT']

        # should never get here
        return menu

    def draw(self, context):
        layout = self.layout
        layout.operator_context = 'INVOKE_REGION_WIN'

        for menu_id in self.extrude_options(context):
            self._extrude_funcs[menu_id](layout)


class VIEW3D_MT_edit_mesh_vertices(Menu):
    bl_label = "Vertex"

    def draw(self, _context):
        layout = self.layout
        layout.operator_context = 'INVOKE_REGION_WIN'

        layout.operator("mesh.extrude_vertices_move", text="Extrude Vertices")
        layout.operator("mesh.bevel", text="Bevel Vertices").vertex_only = True

        layout.separator()

        layout.operator("mesh.edge_face_add", text="New Edge/Face from Vertices")
        layout.operator("mesh.vert_connect_path", text="Connect Vertex Path")
        layout.operator("mesh.vert_connect", text="Connect Vertex Pairs")

        layout.separator()

        props = layout.operator("mesh.rip_move", text="Rip Vertices")
        props.MESH_OT_rip.use_fill = False
        props = layout.operator("mesh.rip_move", text="Rip Vertices and Fill")
        props.MESH_OT_rip.use_fill = True
        layout.operator("mesh.rip_edge_move", text="Rip Vertices and Extend")

        layout.separator()

        layout.operator("transform.vert_slide", text="Slide Vertices")
        layout.operator("mesh.vertices_smooth", text="Smooth Vertices")

        layout.separator()

        layout.operator("mesh.blend_from_shape")
        layout.operator("mesh.shape_propagate_to_all", text="Propagate to Shapes")

        layout.separator()

        layout.menu("VIEW3D_MT_edit_mesh_merge", text="Merge Vertices")

        layout.separator()

        layout.menu("VIEW3D_MT_vertex_group")
        layout.menu("VIEW3D_MT_hook")

        layout.separator()

        layout.operator("object.vertex_parent_set")


class VIEW3D_MT_edit_mesh_edges_data(Menu):
    bl_label = "Edge Data"

    def draw(self, context):
        layout = self.layout

        render = context.scene.render

        layout.operator_context = 'INVOKE_REGION_WIN'

        layout.operator("transform.edge_crease")
        layout.operator("transform.edge_bevelweight")

        layout.separator()

        layout.operator("mesh.mark_seam").clear = False
        layout.operator("mesh.mark_seam", text="Clear Seam").clear = True

        layout.separator()

        layout.operator("mesh.mark_sharp")
        layout.operator("mesh.mark_sharp", text="Clear Sharp").clear = True

        layout.operator("mesh.mark_sharp", text="Mark Sharp from Vertices").use_verts = True
        props = layout.operator("mesh.mark_sharp", text="Clear Sharp from Vertices")
        props.use_verts = True
        props.clear = True

        if render.use_freestyle:
            layout.separator()

            layout.operator("mesh.mark_freestyle_edge").clear = False
            layout.operator("mesh.mark_freestyle_edge", text="Clear Freestyle Edge").clear = True


class VIEW3D_MT_edit_mesh_edges(Menu):
    bl_label = "Edge"

    def draw(self, _context):
        layout = self.layout

        with_freestyle = bpy.app.build_options.freestyle

        layout.operator_context = 'INVOKE_REGION_WIN'

        layout.operator("mesh.extrude_edges_move", text="Extrude Edges")
        layout.operator("mesh.bevel", text="Bevel Edges").vertex_only = False
        layout.operator("mesh.bridge_edge_loops")

        layout.separator()

        layout.operator("mesh.subdivide")
        layout.operator("mesh.subdivide_edgering")
        layout.operator("mesh.unsubdivide")

        layout.separator()

        layout.operator("mesh.edge_rotate", text="Rotate Edge CW").use_ccw = False
        layout.operator("mesh.edge_rotate", text="Rotate Edge CCW").use_ccw = True

        layout.separator()

        layout.operator("transform.edge_slide")
        layout.operator("mesh.edge_split")

        layout.separator()

        layout.operator("transform.edge_crease")
        layout.operator("transform.edge_bevelweight")

        layout.separator()

        layout.operator("mesh.mark_seam").clear = False
        layout.operator("mesh.mark_seam", text="Clear Seam").clear = True

        layout.separator()

        layout.operator("mesh.mark_sharp")
        layout.operator("mesh.mark_sharp", text="Clear Sharp").clear = True

        layout.operator("mesh.mark_sharp", text="Mark Sharp from Vertices").use_verts = True
        props = layout.operator("mesh.mark_sharp", text="Clear Sharp from Vertices")
        props.use_verts = True
        props.clear = True

        if with_freestyle:
            layout.separator()

            layout.operator("mesh.mark_freestyle_edge").clear = False
            layout.operator("mesh.mark_freestyle_edge", text="Clear Freestyle Edge").clear = True


class VIEW3D_MT_edit_mesh_faces_data(Menu):
    bl_label = "Face Data"

    def draw(self, _context):
        layout = self.layout

        with_freestyle = bpy.app.build_options.freestyle

        layout.operator_context = 'INVOKE_REGION_WIN'

        layout.operator("mesh.colors_rotate")
        layout.operator("mesh.colors_reverse")

        layout.separator()

        layout.operator("mesh.uvs_rotate")
        layout.operator("mesh.uvs_reverse")

        layout.separator()

        if with_freestyle:
            layout.operator("mesh.mark_freestyle_face").clear = False
            layout.operator("mesh.mark_freestyle_face", text="Clear Freestyle Face").clear = True


class VIEW3D_MT_edit_mesh_faces(Menu):
    bl_label = "Face"
    bl_idname = "VIEW3D_MT_edit_mesh_faces"

    def draw(self, _context):
        layout = self.layout

        layout.operator_context = 'INVOKE_REGION_WIN'

        layout.operator("view3d.edit_mesh_extrude_move_normal", text="Extrude Faces")
        layout.operator("view3d.edit_mesh_extrude_move_shrink_fatten", text="Extrude Faces Along Normals")
        layout.operator("mesh.extrude_faces_move", text="Extrude Individual Faces")

        layout.separator()

        layout.operator("mesh.inset")
        layout.operator("mesh.poke")
        props = layout.operator("mesh.quads_convert_to_tris")
        props.quad_method = props.ngon_method = 'BEAUTY'
        layout.operator("mesh.tris_convert_to_quads")
        layout.operator("mesh.solidify", text="Solidify Faces")
        layout.operator("mesh.wireframe")

        layout.separator()

        layout.operator("mesh.fill")
        layout.operator("mesh.fill_grid")
        layout.operator("mesh.beautify_fill")

        layout.separator()

        layout.operator("mesh.intersect")
        layout.operator("mesh.intersect_boolean")

        layout.separator()

        layout.operator("mesh.face_split_by_edges")

        layout.separator()

        layout.operator("mesh.faces_shade_smooth")
        layout.operator("mesh.faces_shade_flat")

        layout.separator()

        layout.menu("VIEW3D_MT_edit_mesh_faces_data")


class VIEW3D_MT_edit_mesh_normals_select_strength(Menu):
    bl_label = "Select by Face Strength"

    def draw(self, _context):
        layout = self.layout

        op = layout.operator("mesh.mod_weighted_strength", text="Weak")
        op.set = False
        op.face_strength = 'WEAK'

        op = layout.operator("mesh.mod_weighted_strength", text="Medium")
        op.set = False
        op.face_strength = 'MEDIUM'

        op = layout.operator("mesh.mod_weighted_strength", text="Strong")
        op.set = False
        op.face_strength = 'STRONG'


class VIEW3D_MT_edit_mesh_normals_set_strength(Menu):
    bl_label = "Select by Face Strength"

    def draw(self, _context):
        layout = self.layout

        op = layout.operator("mesh.mod_weighted_strength", text="Weak")
        op.set = True
        op.face_strength = 'WEAK'

        op = layout.operator("mesh.mod_weighted_strength", text="Medium")
        op.set = True
        op.face_strength = 'MEDIUM'

        op = layout.operator("mesh.mod_weighted_strength", text="Strong")
        op.set = True
        op.face_strength = 'STRONG'


class VIEW3D_MT_edit_mesh_normals_average(Menu):
    bl_label = "Average"

    def draw(self, _context):
        layout = self.layout

        layout.operator("mesh.average_normals", text="Custom Normal").average_type = 'CUSTOM_NORMAL'
        layout.operator("mesh.average_normals", text="Face Area").average_type = 'FACE_AREA'
        layout.operator("mesh.average_normals", text="Corner Angle").average_type = 'CORNER_ANGLE'


class VIEW3D_MT_edit_mesh_normals(Menu):
    bl_label = "Normals"

    def draw(self, _context):
        layout = self.layout

        layout.operator("mesh.flip_normals", text="Flip")
        layout.operator("mesh.normals_make_consistent", text="Recalculate Outside").inside = False
        layout.operator("mesh.normals_make_consistent", text="Recalculate Inside").inside = True

        layout.separator()

        layout.operator("mesh.set_normals_from_faces", text="Set From Faces")

        layout.operator_context = 'INVOKE_DEFAULT'
        layout.operator("transform.rotate_normal", text="Rotate...")
        layout.operator("mesh.point_normals", text="Point to Target...")
        layout.operator_context = 'EXEC_DEFAULT'

        layout.operator("mesh.merge_normals", text="Merge")
        layout.operator("mesh.split_normals", text="Split")
        layout.menu("VIEW3D_MT_edit_mesh_normals_average", text="Average")

        layout.separator()

        layout.operator("mesh.normals_tools", text="Copy Vectors").mode = 'COPY'
        layout.operator("mesh.normals_tools", text="Paste Vectors").mode = 'PASTE'

        layout.operator("mesh.smoothen_normals", text="Smoothen Vectors")
        layout.operator("mesh.normals_tools", text="Reset Vectors").mode = 'RESET'

        layout.separator()

        layout.menu("VIEW3D_MT_edit_mesh_normals_select_strength", text="Select by Face Strength")
        layout.menu("VIEW3D_MT_edit_mesh_normals_set_strength", text="Set Face Strength")


class VIEW3D_MT_edit_mesh_shading(Menu):
    bl_label = "Shading"

    def draw(self, _context):
        layout = self.layout

        layout.operator("mesh.faces_shade_smooth", text="Smooth Faces")
        layout.operator("mesh.faces_shade_flat", text="Flat Faces")

        layout.separator()

        layout.operator("mesh.mark_sharp", text="Smooth Edges").clear = True
        layout.operator("mesh.mark_sharp", text="Sharp Edges")

        layout.separator()

        props = layout.operator("mesh.mark_sharp", text="Smooth Vertices")
        props.use_verts = True
        props.clear = True

        layout.operator("mesh.mark_sharp", text="Sharp Vertices").use_verts = True


class VIEW3D_MT_edit_mesh_weights(Menu):
    bl_label = "Weights"

    def draw(self, _context):
        VIEW3D_MT_paint_weight.draw_generic(self.layout, is_editmode=True)


class VIEW3D_MT_edit_mesh_clean(Menu):
    bl_label = "Clean Up"

    def draw(self, _context):
        layout = self.layout

        layout.operator("mesh.delete_loose")

        layout.separator()

        layout.operator("mesh.decimate")
        layout.operator("mesh.dissolve_degenerate")
        layout.operator("mesh.dissolve_limited")
        layout.operator("mesh.face_make_planar")

        layout.separator()

        layout.operator("mesh.vert_connect_nonplanar")
        layout.operator("mesh.vert_connect_concave")
        layout.operator("mesh.remove_doubles")
        layout.operator("mesh.fill_holes")


class VIEW3D_MT_edit_mesh_delete(Menu):
    bl_label = "Delete"

    def draw(self, _context):
        layout = self.layout

        layout.operator_enum("mesh.delete", "type")

        layout.separator()

        layout.operator("mesh.dissolve_verts")
        layout.operator("mesh.dissolve_edges")
        layout.operator("mesh.dissolve_faces")

        layout.separator()

        layout.operator("mesh.dissolve_limited")

        layout.separator()

        layout.operator("mesh.edge_collapse")
        layout.operator("mesh.delete_edgeloop", text="Edge Loops")


class VIEW3D_MT_edit_mesh_merge(Menu):
    bl_label = "Merge"

    def draw(self, _context):
        layout = self.layout

        layout.operator_enum("mesh.merge", "type")

        layout.separator()

        layout.operator("mesh.remove_doubles", text="By Distance")


class VIEW3D_MT_edit_mesh_showhide(ShowHideMenu, Menu):
    _operator_name = "mesh"


class VIEW3D_MT_edit_gpencil_delete(Menu):
    bl_label = "Delete"

    def draw(self, _context):
        layout = self.layout

        layout.operator_enum("gpencil.delete", "type")

        layout.separator()

        layout.operator_enum("gpencil.dissolve", "type")

        layout.separator()

        layout.operator("gpencil.active_frames_delete_all")
# Edit Curve
# draw_curve is used by VIEW3D_MT_edit_curve and VIEW3D_MT_edit_surface


def draw_curve(self, _context):
    layout = self.layout

    layout.menu("VIEW3D_MT_transform")
    layout.menu("VIEW3D_MT_mirror")
    layout.menu("VIEW3D_MT_snap")

    layout.separator()

    layout.operator("curve.spin")
    layout.operator("curve.duplicate_move")

    layout.separator()

    layout.operator("curve.split")
    layout.operator("curve.separate")
    layout.operator("curve.cyclic_toggle")
    layout.operator_menu_enum("curve.spline_type_set", "type")

    layout.separator()

    layout.menu("VIEW3D_MT_edit_curve_showhide")
    layout.menu("VIEW3D_MT_edit_curve_clean")
    layout.menu("VIEW3D_MT_edit_curve_delete")


class VIEW3D_MT_edit_curve(Menu):
    bl_label = "Curve"

    draw = draw_curve


class VIEW3D_MT_edit_curve_ctrlpoints(Menu):
    bl_label = "Control Points"

    def draw(self, context):
        layout = self.layout

        edit_object = context.edit_object

        if edit_object.type in {'CURVE', 'SURFACE'}:
            layout.operator("curve.extrude_move")

            layout.separator()

            layout.operator("curve.make_segment")

            layout.separator()

            if edit_object.type == 'CURVE':
                layout.operator("transform.tilt")
                layout.operator("curve.tilt_clear")

                layout.separator()

                layout.operator_menu_enum("curve.handle_type_set", "type")
                layout.operator("curve.normals_make_consistent")

                layout.separator()

            layout.operator("curve.smooth")
            if edit_object.type == 'CURVE':
                layout.operator("curve.smooth_tilt")
                layout.operator("curve.smooth_radius")
                layout.operator("curve.smooth_weight")

            layout.separator()

        layout.menu("VIEW3D_MT_hook")

        layout.separator()

        layout.operator("object.vertex_parent_set")


class VIEW3D_MT_edit_curve_segments(Menu):
    bl_label = "Segments"

    def draw(self, _context):
        layout = self.layout

        layout.operator("curve.subdivide")
        layout.operator("curve.switch_direction")


class VIEW3D_MT_edit_curve_clean(Menu):
    bl_label = "Clean Up"

    def draw(self, _context):
        layout = self.layout

        layout.operator("curve.decimate")


class VIEW3D_MT_edit_curve_context_menu(Menu):
    bl_label = "Curve Context Menu"

    def draw(self, _context):
        # TODO(campbell): match mesh vertex menu.

        layout = self.layout

        layout.operator_context = 'INVOKE_DEFAULT'

        # Add
        layout.operator("curve.subdivide")
        layout.operator("curve.extrude_move")
        layout.operator("curve.make_segment")
        layout.operator("curve.duplicate_move")

        layout.separator()

        # Transform
        layout.operator("transform.transform", text="Radius").mode = 'CURVE_SHRINKFATTEN'
        layout.operator("transform.tilt")
        layout.operator("curve.tilt_clear")
        layout.operator("curve.smooth")
        layout.operator("curve.smooth_tilt")
        layout.operator("curve.smooth_radius")

        layout.separator()

        layout.menu("VIEW3D_MT_mirror")
        layout.menu("VIEW3D_MT_snap")

        layout.separator()

        # Modify
        layout.operator_menu_enum("curve.spline_type_set", "type")
        layout.operator_menu_enum("curve.handle_type_set", "type")
        layout.operator("curve.cyclic_toggle")
        layout.operator("curve.switch_direction")

        layout.separator()

        layout.operator("curve.normals_make_consistent")
        layout.operator("curve.spline_weight_set")
        layout.operator("curve.radius_set")

        layout.separator()

        # Remove
        layout.operator("curve.split")
        layout.operator("curve.decimate")
        layout.operator("curve.dissolve_verts")
        layout.operator("curve.delete", text="Delete Segment").type = 'SEGMENT'
        layout.operator("curve.delete", text="Delete Point").type = 'VERT'


class VIEW3D_MT_edit_curve_delete(Menu):
    bl_label = "Delete"

    def draw(self, _context):
        layout = self.layout

        layout.operator_enum("curve.delete", "type")

        layout.separator()

        layout.operator("curve.dissolve_verts")


class VIEW3D_MT_edit_curve_showhide(ShowHideMenu, Menu):
    _operator_name = "curve"


class VIEW3D_MT_edit_surface(Menu):
    bl_label = "Surface"

    draw = draw_curve


class VIEW3D_MT_edit_font(Menu):
    bl_label = "Font"

    def draw(self, _context):
        layout = self.layout

        layout.operator("font.style_toggle", text="Toggle Bold", icon='BOLD').style = 'BOLD'
        layout.operator("font.style_toggle", text="Toggle Italic", icon='ITALIC').style = 'ITALIC'
        layout.operator("font.style_toggle", text="Toggle Underline", icon='UNDERLINE').style = 'UNDERLINE'
        layout.operator("font.style_toggle", text="Toggle Small Caps", icon='SMALL_CAPS').style = 'SMALL_CAPS'


class VIEW3D_MT_edit_text_chars(Menu):
    bl_label = "Special Characters"

    def draw(self, _context):
        layout = self.layout

        layout.operator("font.text_insert", text="Copyright").text = "\u00A9"
        layout.operator("font.text_insert", text="Registered Trademark").text = "\u00AE"

        layout.separator()

        layout.operator("font.text_insert", text="Degree Sign").text = "\u00B0"
        layout.operator("font.text_insert", text="Multiplication Sign").text = "\u00D7"
        layout.operator("font.text_insert", text="Circle").text = "\u008A"

        layout.separator()

        layout.operator("font.text_insert", text="Superscript 1").text = "\u00B9"
        layout.operator("font.text_insert", text="Superscript 2").text = "\u00B2"
        layout.operator("font.text_insert", text="Superscript 3").text = "\u00B3"

        layout.separator()

        layout.operator("font.text_insert", text="Double >>").text = "\u00BB"
        layout.operator("font.text_insert", text="Double <<").text = "\u00AB"
        layout.operator("font.text_insert", text="Promillage").text = "\u2030"

        layout.separator()

        layout.operator("font.text_insert", text="Dutch Florin").text = "\u00A4"
        layout.operator("font.text_insert", text="British Pound").text = "\u00A3"
        layout.operator("font.text_insert", text="Japanese Yen").text = "\u00A5"

        layout.separator()

        layout.operator("font.text_insert", text="German S").text = "\u00DF"
        layout.operator("font.text_insert", text="Spanish Question Mark").text = "\u00BF"
        layout.operator("font.text_insert", text="Spanish Exclamation Mark").text = "\u00A1"


class VIEW3D_MT_edit_meta(Menu):
    bl_label = "Metaball"

    def draw(self, _context):
        layout = self.layout

        layout.menu("VIEW3D_MT_transform")
        layout.menu("VIEW3D_MT_mirror")
        layout.menu("VIEW3D_MT_snap")

        layout.separator()

        layout.operator("mball.duplicate_metaelems")

        layout.separator()

        layout.menu("VIEW3D_MT_edit_meta_showhide")

        layout.operator_context = 'EXEC_DEFAULT'
        layout.operator("mball.delete_metaelems", text="Delete")


class VIEW3D_MT_edit_meta_showhide(Menu):
    bl_label = "Show/Hide"

    def draw(self, _context):
        layout = self.layout

        layout.operator("mball.reveal_metaelems")
        layout.operator("mball.hide_metaelems", text="Hide Selected").unselected = False
        layout.operator("mball.hide_metaelems", text="Hide Unselected").unselected = True


class VIEW3D_MT_edit_lattice(Menu):
    bl_label = "Lattice"

    def draw(self, _context):
        layout = self.layout

        layout.separator()

        layout.menu("VIEW3D_MT_transform")
        layout.menu("VIEW3D_MT_mirror")
        layout.menu("VIEW3D_MT_snap")
        layout.operator_menu_enum("lattice.flip", "axis")

        layout.separator()

        layout.operator("lattice.make_regular")

        layout.separator()

        layout.operator("object.vertex_parent_set")


class VIEW3D_MT_edit_armature(Menu):
    bl_label = "Armature"

    def draw(self, context):
        layout = self.layout

        edit_object = context.edit_object
        arm = edit_object.data

        layout.menu("VIEW3D_MT_transform_armature")
        layout.menu("VIEW3D_MT_mirror")
        layout.menu("VIEW3D_MT_snap")
        layout.menu("VIEW3D_MT_edit_armature_roll")

        layout.separator()

        layout.operator("armature.extrude_move")

        if arm.use_mirror_x:
            layout.operator("armature.extrude_forked")

        layout.operator("armature.duplicate_move")
        layout.operator("armature.merge")
        layout.operator("armature.fill")
        layout.operator("armature.split")
        layout.operator("armature.separate")

        layout.separator()

        layout.operator("armature.subdivide", text="Subdivide")
        layout.operator("armature.switch_direction", text="Switch Direction")

        layout.separator()

        layout.operator("armature.symmetrize")
        layout.menu("VIEW3D_MT_edit_armature_names")

        layout.separator()

        layout.operator_context = 'INVOKE_DEFAULT'
        layout.operator("armature.armature_layers")
        layout.operator("armature.bone_layers")

        layout.separator()

        layout.menu("VIEW3D_MT_edit_armature_parent")

        layout.separator()

        layout.menu("VIEW3D_MT_bone_options_toggle", text="Bone Settings")

        layout.separator()

        layout.menu("VIEW3D_MT_edit_armature_delete")


class VIEW3D_MT_armature_context_menu(Menu):
    bl_label = "Armature Context Menu"

    def draw(self, context):
        layout = self.layout

        edit_object = context.edit_object
        arm = edit_object.data

        layout.operator_context = 'INVOKE_REGION_WIN'

        # Add
        layout.operator("armature.subdivide", text="Subdivide")
        layout.operator("armature.duplicate_move", text="Duplicate")
        layout.operator("armature.extrude_move")
        if arm.use_mirror_x:
            layout.operator("armature.extrude_forked")

        layout.separator()

        layout.operator("armature.fill")

        layout.separator()

        # Modify
        layout.menu("VIEW3D_MT_mirror")
        layout.menu("VIEW3D_MT_snap")
        layout.operator("armature.symmetrize")
        layout.operator("armature.switch_direction", text="Switch Direction")
        layout.menu("VIEW3D_MT_edit_armature_names")

        layout.separator()

        layout.menu("VIEW3D_MT_edit_armature_parent")

        layout.separator()

        # Remove
        layout.operator("armature.split")
        layout.operator("armature.merge")
        layout.operator("armature.dissolve")
        layout.operator("armature.delete")


class VIEW3D_MT_edit_armature_names(Menu):
    bl_label = "Names"

    def draw(self, _context):
        layout = self.layout

        layout.operator_context = 'EXEC_REGION_WIN'
        layout.operator("armature.autoside_names", text="AutoName Left/Right").type = 'XAXIS'
        layout.operator("armature.autoside_names", text="AutoName Front/Back").type = 'YAXIS'
        layout.operator("armature.autoside_names", text="AutoName Top/Bottom").type = 'ZAXIS'
        layout.operator("armature.flip_names", text="Flip Names")


class VIEW3D_MT_edit_armature_parent(Menu):
    bl_label = "Parent"

    def draw(self, _context):
        layout = self.layout

        layout.operator("armature.parent_set", text="Make")
        layout.operator("armature.parent_clear", text="Clear")


class VIEW3D_MT_edit_armature_roll(Menu):
    bl_label = "Bone Roll"

    def draw(self, _context):
        layout = self.layout

        layout.operator_menu_enum("armature.calculate_roll", "type")

        layout.separator()

        layout.operator("transform.transform", text="Set Roll").mode = 'BONE_ROLL'
        layout.operator("armature.roll_clear")


class VIEW3D_MT_edit_armature_delete(Menu):
    bl_label = "Delete"

    def draw(self, _context):
        layout = self.layout
        layout.operator_context = 'EXEC_AREA'

        layout.operator("armature.delete", text="Bones")

        layout.separator()

        layout.operator("armature.dissolve", text="Dissolve Bones")


# ********** Grease Pencil Stroke menus **********
class VIEW3D_MT_gpencil_autoweights(Menu):
    bl_label = "Generate Weights"

    def draw(self, _context):
        layout = self.layout
        layout.operator("gpencil.generate_weights", text="With Empty Groups").mode = 'NAME'
        layout.operator("gpencil.generate_weights", text="With Automatic Weights").mode = 'AUTO'


class VIEW3D_MT_gpencil_simplify(Menu):
    bl_label = "Simplify"

    def draw(self, _context):
        layout = self.layout
        layout.operator("gpencil.stroke_simplify_fixed", text="Fixed")
        layout.operator("gpencil.stroke_simplify", text="Adaptive")


class VIEW3D_MT_paint_gpencil(Menu):
    bl_label = "Strokes"

    def draw(self, _context):

        layout = self.layout

        layout.menu("VIEW3D_MT_gpencil_animation")
        layout.menu("VIEW3D_MT_edit_gpencil_interpolate")

        layout.separator()

        layout.operator("gpencil.delete", text="Delete Frame").type = 'FRAME'
        layout.operator("gpencil.active_frames_delete_all")


class VIEW3D_MT_assign_material(Menu):
    bl_label = "Assign Material"

    def draw(self, context):
        layout = self.layout
        ob = context.active_object

        for slot in ob.material_slots:
            mat = slot.material
            if mat:
                layout.operator("gpencil.stroke_change_color", text=mat.name).material = mat.name


class VIEW3D_MT_gpencil_copy_layer(Menu):
    bl_label = "Copy Layer to Object"

    def draw(self, context):
        layout = self.layout
        view_layer = context.view_layer
        obact = context.active_object
        gpl = context.active_gpencil_layer

        done = False
        if gpl is not None:
            for ob in view_layer.objects:
                if ob.type == 'GPENCIL' and ob != obact:
                    layout.operator("gpencil.layer_duplicate_object", text=ob.name).object = ob.name
                    done = True

            if done is False:
                layout.label(text="No destination object", icon='ERROR')
        else:
            layout.label(text="No layer to copy", icon='ERROR')


class VIEW3D_MT_edit_gpencil(Menu):
    bl_label = "Strokes"

    def draw(self, _context):
        layout = self.layout

        layout.menu("VIEW3D_MT_edit_gpencil_transform")

        layout.separator()
        layout.menu("GPENCIL_MT_snap")

        layout.separator()

        layout.menu("VIEW3D_MT_gpencil_animation")
        layout.menu("VIEW3D_MT_edit_gpencil_interpolate")

        layout.separator()

        # Cut, Copy, Paste
        layout.operator("gpencil.duplicate_move", text="Duplicate")
        layout.operator("gpencil.copy", text="Copy", icon='COPYDOWN')
        layout.operator("gpencil.paste", text="Paste", icon='PASTEDOWN').type = 'COPY'
        layout.operator("gpencil.paste", text="Paste & Merge").type = 'MERGE'

        layout.separator()

        layout.operator("gpencil.stroke_smooth", text="Smooth")
        layout.operator("gpencil.stroke_subdivide", text="Subdivide")
        layout.menu("VIEW3D_MT_gpencil_simplify")
        layout.operator("gpencil.stroke_trim", text="Trim")

        layout.separator()

        layout.operator_menu_enum("gpencil.stroke_separate", "mode", text="Separate...")
        layout.operator("gpencil.stroke_split", text="Split")
        layout.operator("gpencil.stroke_merge", text="Merge")
        op = layout.operator("gpencil.stroke_cyclical_set", text="Close")
        op.type = 'CLOSE'
        op.geometry = True
        layout.operator_menu_enum("gpencil.stroke_join", "type", text="Join...")
        layout.operator("gpencil.stroke_flip", text="Flip Direction")

        layout.separator()

        layout.operator_menu_enum("gpencil.move_to_layer", "layer", text="Move to Layer")
        layout.menu("VIEW3D_MT_assign_material")
        layout.operator_menu_enum("gpencil.stroke_arrange", "direction", text="Arrange Strokes...")

        layout.separator()

        # Convert
        layout.operator("gpencil.stroke_cyclical_set", text="Toggle Cyclic").type = 'TOGGLE'
        layout.operator_menu_enum("gpencil.stroke_caps_set", text="Toggle Caps...", property="type")

        layout.separator()

        # Remove
        layout.menu("GPENCIL_MT_cleanup")
        layout.menu("VIEW3D_MT_edit_gpencil_delete")


class VIEW3D_MT_weight_gpencil(Menu):
    bl_label = "Weights"

    def draw(self, _context):
        layout = self.layout

        layout.operator("gpencil.vertex_group_normalize_all", text="Normalize All")
        layout.operator("gpencil.vertex_group_normalize", text="Normalize")

        layout.separator()
        layout.operator("gpencil.vertex_group_invert", text="Invert")
        layout.operator("gpencil.vertex_group_smooth", text="Smooth")

        layout.separator()
        layout.menu("VIEW3D_MT_gpencil_autoweights")


class VIEW3D_MT_gpencil_animation(Menu):
    bl_label = "Animation"

    @classmethod
    def poll(cls, context):
        ob = context.active_object
        return ob and ob.type == 'GPENCIL' and ob.mode != 'OBJECT'

    def draw(self, _context):
        layout = self.layout

        layout.operator("gpencil.blank_frame_add")
        layout.operator("gpencil.active_frames_delete_all", text="Delete Frame(s)")

        layout.separator()
        layout.operator("gpencil.frame_duplicate", text="Duplicate Active Frame")
        layout.operator("gpencil.frame_duplicate", text="Duplicate All Layers").mode = 'ALL'


class VIEW3D_MT_edit_gpencil_transform(Menu):
    bl_label = "Transform"

    def draw(self, _context):
        layout = self.layout

        layout.operator("transform.translate")
        layout.operator("transform.rotate")
        layout.operator("transform.resize", text="Scale")

        layout.separator()

        layout.operator("transform.bend", text="Bend")
        layout.operator("transform.shear", text="Shear")
        layout.operator("transform.tosphere", text="To Sphere")
        layout.operator("transform.transform", text="Shrink Fatten").mode = 'GPENCIL_SHRINKFATTEN'


class VIEW3D_MT_edit_gpencil_interpolate(Menu):
    bl_label = "Interpolate"

    def draw(self, _context):
        layout = self.layout

        layout.operator("gpencil.interpolate", text="Interpolate")
        layout.operator("gpencil.interpolate_sequence", text="Sequence")


class VIEW3D_MT_object_mode_pie(Menu):
    bl_label = "Mode"

    def draw(self, _context):
        layout = self.layout

        pie = layout.menu_pie()
        pie.operator_enum("object.mode_set", "mode")


class VIEW3D_MT_view_pie(Menu):
    bl_label = "View"
    bl_idname = "VIEW3D_MT_view_pie"

    def draw(self, _context):
        layout = self.layout

        pie = layout.menu_pie()
        pie.operator_enum("view3d.view_axis", "type")
        pie.operator("view3d.view_camera", text="View Camera", icon='CAMERA_DATA')
        pie.operator("view3d.view_selected", text="View Selected", icon='ZOOM_SELECTED')


class VIEW3D_MT_transform_gizmo_pie(Menu):
    bl_label = "View"

    def draw(self, context):
        layout = self.layout

        pie = layout.menu_pie()
        # 1: Left
        pie.operator("view3d.transform_gizmo_set", text="Move").type = {'TRANSLATE'}
        # 2: Right
        pie.operator("view3d.transform_gizmo_set", text="Rotate").type = {'ROTATE'}
        # 3: Down
        pie.operator("view3d.transform_gizmo_set", text="Scale").type = {'SCALE'}
        # 4: Up
        pie.prop(context.space_data, "show_gizmo", text="Show Gizmos", icon='GIZMO')
        # 5: Up/Left
        pie.operator("view3d.transform_gizmo_set", text="All").type = {'TRANSLATE', 'ROTATE', 'SCALE'}


class VIEW3D_MT_shading_pie(Menu):
    bl_label = "Shading"

    def draw(self, context):
        layout = self.layout
        pie = layout.menu_pie()

        view = context.space_data

        pie.prop(view.shading, "type", expand=True)


class VIEW3D_MT_shading_ex_pie(Menu):
    bl_label = "Shading"

    def draw(self, context):
        layout = self.layout
        pie = layout.menu_pie()

        view = context.space_data

        pie.prop_enum(view.shading, "type", value='WIREFRAME')
        pie.prop_enum(view.shading, "type", value='SOLID')

        # Note this duplicates "view3d.toggle_xray" logic, so we can see the active item: T58661.
        if context.pose_object:
            pie.prop(view.overlay, "show_xray_bone", icon='XRAY')
        else:
            xray_active = (
                (context.mode == 'EDIT_MESH') or
                (view.shading.type in {'SOLID', 'WIREFRAME'})
            )
            if xray_active:
                sub = pie
            else:
                sub = pie.row()
                sub.active = False
            sub.prop(
                view.shading,
                "show_xray_wireframe" if (view.shading.type == 'WIREFRAME') else "show_xray",
                text="Toggle X-Ray",
                icon='XRAY',
            )

        pie.prop(view.overlay, "show_overlays", text="Toggle Overlays", icon='OVERLAY')

        pie.prop_enum(view.shading, "type", value='MATERIAL')
        pie.prop_enum(view.shading, "type", value='RENDERED')


class VIEW3D_MT_pivot_pie(Menu):
    bl_label = "Pivot Point"

    def draw(self, context):
        layout = self.layout
        pie = layout.menu_pie()
        obj = context.active_object
        mode = context.mode

        pie.prop_enum(context.scene.tool_settings, "transform_pivot_point", value='BOUNDING_BOX_CENTER')
        pie.prop_enum(context.scene.tool_settings, "transform_pivot_point", value='CURSOR')
        pie.prop_enum(context.scene.tool_settings, "transform_pivot_point", value='INDIVIDUAL_ORIGINS')
        pie.prop_enum(context.scene.tool_settings, "transform_pivot_point", value='MEDIAN_POINT')
        pie.prop_enum(context.scene.tool_settings, "transform_pivot_point", value='ACTIVE_ELEMENT')
        if (obj is None) or (mode in {'OBJECT', 'POSE', 'WEIGHT_PAINT'}):
            pie.prop(context.scene.tool_settings, "use_transform_pivot_point_align")


class VIEW3D_MT_orientations_pie(Menu):
    bl_label = "Orientation"

    def draw(self, context):
        layout = self.layout
        pie = layout.menu_pie()
        scene = context.scene

        pie.prop(scene.transform_orientation_slots[0], "type", expand=True)


class VIEW3D_MT_snap_pie(Menu):
    bl_label = "Snap"

    def draw(self, _context):
        layout = self.layout
        pie = layout.menu_pie()

        pie.operator("view3d.snap_cursor_to_grid", text="Cursor to Grid", icon='CURSOR')
        pie.operator("view3d.snap_selected_to_grid", text="Selection to Grid", icon='RESTRICT_SELECT_OFF')
        pie.operator("view3d.snap_cursor_to_selected", text="Cursor to Selected", icon='CURSOR')
        pie.operator(
            "view3d.snap_selected_to_cursor",
            text="Selection to Cursor",
            icon='RESTRICT_SELECT_OFF',
        ).use_offset = False
        pie.operator(
            "view3d.snap_selected_to_cursor",
            text="Selection to Cursor (Keep Offset)",
            icon='RESTRICT_SELECT_OFF',
        ).use_offset = True
        pie.operator("view3d.snap_selected_to_active", text="Selection to Active", icon='RESTRICT_SELECT_OFF')
        pie.operator("view3d.snap_cursor_to_center", text="Cursor to World Origin", icon='CURSOR')
        pie.operator("view3d.snap_cursor_to_active", text="Cursor to Active", icon='CURSOR')


class VIEW3D_MT_proportional_editing_falloff_pie(Menu):
    bl_label = "Proportional Editing Falloff"

    def draw(self, context):
        layout = self.layout
        pie = layout.menu_pie()
        tool_settings = context.scene.tool_settings

        pie.prop(tool_settings, "proportional_edit_falloff", expand=True)


# ********** Panel **********


class VIEW3D_PT_active_tool(Panel, ToolActivePanelHelper):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "Tool"
    # See comment below.
    # bl_options = {'HIDE_HEADER'}

    # Don't show in properties editor.
    @classmethod
    def poll(cls, context):
        return context.area.type == 'VIEW_3D'


# FIXME(campbell): remove this second panel once 'HIDE_HEADER' works with category tabs,
# Currently pinning allows ordering headerless panels below panels with headers.
class VIEW3D_PT_active_tool_duplicate(Panel, ToolActivePanelHelper):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "Tool"
    bl_options = {'HIDE_HEADER'}

    # Only show in properties editor.
    @classmethod
    def poll(cls, context):
        return context.area.type != 'VIEW_3D'


class VIEW3D_PT_view3d_properties(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "View"
    bl_label = "View"

    def draw(self, context):
        layout = self.layout

        view = context.space_data

        layout.use_property_split = True
        layout.use_property_decorate = False  # No animation.

        flow = layout.grid_flow(row_major=True, columns=0, even_columns=False, even_rows=False, align=True)
        col = flow.column()

        subcol = col.column()
        subcol.active = bool(view.region_3d.view_perspective != 'CAMERA' or view.region_quadviews)
        subcol.prop(view, "lens", text="Focal Length")

        subcol = col.column(align=True)
        subcol.prop(view, "clip_start", text="Clip Start")
        subcol.prop(view, "clip_end", text="End")

        subcol.separator()

        col = flow.column()

        subcol = col.column()
        subcol.prop(view, "use_local_camera")

        subcol = col.column()
        subcol.enabled = view.use_local_camera
        subcol.prop(view, "camera", text="Local Camera")

        subcol = col.column(align=True)
        subcol.prop(view, "use_render_border")
        subcol.active = view.region_3d.view_perspective != 'CAMERA'


class VIEW3D_PT_view3d_lock(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "View"
    bl_label = "View Lock"
    bl_parent_id = "VIEW3D_PT_view3d_properties"

    def draw(self, context):
        layout = self.layout

        layout.use_property_split = True
        layout.use_property_decorate = False  # No animation.

        view = context.space_data

        col = layout.column(align=True)
        subcol = col.column()
        subcol.active = bool(view.region_3d.view_perspective != 'CAMERA' or view.region_quadviews)

        subcol.prop(view, "lock_object")
        lock_object = view.lock_object
        if lock_object:
            if lock_object.type == 'ARMATURE':
                subcol.prop_search(
                    view, "lock_bone", lock_object.data,
                    "edit_bones" if lock_object.mode == 'EDIT'
                    else "bones",
                    text="",
                )
        else:
            subcol.prop(view, "lock_cursor", text="Lock to 3D Cursor")

        col.prop(view, "lock_camera")


class VIEW3D_PT_view3d_cursor(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "View"
    bl_label = "3D Cursor"

    def draw(self, context):
        layout = self.layout

        cursor = context.scene.cursor

        layout.column().prop(cursor, "location", text="Location")
        rotation_mode = cursor.rotation_mode
        if rotation_mode == 'QUATERNION':
            layout.column().prop(cursor, "rotation_quaternion", text="Rotation")
        elif rotation_mode == 'AXIS_ANGLE':
            layout.column().prop(cursor, "rotation_axis_angle", text="Rotation")
        else:
            layout.column().prop(cursor, "rotation_euler", text="Rotation")
        layout.prop(cursor, "rotation_mode", text="")


class VIEW3D_PT_collections(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "View"
    bl_label = "Collections"
    bl_options = {'DEFAULT_CLOSED'}

    def _draw_collection(self, layout, view_layer, collection, index):
        need_separator = index
        for child in collection.children:
            index += 1

            if child.exclude:
                continue

            if child.collection.hide_viewport:
                continue

            if need_separator:
                layout.separator()
                need_separator = False

            icon = 'BLANK1'
            # has_objects = True
            if child.has_selected_objects(view_layer):
                icon = 'LAYER_ACTIVE'
            elif child.has_objects():
                icon = 'LAYER_USED'
            else:
                # has_objects = False
                pass

            row = layout.row()
            sub = row.split(factor=0.98)
            subrow = sub.row()
            subrow.alignment = 'LEFT'
            subrow.operator(
                "object.hide_collection", text=child.name, icon=icon, emboss=False,
            ).collection_index = index

            sub = row.split()
            subrow = sub.row(align=True)
            subrow.alignment = 'RIGHT'
            subrow.active = collection.is_visible  # Parent collection runtime visibility
            subrow.prop(child, "hide_viewport", text="", emboss=False)

        for child in collection.children:
            index = self._draw_collection(layout, view_layer, child, index)

        return index

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = False

        view_layer = context.view_layer
        # We pass index 0 here beause the index is increased
        # so the first real index is 1
        # And we start with index as 1 because we skip the master collection
        self._draw_collection(layout, view_layer, view_layer.layer_collection, 0)


class VIEW3D_PT_object_type_visibility(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'
    bl_label = "View Object Types"
    bl_ui_units_x = 6

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        view = context.space_data

        layout.label(text="Object Types Visibility")
        col = layout.column()

        attr_object_types = (
            # Geometry
            ("mesh", "Mesh"),
            ("curve", "Curve"),
            ("surf", "Surface"),
            ("meta", "Meta"),
            ("font", "Text"),
            ("grease_pencil", "Grease Pencil"),
            (None, None),
            # Other
            ("armature", "Armature"),
            ("lattice", "Lattice"),
            ("empty", "Empty"),
            ("light", "Light"),
            ("light_probe", "Light Probe"),
            ("camera", "Camera"),
            ("speaker", "Speaker"),
        )

        for attr, attr_name in attr_object_types:
            if attr is None:
                col.separator()
                continue

            attr_v = "show_object_viewport_" f"{attr:s}"
            attr_s = "show_object_select_" f"{attr:s}"

            icon_v = 'HIDE_OFF' if getattr(view, attr_v) else 'HIDE_ON'
            icon_s = 'RESTRICT_SELECT_OFF' if getattr(view, attr_s) else 'RESTRICT_SELECT_ON'

            row = col.row(align=True)
            row.alignment = 'RIGHT'

            row.label(text=attr_name)
            row.prop(view, attr_v, text="", icon=icon_v, emboss=False)
            rowsub = row.row(align=True)
            rowsub.active = getattr(view, attr_v)
            rowsub.prop(view, attr_s, text="", icon=icon_s, emboss=False)


class VIEW3D_PT_shading(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'
    bl_label = "Shading"
    bl_ui_units_x = 12

    @classmethod
    def get_shading(cls, context):
        # Get settings from 3D viewport or OpenGL render engine
        view = context.space_data
        if view.type == 'VIEW_3D':
            return view.shading
        else:
            return context.scene.display.shading

    def draw(self, _context):
        layout = self.layout
        layout.label(text="Viewport Shading")


class VIEW3D_PT_shading_lighting(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'
    bl_label = "Lighting"
    bl_parent_id = 'VIEW3D_PT_shading'

    @classmethod
    def poll(cls, context):
        shading = VIEW3D_PT_shading.get_shading(context)
        return shading.type in {'SOLID', 'MATERIAL'}

    def draw(self, context):
        layout = self.layout
        shading = VIEW3D_PT_shading.get_shading(context)

        col = layout.column()
        split = col.split(factor=0.9)

        if shading.type == 'SOLID':
            split.row().prop(shading, "light", expand=True)
            col = split.column()

            split = layout.split(factor=0.9)
            col = split.column()
            sub = col.row()

            if shading.light == 'STUDIO':
                prefs = context.preferences
                system = prefs.system

                if not system.use_studio_light_edit:
                    sub.scale_y = 0.6  # smaller studiolight preview
                    sub.template_icon_view(shading, "studio_light", scale_popup=3.0)
                else:
                    sub.prop(
                        system,
                        "use_studio_light_edit",
                        text="Disable Studio Light Edit",
                        icon='NONE',
                        toggle=True,
                    )

                col = split.column()
                col.operator("preferences.studiolight_show", emboss=False, text="", icon='PREFERENCES')

                split = layout.split(factor=0.9)
                col = split.column()

                row = col.row()
                row.prop(shading, "use_world_space_lighting", text="", icon='WORLD', toggle=True)
                row = row.row()
                row.active = shading.use_world_space_lighting
                row.prop(shading, "studiolight_rotate_z", text="Rotation")
                col = split.column()  # to align properly with above

            elif shading.light == 'MATCAP':
                sub.scale_y = 0.6  # smaller matcap preview

                sub.template_icon_view(shading, "studio_light", scale_popup=3.0)

                col = split.column()
                col.operator("preferences.studiolight_show", emboss=False, text="", icon='PREFERENCES')
                col.operator("view3d.toggle_matcap_flip", emboss=False, text="", icon='ARROW_LEFTRIGHT')

        elif shading.type == 'MATERIAL':
            col.prop(shading, "use_scene_lights")
            col.prop(shading, "use_scene_world")

            if not shading.use_scene_world:
                col = layout.column()
                split = col.split(factor=0.9)

                col = split.column()
                sub = col.row()
                sub.scale_y = 0.6
                sub.template_icon_view(shading, "studio_light", scale_popup=3)

                col = split.column()
                col.operator("preferences.studiolight_show", emboss=False, text="", icon='PREFERENCES')

                if shading.selected_studio_light.type == 'WORLD':
                    split = layout.split(factor=0.9)
                    col = split.column()
                    col.prop(shading, "studiolight_rotate_z", text="Rotation")
                    col.prop(shading, "studiolight_background_alpha")
                    col = split.column()  # to align properly with above


class VIEW3D_PT_shading_color(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'
    bl_label = "Color"
    bl_parent_id = 'VIEW3D_PT_shading'

    @classmethod
    def poll(cls, context):
        shading = VIEW3D_PT_shading.get_shading(context)
        return shading.type in {'WIREFRAME', 'SOLID'}

    def _draw_color_type(self, context):
        layout = self.layout
        shading = VIEW3D_PT_shading.get_shading(context)

        layout.grid_flow(columns=3, align=True).prop(shading, "color_type", expand=True)
        if shading.color_type == 'SINGLE':
            layout.row().prop(shading, "single_color", text="")

    def _draw_background_color(self, context):
        layout = self.layout
        shading = VIEW3D_PT_shading.get_shading(context)

        layout.row().label(text="Background")
        layout.row().prop(shading, "background_type", expand=True)
        if shading.background_type == 'VIEWPORT':
            layout.row().prop(shading, "background_color", text="")

    def draw(self, context):
        shading = VIEW3D_PT_shading.get_shading(context)
        if shading.type == 'WIREFRAME':
            self.layout.row().prop(shading, "wireframe_color_type", expand=True)
        else:
            self._draw_color_type(context)
            self.layout.separator()
        self._draw_background_color(context)


class VIEW3D_PT_shading_options(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'
    bl_label = "Options"
    bl_parent_id = 'VIEW3D_PT_shading'

    @classmethod
    def poll(cls, context):
        shading = VIEW3D_PT_shading.get_shading(context)
        return shading.type in {'WIREFRAME', 'SOLID'}

    def draw(self, context):
        layout = self.layout

        shading = VIEW3D_PT_shading.get_shading(context)

        col = layout.column()

        if shading.type == 'SOLID':
            col.prop(shading, "show_backface_culling")

        row = col.row(align=True)

        if shading.type == 'WIREFRAME':
            row.prop(shading, "show_xray_wireframe", text="")
            sub = row.row()
            sub.active = shading.show_xray_wireframe
            sub.prop(shading, "xray_alpha_wireframe", text="X-Ray")
        elif shading.type == 'SOLID':
            row.prop(shading, "show_xray", text="")
            sub = row.row()
            sub.active = shading.show_xray
            sub.prop(shading, "xray_alpha", text="X-Ray")
            # X-ray mode is off when alpha is 1.0
            xray_active = shading.show_xray and shading.xray_alpha != 1

            row = col.row(align=True)
            row.prop(shading, "show_shadows", text="")
            row.active = not xray_active
            sub = row.row(align=True)
            sub.active = shading.show_shadows
            sub.prop(shading, "shadow_intensity", text="Shadow")
            sub.popover(
                panel="VIEW3D_PT_shading_options_shadow",
                icon='PREFERENCES',
                text="",
            )

            col = layout.column()

            row = col.row()
            row.active = not xray_active
            row.prop(shading, "show_cavity")

            if shading.show_cavity and not xray_active:
                row.prop(shading, "cavity_type", text="Type")

                if shading.cavity_type in {'WORLD', 'BOTH'}:
                    col.label(text="World Space")
                    sub = col.row(align=True)
                    sub.prop(shading, "cavity_ridge_factor", text="Ridge")
                    sub.prop(shading, "cavity_valley_factor", text="Valley")
                    sub.popover(
                        panel="VIEW3D_PT_shading_options_ssao",
                        icon='PREFERENCES',
                        text="",
                    )

                if shading.cavity_type in {'SCREEN', 'BOTH'}:
                    col.label(text="Screen Space")
                    sub = col.row(align=True)
                    sub.prop(shading, "curvature_ridge_factor", text="Ridge")
                    sub.prop(shading, "curvature_valley_factor", text="Valley")

            row = col.row()
            row.active = not xray_active
            row.prop(shading, "use_dof", text="Depth Of Field")

        if shading.type in {'WIREFRAME', 'SOLID'}:
            row = layout.split()
            row.prop(shading, "show_object_outline")
            sub = row.row()
            sub.active = shading.show_object_outline
            sub.prop(shading, "object_outline_color", text="")

            col = layout.column()
            if (shading.light == 'STUDIO') and (shading.type != 'WIREFRAME'):
                col.prop(shading, "show_specular_highlight", text="Specular Lighting")


class VIEW3D_PT_shading_options_shadow(Panel):
    bl_label = "Shadow Settings"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        scene = context.scene

        col = layout.column()
        col.prop(scene.display, "light_direction")
        col.prop(scene.display, "shadow_shift")
        col.prop(scene.display, "shadow_focus")


class VIEW3D_PT_shading_options_ssao(Panel):
    bl_label = "SSAO Settings"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        scene = context.scene

        col = layout.column(align=True)
        col.prop(scene.display, "matcap_ssao_samples")
        col.prop(scene.display, "matcap_ssao_distance")
        col.prop(scene.display, "matcap_ssao_attenuation")


class VIEW3D_PT_gizmo_display(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'
    bl_label = "Gizmo"

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        view = context.space_data

        col = layout.column()
        col.label(text="Viewport Gizmos")

        col.active = view.show_gizmo
        colsub = col.column()
        colsub.prop(view, "show_gizmo_navigate", text="Navigate")
        colsub.prop(view, "show_gizmo_tool", text="Active Tools")
        colsub.prop(view, "show_gizmo_context", text="Active Object")

        layout.separator()

        col = layout.column()
        col.active = view.show_gizmo_context
        col.label(text="Object Gizmos")
        col.prop(scene.transform_orientation_slots[1], "type", text="")
        col.prop(view, "show_gizmo_object_translate", text="Move")
        col.prop(view, "show_gizmo_object_rotate", text="Rotate")
        col.prop(view, "show_gizmo_object_scale", text="Scale")

        layout.separator()

        # Match order of object type visibility
        col = layout.column()
        col.label(text="Empty")
        col.prop(view, "show_gizmo_empty_image", text="Image")
        col.prop(view, "show_gizmo_empty_force_field", text="Force Field")
        col.label(text="Light")
        col.prop(view, "show_gizmo_light_size", text="Size")
        col.prop(view, "show_gizmo_light_look_at", text="Look At")
        col.label(text="Camera")
        col.prop(view, "show_gizmo_camera_lens", text="Lens")
        col.prop(view, "show_gizmo_camera_dof_distance", text="Focus Distance")


class VIEW3D_PT_overlay(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'
    bl_label = "Overlays"
    bl_ui_units_x = 13

    def draw(self, _context):
        layout = self.layout
        layout.label(text="Viewport Overlays")


class VIEW3D_PT_overlay_guides(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'
    bl_parent_id = 'VIEW3D_PT_overlay'
    bl_label = "Guides"

    def draw(self, context):
        layout = self.layout

        view = context.space_data
        overlay = view.overlay
        shading = view.shading
        display_all = overlay.show_overlays

        col = layout.column()
        col.active = display_all

        split = col.split()
        sub = split.column()

        row = sub.row()
        row_el = row.column()
        row_el.prop(overlay, "show_ortho_grid", text="Grid")
        grid_active = (
            view.region_quadviews or
            (view.region_3d.is_orthographic_side_view and not view.region_3d.is_perspective)
        )
        row_el.active = grid_active
        row.prop(overlay, "show_floor", text="Floor")

        if overlay.show_floor or overlay.show_ortho_grid:
            sub = col.row(align=True)
            sub.active = (
                (overlay.show_floor and not view.region_3d.is_orthographic_side_view) or
                (overlay.show_ortho_grid and grid_active)
            )
            sub.prop(overlay, "grid_scale", text="Scale")
            sub.prop(overlay, "grid_subdivisions", text="Subdivisions")

        sub = split.column()
        row = sub.row()
        row.label(text="Axes")

        subrow = row.row(align=True)
        subrow.prop(overlay, "show_axis_x", text="X", toggle=True)
        subrow.prop(overlay, "show_axis_y", text="Y", toggle=True)
        subrow.prop(overlay, "show_axis_z", text="Z", toggle=True)

        split = col.split()
        sub = split.column()
        sub.prop(overlay, "show_text", text="Text Info")
        sub = split.column()
        sub.prop(overlay, "show_cursor", text="3D Cursor")

        if shading.type == 'MATERIAL':
            col.prop(overlay, "show_look_dev")

        col.prop(overlay, "show_annotation", text="Annotations")


class VIEW3D_PT_overlay_object(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'
    bl_parent_id = 'VIEW3D_PT_overlay'
    bl_label = "Objects"

    def draw(self, context):
        layout = self.layout
        view = context.space_data
        overlay = view.overlay
        display_all = overlay.show_overlays

        col = layout.column(align=True)
        col.active = display_all

        split = col.split()

        sub = split.column(align=True)
        sub.prop(overlay, "show_extras", text="Extras")
        sub.prop(overlay, "show_relationship_lines")
        sub.prop(overlay, "show_outline_selected")

        sub = split.column(align=True)
        sub.prop(overlay, "show_bones", text="Bones")
        sub.prop(overlay, "show_motion_paths")
        sub.prop(overlay, "show_object_origins", text="Origins")
        subsub = sub.column()
        subsub.active = overlay.show_object_origins
        subsub.prop(overlay, "show_object_origins_all", text="Origins (All)")


class VIEW3D_PT_overlay_geometry(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'
    bl_parent_id = 'VIEW3D_PT_overlay'
    bl_label = "Geometry"

    def draw(self, context):
        layout = self.layout
        view = context.space_data
        overlay = view.overlay
        display_all = overlay.show_overlays
        is_wireframes = view.shading.type == 'WIREFRAME'

        col = layout.column()
        col.active = display_all

        row = col.row(align=True)
        if not is_wireframes:
            row.prop(overlay, "show_wireframes", text="")
        sub = row.row()
        sub.active = overlay.show_wireframes or is_wireframes
        sub.prop(overlay, "wireframe_threshold", text="Wireframe")

        col = layout.column(align=True)
        col.active = display_all

        col.prop(overlay, "show_face_orientation")

        # sub.prop(overlay, "show_onion_skins")


class VIEW3D_PT_overlay_motion_tracking(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'
    bl_parent_id = 'VIEW3D_PT_overlay'
    bl_label = "Motion Tracking"

    def draw_header(self, context):
        view = context.space_data
        self.layout.prop(view, "show_reconstruction", text="")

    def draw(self, context):
        layout = self.layout
        view = context.space_data
        overlay = view.overlay
        display_all = overlay.show_overlays

        col = layout.column()
        col.active = display_all

        if view.show_reconstruction:
            split = col.split()

            sub = split.column(align=True)
            sub.active = view.show_reconstruction
            sub.prop(view, "show_camera_path", text="Camera Path")

            sub = split.column()
            sub.prop(view, "show_bundle_names", text="Marker Names")

            col = layout.column()
            col.label(text="Tracks:")
            row = col.row(align=True)
            row.prop(view, "tracks_display_type", text="")
            row.prop(view, "tracks_display_size", text="Size")


class VIEW3D_PT_overlay_edit_mesh(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'
    bl_parent_id = 'VIEW3D_PT_overlay'
    bl_label = "Mesh Edit Mode"

    @classmethod
    def poll(cls, context):
        return context.mode == 'EDIT_MESH'

    def draw(self, context):
        layout = self.layout

        view = context.space_data
        overlay = view.overlay
        display_all = overlay.show_overlays

        col = layout.column()
        col.active = display_all

        split = col.split()

        sub = split.column()
        sub.prop(overlay, "show_edges", text="Edges")
        sub = split.column()
        sub.prop(overlay, "show_faces", text="Faces")
        sub = split.column()
        sub.prop(overlay, "show_face_center", text="Center")

        row = col.row(align=True)
        row.prop(overlay, "show_edge_crease", text="Creases", toggle=True)
        row.prop(overlay, "show_edge_sharp", text="Sharp", text_ctxt=i18n_contexts.plural, toggle=True)
        row.prop(overlay, "show_edge_bevel_weight", text="Bevel", toggle=True)
        row.prop(overlay, "show_edge_seams", text="Seams", toggle=True)

        if context.preferences.view.show_developer_ui:
            col.label(text="Developer")
            col.prop(overlay, "show_extra_indices", text="Indices")


class VIEW3D_PT_overlay_edit_mesh_shading(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'
    bl_parent_id = 'VIEW3D_PT_overlay_edit_mesh'
    bl_label = "Shading"

    @classmethod
    def poll(cls, context):
        return context.mode == 'EDIT_MESH'

    def draw(self, context):
        layout = self.layout

        view = context.space_data
        shading = view.shading
        overlay = view.overlay
        tool_settings = context.tool_settings
        display_all = overlay.show_overlays
        statvis = tool_settings.statvis

        col = layout.column()
        col.active = display_all

        col.prop(overlay, "show_occlude_wire")

        col.prop(overlay, "show_weight", text="Vertex Group Weights")
        if overlay.show_weight:
            row = col.split(factor=0.33)
            row.label(text="Zero Weights")
            sub = row.row()
            sub.prop(tool_settings, "vertex_group_user", expand=True)

        if shading.type == 'WIREFRAME':
            xray = shading.show_xray_wireframe and shading.xray_alpha_wireframe < 1.0
        elif shading.type == 'SOLID':
            xray = shading.show_xray and shading.xray_alpha < 1.0
        else:
            xray = False
        statvis_active = not xray
        row = col.row()
        row.active = statvis_active
        row.prop(overlay, "show_statvis", text="Mesh Analysis")
        if overlay.show_statvis:
            col = col.column()
            col.active = statvis_active

            sub = col.split()
            sub.label(text="Type")
            sub.prop(statvis, "type", text="")

            statvis_type = statvis.type
            if statvis_type == 'OVERHANG':
                row = col.row(align=True)
                row.prop(statvis, "overhang_min", text="Minimum")
                row.prop(statvis, "overhang_max", text="Maximum")
                col.row().prop(statvis, "overhang_axis", expand=True)
            elif statvis_type == 'THICKNESS':
                row = col.row(align=True)
                row.prop(statvis, "thickness_min", text="Minimum")
                row.prop(statvis, "thickness_max", text="Maximum")
                col.prop(statvis, "thickness_samples")
            elif statvis_type == 'INTERSECT':
                pass
            elif statvis_type == 'DISTORT':
                row = col.row(align=True)
                row.prop(statvis, "distort_min", text="Minimum")
                row.prop(statvis, "distort_max", text="Maximum")
            elif statvis_type == 'SHARP':
                row = col.row(align=True)
                row.prop(statvis, "sharp_min", text="Minimum")
                row.prop(statvis, "sharp_max", text="Maximum")


class VIEW3D_PT_overlay_edit_mesh_measurement(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'
    bl_parent_id = 'VIEW3D_PT_overlay_edit_mesh'
    bl_label = "Measurement"

    @classmethod
    def poll(cls, context):
        return context.mode == 'EDIT_MESH'

    def draw(self, context):
        layout = self.layout

        view = context.space_data
        overlay = view.overlay
        display_all = overlay.show_overlays

        col = layout.column()
        col.active = display_all

        split = col.split()

        sub = split.column()
        sub.prop(overlay, "show_extra_edge_length", text="Edge Length")
        sub.prop(overlay, "show_extra_edge_angle", text="Edge Angle")

        sub = split.column()
        sub.prop(overlay, "show_extra_face_area", text="Face Area")
        sub.prop(overlay, "show_extra_face_angle", text="Face Angle")


class VIEW3D_PT_overlay_edit_mesh_normals(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'
    bl_parent_id = 'VIEW3D_PT_overlay_edit_mesh'
    bl_label = "Normals"

    @classmethod
    def poll(cls, context):
        return context.mode == 'EDIT_MESH'

    def draw(self, context):
        layout = self.layout

        view = context.space_data
        overlay = view.overlay
        display_all = overlay.show_overlays

        col = layout.column()
        col.active = display_all

        row = col.row(align=True)
        row.prop(overlay, "show_vertex_normals", text="", icon='NORMALS_VERTEX')
        row.prop(overlay, "show_split_normals", text="", icon='NORMALS_VERTEX_FACE')
        row.prop(overlay, "show_face_normals", text="", icon='NORMALS_FACE')

        sub = row.row(align=True)
        sub.active = overlay.show_vertex_normals or overlay.show_face_normals or overlay.show_split_normals
        sub.prop(overlay, "normals_length", text="Size")


class VIEW3D_PT_overlay_edit_mesh_freestyle(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'
    bl_parent_id = 'VIEW3D_PT_overlay'
    bl_label = "Freestyle"

    @classmethod
    def poll(cls, context):
        return context.mode == 'EDIT_MESH' and bpy.app.build_options.freestyle

    def draw(self, context):
        layout = self.layout

        view = context.space_data
        overlay = view.overlay
        display_all = overlay.show_overlays

        col = layout.column()
        col.active = display_all

        row = col.row()
        row.prop(overlay, "show_freestyle_edge_marks", text="Edge Marks")
        row.prop(overlay, "show_freestyle_face_marks", text="Face Marks")


class VIEW3D_PT_overlay_edit_curve(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'
    bl_parent_id = 'VIEW3D_PT_overlay'
    bl_label = "Curve Edit Mode"

    @classmethod
    def poll(cls, context):
        return context.mode == 'EDIT_CURVE'

    def draw(self, context):
        layout = self.layout
        view = context.space_data
        overlay = view.overlay
        display_all = overlay.show_overlays

        col = layout.column()
        col.active = display_all

        row = col.row()
        row.prop(overlay, "show_curve_handles", text="Handles")

        row = col.row()
        row.prop(overlay, "show_curve_normals", text="")
        sub = row.row()
        sub.active = overlay.show_curve_normals
        sub.prop(overlay, "normals_length", text="Normals")


class VIEW3D_PT_overlay_sculpt(Panel):
    bl_space_type = 'VIEW_3D'
    bl_context = ".sculpt_mode"
    bl_region_type = 'HEADER'
    bl_parent_id = 'VIEW3D_PT_overlay'
    bl_label = "Sculpt"

    @classmethod
    def poll(cls, context):
        return (
            context.mode == 'SCULPT' and
            (context.sculpt_object and context.tool_settings.sculpt)
        )

    def draw(self, context):
        layout = self.layout
        tool_settings = context.tool_settings
        sculpt = tool_settings.sculpt

        view = context.space_data
        overlay = view.overlay

        layout.prop(sculpt, "show_diffuse_color")

        row = layout.row(align=True)
        row.prop(sculpt, "show_mask", text="")
        sub = row.row()
        sub.active = sculpt.show_mask
        sub.prop(overlay, "sculpt_mode_mask_opacity", text="Mask")


class VIEW3D_PT_overlay_pose(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'
    bl_parent_id = 'VIEW3D_PT_overlay'
    bl_label = "Pose Mode"

    @classmethod
    def poll(cls, context):
        mode = context.mode
        return (
            (mode == 'POSE') or
            (mode == 'PAINT_WEIGHT' and context.pose_object)
        )

    def draw(self, context):
        layout = self.layout
        view = context.space_data
        mode = context.mode
        overlay = view.overlay
        display_all = overlay.show_overlays

        col = layout.column()
        col.active = display_all

        if mode == 'POSE':
            row = col.row()
            row.prop(overlay, "show_xray_bone", text="")
            sub = row.row()
            sub.active = display_all and overlay.show_xray_bone
            sub.prop(overlay, "xray_alpha_bone", text="Fade Geometry")
        else:
            row = col.row()
            row.prop(overlay, "show_xray_bone")


class VIEW3D_PT_overlay_paint(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'
    bl_parent_id = 'VIEW3D_PT_overlay'
    bl_label = ""

    @classmethod
    def poll(cls, context):
        return context.mode in {'PAINT_WEIGHT', 'PAINT_VERTEX', 'PAINT_TEXTURE'}

    def draw_header(self, context):
        layout = self.layout
        layout.label(text={
            'PAINT_TEXTURE': "Texture Paint",
            'PAINT_VERTEX': "Vertex Paint",
            'PAINT_WEIGHT': "Weight Paint",
        }[context.mode])

    def draw(self, context):
        layout = self.layout
        view = context.space_data
        overlay = view.overlay
        display_all = overlay.show_overlays

        col = layout.column()
        col.active = display_all

        col.prop(overlay, {
            'PAINT_TEXTURE': "texture_paint_mode_opacity",
            'PAINT_VERTEX': "vertex_paint_mode_opacity",
            'PAINT_WEIGHT': "weight_paint_mode_opacity",
        }[context.mode], text="Opacity")

        if context.mode == 'PAINT_WEIGHT':
            row = col.split(factor=0.33)
            row.label(text="Zero Weights")
            sub = row.row()
            sub.prop(context.tool_settings, "vertex_group_user", expand=True)

            col.prop(overlay, "show_wpaint_contours")

        if context.mode in {'PAINT_WEIGHT', 'PAINT_VERTEX'}:
            col.prop(overlay, "show_paint_wire")


class VIEW3D_PT_pivot_point(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'
    bl_label = "Pivot Point"
    bl_ui_units_x = 8

    def draw(self, context):
        tool_settings = context.tool_settings
        obj = context.active_object
        mode = context.mode

        layout = self.layout
        col = layout.column()
        col.label(text="Pivot Point")
        col.prop(tool_settings, "transform_pivot_point", expand=True)

        if (obj is None) or (mode in {'OBJECT', 'POSE', 'WEIGHT_PAINT'}):
            col.separator()

            col.prop(tool_settings, "use_transform_pivot_point_align")


class VIEW3D_PT_snapping(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'
    bl_label = "Snapping"

    def draw(self, context):
        tool_settings = context.tool_settings
        snap_elements = tool_settings.snap_elements
        obj = context.active_object
        object_mode = 'OBJECT' if obj is None else obj.mode

        layout = self.layout
        col = layout.column()
        col.label(text="Snapping")
        col.prop(tool_settings, "snap_elements", expand=True)

        col.separator()
        if 'INCREMENT' in snap_elements:
            col.prop(tool_settings, "use_snap_grid_absolute")

        if snap_elements != {'INCREMENT'}:
            col.label(text="Target")
            row = col.row(align=True)
            row.prop(tool_settings, "snap_target", expand=True)

            if obj:
                if object_mode == 'EDIT':
                    col.prop(tool_settings, "use_snap_self")
                if object_mode in {'OBJECT', 'POSE', 'EDIT', 'WEIGHT_PAINT'}:
                    col.prop(tool_settings, "use_snap_align_rotation")

            if 'FACE' in snap_elements:
                col.prop(tool_settings, "use_snap_project")

            if 'VOLUME' in snap_elements:
                col.prop(tool_settings, "use_snap_peel_object")

        col.label(text="Affect")
        row = col.row(align=True)
        row.prop(tool_settings, "use_snap_translate", text="Move", toggle=True)
        row.prop(tool_settings, "use_snap_rotate", text="Rotate", toggle=True)
        row.prop(tool_settings, "use_snap_scale", text="Scale", toggle=True)


class VIEW3D_PT_proportional_edit(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'
    bl_label = "Proportional Editing"
    bl_ui_units_x = 8

    def draw(self, context):
        layout = self.layout
        tool_settings = context.tool_settings
        col = layout.column()

        if context.mode != 'OBJECT':
            col.prop(tool_settings, "use_proportional_connected")
            sub = col.column()
            sub.active = not tool_settings.use_proportional_connected
            sub.prop(tool_settings, "use_proportional_projected")
            col.separator()

        col.prop(tool_settings, "proportional_edit_falloff", expand=True)


class VIEW3D_PT_transform_orientations(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'
    bl_label = "Transform Orientations"
    bl_ui_units_x = 8

    def draw(self, context):
        layout = self.layout
        layout.label(text="Transform Orientations")

        scene = context.scene
        orient_slot = scene.transform_orientation_slots[0]
        orientation = orient_slot.custom_orientation

        row = layout.row()
        col = row.column()
        col.prop(orient_slot, "type", expand=True)
        row.operator("transform.create_orientation", text="", icon='ADD', emboss=False).use = True

        if orientation:
            row = layout.row(align=False)
            row.prop(orientation, "name", text="", icon='OBJECT_ORIGIN')
            row.operator("transform.delete_orientation", text="", icon='X', emboss=False)


class VIEW3D_PT_gpencil_origin(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'
    bl_label = "Stroke Placement"

    def draw(self, context):
        layout = self.layout
        tool_settings = context.tool_settings
        gpd = context.gpencil_data

        layout.label(text="Stroke Placement")

        row = layout.row()
        col = row.column()
        col.prop(tool_settings, "gpencil_stroke_placement_view3d", expand=True)

        if tool_settings.gpencil_stroke_placement_view3d == 'SURFACE':
            row = layout.row()
            row.label(text="Offset")
            row = layout.row()
            row.prop(gpd, "zdepth_offset", text="")

        if tool_settings.gpencil_stroke_placement_view3d == 'STROKE':
            row = layout.row()
            row.label(text="Target")
            row = layout.row()
            row.prop(tool_settings, "gpencil_stroke_snap_mode", expand=True)


class VIEW3D_PT_gpencil_lock(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'
    bl_label = "Drawing Plane"

    def draw(self, context):
        layout = self.layout
        layout.label(text="Drawing Plane")

        row = layout.row()
        col = row.column()
        col.prop(context.tool_settings.gpencil_sculpt, "lock_axis", expand=True)


class VIEW3D_PT_gpencil_guide(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'
    bl_label = "Guides"

    def draw(self, context):
        settings = context.tool_settings.gpencil_sculpt.guide

        layout = self.layout
        layout.label(text="Guides")

        col = layout.column()
        col.active = settings.use_guide
        col.prop(settings, "type", expand=True)

        if settings.type == 'PARALLEL':
            col.prop(settings, "angle")
            row = col.row(align=True)

        col.prop(settings, "use_snapping")
        if settings.use_snapping:

            if settings.type == 'RADIAL':
                col.prop(settings, "angle_snap")
            else:
                col.prop(settings, "spacing")

        if settings.type in {'CIRCULAR', 'RADIAL'}:
            col.label(text="Reference Point")
            row = col.row(align=True)
            row.prop(settings, "reference_point", expand=True)
            if settings.reference_point == 'CUSTOM':
                col.prop(settings, "location", text="Custom Location")
            elif settings.reference_point == 'OBJECT':
                col.prop(settings, "reference_object", text="Object Location")
                if not settings.reference_object:
                    col.label(text="No object selected, using cursor")


class VIEW3D_PT_overlay_gpencil_options(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'
    bl_parent_id = 'VIEW3D_PT_overlay'
    bl_label = ""

    @classmethod
    def poll(cls, context):
        return context.object and context.object.type == 'GPENCIL'

    def draw_header(self, context):
        layout = self.layout
        layout.label(text={
            'PAINT_GPENCIL': "Draw Grease Pencil",
            'EDIT_GPENCIL': "Edit Grease Pencil",
            'SCULPT_GPENCIL': "Sculpt Grease Pencil",
            'WEIGHT_GPENCIL': "Weight Grease Pencil",
            'OBJECT': "Grease Pencil",
        }[context.mode])

    def draw(self, context):
        layout = self.layout
        view = context.space_data
        overlay = view.overlay

        layout.prop(overlay, "use_gpencil_onion_skin", text="Onion Skin")

        col = layout.column()
        row = col.row()
        row.prop(overlay, "use_gpencil_grid", text="")
        sub = row.row()
        sub.active = overlay.use_gpencil_grid
        sub.prop(overlay, "gpencil_grid_opacity", text="Canvas", slider=True)

        row = col.row()
        row.prop(overlay, "use_gpencil_paper", text="")
        sub = row.row()
        sub.active = overlay.use_gpencil_paper
        sub.prop(overlay, "gpencil_paper_opacity", text="Fade 3D Objects", slider=True)

        if context.object.mode == 'PAINT_GPENCIL':
            row = col.row()
            row.prop(overlay, "use_gpencil_fade_layers", text="")
            sub = row.row()
            sub.active = overlay.use_gpencil_fade_layers
            sub.prop(overlay, "gpencil_fade_layer", text="Fade Layers", slider=True)

        if context.object.mode in {'EDIT_GPENCIL', 'SCULPT_GPENCIL', 'WEIGHT_GPENCIL'}:
            layout.prop(overlay, "use_gpencil_edit_lines", text="Edit Lines")
            layout.prop(overlay, "use_gpencil_multiedit_line_only", text="Show Edit Lines only in multiframe")
            layout.prop(overlay, "vertex_opacity", text="Vertex Opacity", slider=True)


class VIEW3D_PT_quad_view(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "View"
    bl_label = "Quad View"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        view = context.space_data
        return view.region_quadviews

    def draw(self, context):
        layout = self.layout

        view = context.space_data

        region = view.region_quadviews[2]
        col = layout.column()
        col.prop(region, "lock_rotation")
        row = col.row()
        row.enabled = region.lock_rotation
        row.prop(region, "show_sync_view")
        row = col.row()
        row.enabled = region.lock_rotation and region.show_sync_view
        row.prop(region, "use_box_clip")


# Annotation properties
class VIEW3D_PT_grease_pencil(AnnotationDataPanel, Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "View"

    # NOTE: this is just a wrapper around the generic GP Panel


class VIEW3D_PT_annotation_onion(AnnotationOnionSkin, Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "View"
    bl_parent_id = 'VIEW3D_PT_grease_pencil'

    # NOTE: this is just a wrapper around the generic GP Panel


class TOPBAR_PT_annotation_layers(Panel, AnnotationDataPanel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'
    bl_label = "Layers"
    bl_ui_units_x = 14


class VIEW3D_PT_view3d_stereo(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "View"
    bl_label = "Stereoscopy"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        scene = context.scene

        multiview = scene.render.use_multiview
        return multiview

    def draw(self, context):
        layout = self.layout
        view = context.space_data

        basic_stereo = context.scene.render.views_format == 'STEREO_3D'

        col = layout.column()
        col.row().prop(view, "stereo_3d_camera", expand=True)

        col.label(text="Display:")
        row = col.row()
        row.active = basic_stereo
        row.prop(view, "show_stereo_3d_cameras")
        row = col.row()
        row.active = basic_stereo
        split = row.split()
        split.prop(view, "show_stereo_3d_convergence_plane")
        split = row.split()
        split.prop(view, "stereo_3d_convergence_plane_alpha", text="Alpha")
        split.active = view.show_stereo_3d_convergence_plane
        row = col.row()
        split = row.split()
        split.prop(view, "show_stereo_3d_volume")
        split = row.split()
        split.active = view.show_stereo_3d_volume
        split.prop(view, "stereo_3d_volume_alpha", text="Alpha")


class VIEW3D_PT_context_properties(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "Item"
    bl_label = "Properties"
    bl_options = {'DEFAULT_CLOSED'}

    @staticmethod
    def _active_context_member(context):
        obj = context.object
        if obj:
            object_mode = obj.mode
            if object_mode == 'POSE':
                return "active_pose_bone"
            elif object_mode == 'EDIT' and obj.type == 'ARMATURE':
                return "active_bone"
            else:
                return "object"

        return ""

    @classmethod
    def poll(cls, context):
        import rna_prop_ui
        member = cls._active_context_member(context)

        if member:
            context_member, member = rna_prop_ui.rna_idprop_context_value(context, member, object)
            return context_member and rna_prop_ui.rna_idprop_has_properties(context_member)

        return False

    def draw(self, context):
        import rna_prop_ui
        member = VIEW3D_PT_context_properties._active_context_member(context)

        if member:
            # Draw with no edit button
            rna_prop_ui.draw(self.layout, context, member, object, False)


# Grease Pencil Object - Multiframe falloff tools
class VIEW3D_PT_gpencil_multi_frame(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'
    bl_label = "Multi Frame"

    def draw(self, context):
        gpd = context.gpencil_data
        settings = context.tool_settings.gpencil_sculpt

        layout = self.layout
        col = layout.column(align=True)
        col.prop(settings, "use_multiframe_falloff")

        # Falloff curve
        if gpd.use_multiedit and settings.use_multiframe_falloff:
            layout.template_curve_mapping(settings, "multiframe_falloff_curve", brush=True)


class VIEW3D_MT_gpencil_edit_context_menu(Menu):
    bl_label = "Edit Context Menu"

    def draw(self, context):
        layout = self.layout
        is_3d_view = context.space_data.type == 'VIEW_3D'

        layout.operator_context = 'INVOKE_REGION_WIN'

        # Add
        layout.operator("gpencil.stroke_subdivide", text="Subdivide")

        layout.separator()

        # Transform
        layout.operator("transform.transform", text="Shrink/Fatten").mode = 'GPENCIL_SHRINKFATTEN'
        layout.operator("gpencil.stroke_smooth", text="Smooth")
        layout.operator("gpencil.stroke_trim", text="Trim")

        layout.separator()

        # Modify
        layout.menu("VIEW3D_MT_assign_material")
        layout.operator_menu_enum("gpencil.stroke_arrange", "direction", text="Arrange Strokes")
        layout.operator("gpencil.stroke_flip", text="Flip Direction")
        layout.operator_menu_enum("gpencil.stroke_caps_set", text="Toggle Caps", property="type")

        layout.separator()

        layout.operator("gpencil.duplicate_move", text="Duplicate")
        layout.operator("gpencil.copy", text="Copy", icon='COPYDOWN')
        layout.operator("gpencil.paste", text="Paste", icon='PASTEDOWN').type = 'COPY'
        layout.operator("gpencil.paste", text="Paste & Merge").type = 'MERGE'
        layout.menu("VIEW3D_MT_gpencil_copy_layer")
        layout.operator("gpencil.frame_duplicate", text="Duplicate Active Frame")
        layout.operator("gpencil.frame_duplicate", text="Duplicate Active Frame All Layers").mode = 'ALL'

        layout.separator()

        layout.operator("gpencil.stroke_join", text="Join").type = 'JOIN'
        layout.operator("gpencil.stroke_join", text="Join & Copy").type = 'JOINCOPY'
        layout.menu("GPENCIL_MT_separate", text="Separate")
        layout.operator("gpencil.stroke_split", text="Split")
        op = layout.operator("gpencil.stroke_cyclical_set", text="Close")
        op.type = 'CLOSE'
        op.geometry = True

        layout.separator()

        layout.menu("VIEW3D_MT_mirror")
        layout.menu("VIEW3D_MT_snap")

        layout.separator()

        # Remove
        if is_3d_view:
            layout.menu("GPENCIL_MT_cleanup")

        layout.operator("gpencil.stroke_simplify_fixed", text="Simplify")
        layout.operator("gpencil.stroke_simplify", text="Simplify Adaptive")
        layout.operator("gpencil.stroke_merge", text="Merge")
        layout.menu("VIEW3D_MT_edit_gpencil_delete")


class VIEW3D_PT_gpencil_sculpt_context_menu(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'WINDOW'
    bl_label = "Sculpt Context Menu"

    def draw(self, context):
        brush = context.tool_settings.gpencil_sculpt.brush

        layout = self.layout

        if context.mode == 'WEIGHT_GPENCIL':
            layout.prop(brush, "weight")
        layout.prop(brush, "size", slider=True)
        layout.prop(brush, "strength")

        layout.separator()

        # Frames
        layout.label(text="Frames:")

        layout.operator_context = 'INVOKE_REGION_WIN'

        layout.operator("gpencil.blank_frame_add", text="Insert Blank", icon='ADD')
        layout.operator("gpencil.frame_duplicate", text="Duplicate Active", icon='DUPLICATE')
        layout.operator("gpencil.frame_duplicate", text="Duplicate for All Layers", icon='DUPLICATE').mode = 'ALL'

        layout.separator()

        layout.operator("gpencil.delete", text="Delete Active", icon='REMOVE').type = 'FRAME'
        layout.operator("gpencil.active_frames_delete_all", text="Delete All Active Layers", icon='REMOVE')


class VIEW3D_PT_gpencil_draw_context_menu(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'WINDOW'
    bl_label = "Draw Context Menu"

    def draw(self, context):
        brush = context.tool_settings.gpencil_paint.brush
        gp_settings = brush.gpencil_settings

        layout = self.layout

        if brush.gpencil_tool not in {'FILL', 'CUTTER'}:
            layout.prop(brush, "size", slider=True)
        if brush.gpencil_tool not in {'ERASE', 'FILL', 'CUTTER'}:
            layout.prop(gp_settings, "pen_strength")

        layout.separator()

        # Frames
        layout.label(text="Frames:")

        layout.operator_context = 'INVOKE_REGION_WIN'

        layout.operator("gpencil.blank_frame_add", text="Insert Blank", icon='ADD')
        layout.operator("gpencil.frame_duplicate", text="Duplicate Active", icon='DUPLICATE')
        layout.operator("gpencil.frame_duplicate", text="Duplicate for All Layers", icon='DUPLICATE').mode = 'ALL'

        layout.separator()

        layout.operator("gpencil.delete", text="Delete Active", icon='REMOVE').type = 'FRAME'
        layout.operator("gpencil.active_frames_delete_all", text="Delete All Active Layers", icon='REMOVE')


class VIEW3D_PT_paint_vertex_context_menu(Panel):
    # Only for popover, these are dummy values.
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'WINDOW'
    bl_label = "Vertex Paint Context Menu"

    def draw(self, context):
        layout = self.layout
        brush = context.tool_settings.vertex_paint.brush
        capabilities = brush.vertex_paint_capabilities

        if capabilities.has_color:
            split = layout.split(factor=0.1)
            UnifiedPaintPanel.prop_unified_color(split, context, brush, "color", text="")
            UnifiedPaintPanel.prop_unified_color_picker(split, context, brush, "color", value_slider=True)
            layout.prop(brush, "blend", text="")

        UnifiedPaintPanel.prop_unified_size(layout, context, brush, "size", slider=True)
        UnifiedPaintPanel.prop_unified_strength(layout, context, brush, "strength")


class VIEW3D_PT_paint_texture_context_menu(Panel):
    # Only for popover, these are dummy values.
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'WINDOW'
    bl_label = "Texture Paint Context Menu"

    def draw(self, context):
        layout = self.layout
        brush = context.tool_settings.image_paint.brush
        capabilities = brush.image_paint_capabilities

        if capabilities.has_color:
            split = layout.split(factor=0.1)
            UnifiedPaintPanel.prop_unified_color(split, context, brush, "color", text="")
            UnifiedPaintPanel.prop_unified_color_picker(split, context, brush, "color", value_slider=True)
            layout.prop(brush, "blend", text="")

        if capabilities.has_radius:
            UnifiedPaintPanel.prop_unified_size(layout, context, brush, "size", slider=True)
        UnifiedPaintPanel.prop_unified_strength(layout, context, brush, "strength")


class VIEW3D_PT_paint_weight_context_menu(Panel):
    # Only for popover, these are dummy values.
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'WINDOW'
    bl_label = "Weights Context Menu"

    def draw(self, context):
        layout = self.layout

        brush = context.tool_settings.weight_paint.brush
        UnifiedPaintPanel.prop_unified_weight(layout, context, brush, "weight", slider=True)
        UnifiedPaintPanel.prop_unified_size(layout, context, brush, "size", slider=True)
        UnifiedPaintPanel.prop_unified_strength(layout, context, brush, "strength")


class VIEW3D_PT_sculpt_context_menu(Panel):
    # Only for popover, these are dummy values.
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'WINDOW'
    bl_label = "Sculpt Context Menu"

    def draw(self, context):
        layout = self.layout

        brush = context.tool_settings.sculpt.brush
        capabilities = brush.sculpt_capabilities

        UnifiedPaintPanel.prop_unified_size(layout, context, brush, "size", slider=True)
        UnifiedPaintPanel.prop_unified_strength(layout, context, brush, "strength")

        if capabilities.has_auto_smooth:
            layout.prop(brush, "auto_smooth_factor", slider=True)

        if capabilities.has_normal_weight:
            layout.prop(brush, "normal_weight", slider=True)

        if capabilities.has_pinch_factor:
            layout.prop(brush, "crease_pinch_factor", slider=True, text="Pinch")

        if capabilities.has_rake_factor:
            layout.prop(brush, "rake_factor", slider=True)

        if capabilities.has_plane_offset:
            layout.prop(brush, "plane_offset", slider=True)
            layout.prop(brush, "plane_trim", slider=True, text="Distance")

        if capabilities.has_height:
            layout.prop(brush, "height", slider=True, text="Height")


class TOPBAR_PT_gpencil_materials(GreasePencilMaterialsPanel, Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'
    bl_label = "Materials"
    bl_ui_units_x = 14

    @classmethod
    def poll(cls, context):
        ob = context.object
        return ob and ob.type == 'GPENCIL'


classes = (
    VIEW3D_HT_header,
    VIEW3D_HT_tool_header,
    VIEW3D_MT_editor_menus,
    VIEW3D_MT_transform,
    VIEW3D_MT_transform_base,
    VIEW3D_MT_transform_object,
    VIEW3D_MT_transform_armature,
    VIEW3D_MT_mirror,
    VIEW3D_MT_snap,
    VIEW3D_MT_uv_map,
    VIEW3D_MT_view,
    VIEW3D_MT_view_local,
    VIEW3D_MT_view_cameras,
    VIEW3D_MT_view_navigation,
    VIEW3D_MT_view_align,
    VIEW3D_MT_view_align_selected,
    VIEW3D_MT_view_viewpoint,
    VIEW3D_MT_view_regions,
    VIEW3D_MT_select_object,
    VIEW3D_MT_select_object_more_less,
    VIEW3D_MT_select_pose,
    VIEW3D_MT_select_pose_more_less,
    VIEW3D_MT_select_particle,
    VIEW3D_MT_edit_mesh,
    VIEW3D_MT_edit_mesh_select_similar,
    VIEW3D_MT_edit_mesh_select_by_trait,
    VIEW3D_MT_edit_mesh_select_more_less,
    VIEW3D_MT_select_edit_mesh,
    VIEW3D_MT_select_edit_curve,
    VIEW3D_MT_select_edit_surface,
    VIEW3D_MT_edit_text_context_menu,
    VIEW3D_MT_select_edit_text,
    VIEW3D_MT_select_edit_metaball,
    VIEW3D_MT_edit_lattice_context_menu,
    VIEW3D_MT_select_edit_lattice,
    VIEW3D_MT_select_edit_armature,
    VIEW3D_MT_select_gpencil,
    VIEW3D_MT_select_paint_mask,
    VIEW3D_MT_select_paint_mask_vertex,
    VIEW3D_MT_angle_control,
    VIEW3D_MT_mesh_add,
    VIEW3D_MT_curve_add,
    VIEW3D_MT_surface_add,
    VIEW3D_MT_edit_metaball_context_menu,
    VIEW3D_MT_metaball_add,
    TOPBAR_MT_edit_curve_add,
    TOPBAR_MT_edit_armature_add,
    VIEW3D_MT_armature_add,
    VIEW3D_MT_light_add,
    VIEW3D_MT_lightprobe_add,
    VIEW3D_MT_camera_add,
    VIEW3D_MT_add,
    VIEW3D_MT_image_add,
    VIEW3D_MT_object,
    VIEW3D_MT_object_animation,
    VIEW3D_MT_object_rigid_body,
    VIEW3D_MT_object_clear,
    VIEW3D_MT_object_context_menu,
    VIEW3D_MT_object_shading,
    VIEW3D_MT_object_apply,
    VIEW3D_MT_object_relations,
    VIEW3D_MT_object_parent,
    VIEW3D_MT_object_track,
    VIEW3D_MT_object_collection,
    VIEW3D_MT_object_constraints,
    VIEW3D_MT_object_quick_effects,
    VIEW3D_MT_object_showhide,
    VIEW3D_MT_make_single_user,
    VIEW3D_MT_make_links,
    VIEW3D_MT_brush,
    VIEW3D_MT_brush_paint_modes,
    VIEW3D_MT_paint_vertex,
    VIEW3D_MT_hook,
    VIEW3D_MT_vertex_group,
    VIEW3D_MT_paint_weight,
    VIEW3D_MT_sculpt,
    VIEW3D_MT_hide_mask,
    VIEW3D_MT_particle,
    VIEW3D_MT_particle_context_menu,
    VIEW3D_MT_particle_showhide,
    VIEW3D_MT_pose,
    VIEW3D_MT_pose_transform,
    VIEW3D_MT_pose_slide,
    VIEW3D_MT_pose_propagate,
    VIEW3D_MT_pose_library,
    VIEW3D_MT_pose_motion,
    VIEW3D_MT_pose_group,
    VIEW3D_MT_pose_ik,
    VIEW3D_MT_pose_constraints,
    VIEW3D_MT_pose_showhide,
    VIEW3D_MT_pose_apply,
    VIEW3D_MT_pose_context_menu,
    VIEW3D_MT_bone_options_toggle,
    VIEW3D_MT_bone_options_enable,
    VIEW3D_MT_bone_options_disable,
    VIEW3D_MT_edit_mesh_context_menu,
    VIEW3D_MT_edit_mesh_select_mode,
    VIEW3D_MT_edit_mesh_select_linked,
    VIEW3D_MT_edit_mesh_select_loops,
    VIEW3D_MT_edit_mesh_extrude,
    VIEW3D_MT_edit_mesh_vertices,
    VIEW3D_MT_edit_mesh_edges,
    VIEW3D_MT_edit_mesh_edges_data,
    VIEW3D_MT_edit_mesh_faces,
    VIEW3D_MT_edit_mesh_faces_data,
    VIEW3D_MT_edit_mesh_normals,
    VIEW3D_MT_edit_mesh_normals_select_strength,
    VIEW3D_MT_edit_mesh_normals_set_strength,
    VIEW3D_MT_edit_mesh_normals_average,
    VIEW3D_MT_edit_mesh_shading,
    VIEW3D_MT_edit_mesh_weights,
    VIEW3D_MT_edit_mesh_clean,
    VIEW3D_MT_edit_mesh_delete,
    VIEW3D_MT_edit_mesh_merge,
    VIEW3D_MT_edit_mesh_showhide,
    VIEW3D_MT_paint_gpencil,
    VIEW3D_MT_assign_material,
    VIEW3D_MT_edit_gpencil,
    VIEW3D_MT_edit_gpencil_delete,
    VIEW3D_MT_weight_gpencil,
    VIEW3D_MT_gpencil_animation,
    VIEW3D_MT_gpencil_simplify,
    VIEW3D_MT_gpencil_copy_layer,
    VIEW3D_MT_gpencil_autoweights,
    VIEW3D_MT_gpencil_edit_context_menu,
    VIEW3D_MT_edit_curve,
    VIEW3D_MT_edit_curve_ctrlpoints,
    VIEW3D_MT_edit_curve_segments,
    VIEW3D_MT_edit_curve_clean,
    VIEW3D_MT_edit_curve_context_menu,
    VIEW3D_MT_edit_curve_delete,
    VIEW3D_MT_edit_curve_showhide,
    VIEW3D_MT_edit_surface,
    VIEW3D_MT_edit_font,
    VIEW3D_MT_edit_text_chars,
    VIEW3D_MT_edit_meta,
    VIEW3D_MT_edit_meta_showhide,
    VIEW3D_MT_edit_lattice,
    VIEW3D_MT_edit_armature,
    VIEW3D_MT_armature_context_menu,
    VIEW3D_MT_edit_armature_parent,
    VIEW3D_MT_edit_armature_roll,
    VIEW3D_MT_edit_armature_names,
    VIEW3D_MT_edit_armature_delete,
    VIEW3D_MT_edit_gpencil_transform,
    VIEW3D_MT_edit_gpencil_interpolate,
    VIEW3D_MT_object_mode_pie,
    VIEW3D_MT_view_pie,
    VIEW3D_MT_transform_gizmo_pie,
    VIEW3D_MT_shading_pie,
    VIEW3D_MT_shading_ex_pie,
    VIEW3D_MT_pivot_pie,
    VIEW3D_MT_snap_pie,
    VIEW3D_MT_orientations_pie,
    VIEW3D_MT_proportional_editing_falloff_pie,
    VIEW3D_PT_active_tool,
    VIEW3D_PT_active_tool_duplicate,
    VIEW3D_PT_view3d_properties,
    VIEW3D_PT_view3d_lock,
    VIEW3D_PT_view3d_cursor,
    VIEW3D_PT_collections,
    VIEW3D_PT_object_type_visibility,
    VIEW3D_PT_grease_pencil,
    VIEW3D_PT_annotation_onion,
    VIEW3D_PT_gpencil_multi_frame,
    VIEW3D_PT_quad_view,
    VIEW3D_PT_view3d_stereo,
    VIEW3D_PT_shading,
    VIEW3D_PT_shading_lighting,
    VIEW3D_PT_shading_color,
    VIEW3D_PT_shading_options,
    VIEW3D_PT_shading_options_shadow,
    VIEW3D_PT_shading_options_ssao,
    VIEW3D_PT_gizmo_display,
    VIEW3D_PT_overlay,
    VIEW3D_PT_overlay_guides,
    VIEW3D_PT_overlay_object,
    VIEW3D_PT_overlay_geometry,
    VIEW3D_PT_overlay_motion_tracking,
    VIEW3D_PT_overlay_edit_mesh,
    VIEW3D_PT_overlay_edit_mesh_shading,
    VIEW3D_PT_overlay_edit_mesh_measurement,
    VIEW3D_PT_overlay_edit_mesh_normals,
    VIEW3D_PT_overlay_edit_mesh_freestyle,
    VIEW3D_PT_overlay_edit_curve,
    VIEW3D_PT_overlay_paint,
    VIEW3D_PT_overlay_pose,
    VIEW3D_PT_overlay_sculpt,
    VIEW3D_PT_pivot_point,
    VIEW3D_PT_snapping,
    VIEW3D_PT_proportional_edit,
    VIEW3D_PT_gpencil_origin,
    VIEW3D_PT_gpencil_lock,
    VIEW3D_PT_gpencil_guide,
    VIEW3D_PT_transform_orientations,
    VIEW3D_PT_overlay_gpencil_options,
    VIEW3D_PT_context_properties,
    VIEW3D_PT_paint_vertex_context_menu,
    VIEW3D_PT_paint_texture_context_menu,
    VIEW3D_PT_paint_weight_context_menu,
    VIEW3D_PT_gpencil_sculpt_context_menu,
    VIEW3D_PT_gpencil_draw_context_menu,
    VIEW3D_PT_sculpt_context_menu,
    TOPBAR_PT_gpencil_materials,
    TOPBAR_PT_annotation_layers,
)


if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
