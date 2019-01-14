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

# For now group all tools together
# we may want to move these into per space-type files.
#
# For now keep this in a single file since it's an area that may change,
# so avoid making changes all over the place.

import bpy
from bpy.types import Panel

from .space_toolsystem_common import (
    ToolSelectPanelHelper,
    ToolDef,
)


def kmi_to_string_or_none(kmi):
    return kmi.to_string() if kmi else "<none>"


def generate_from_enum_ex(
        context, *,
        icon_prefix,
        type,
        attr,
        tooldef_keywords={},
):
    tool_defs = []
    for enum in type.bl_rna.properties[attr].enum_items_static:
        name = enum.name
        identifier = enum.identifier
        tool_defs.append(
            ToolDef.from_dict(
                dict(
                    text=name,
                    icon=icon_prefix + identifier.lower(),
                    data_block=identifier,
                    **tooldef_keywords,
                )
            )
        )
    return tuple(tool_defs)


# Use for shared widget data.
class _template_widget:
    class VIEW3D_GGT_xform_extrude:
        @staticmethod
        def draw_settings(context, layout, tool):
            props = tool.gizmo_group_properties("VIEW3D_GGT_xform_extrude")
            layout.prop(props, "axis_type", expand=True)

    class TRANSFORM_GGT_gizmo:
        @staticmethod
        def draw_settings_with_index(context, layout, index):
            scene = context.scene
            orient_slot = scene.transform_orientation_slots[index]
            layout.prop(orient_slot, "type")


class _defs_view3d_generic:
    @ToolDef.from_fn
    def cursor():
        def draw_settings(context, layout, tool):
            props = tool.operator_properties("view3d.cursor3d")
            layout.prop(props, "use_depth")
            layout.prop(props, "orientation")
        return dict(
            text="Cursor",
            description=(
                "Set the cursor location, drag to transform"
            ),
            icon="ops.generic.cursor",
            keymap="3D View Tool: Cursor",
            draw_settings=draw_settings,
        )

    @ToolDef.from_fn
    def cursor_click():
        return dict(
            text="None",
            icon="ops.generic.cursor",
            keymap=(),
        )

    @ToolDef.from_fn
    def ruler():
        def description(context, item, km):
            if km is not None:
                kmi = km.keymap_items.find_from_operator("view3d.ruler_add")
            else:
                kmi = None
            return (
                "Measure distance and angles.\n"
                "\u2022 {} anywhere for new measurement.\n"
                "\u2022 Drag ruler segment to measure an angle.\n"
                "\u2022 Drag ruler outside the view to remove.\n"
                "\u2022 Ctrl to snap.\n"
                "\u2022 Shift to measure surface thickness"
            ).format(
                kmi_to_string_or_none(kmi)
            )
        return dict(
            text="Measure",
            description=description,
            icon="ops.view3d.ruler",
            widget="VIEW3D_GGT_ruler",
            keymap="3D View Tool: Measure",
        )


class _defs_annotate:

    def draw_settings_common(context, layout, tool):
        if type(context.gpencil_data_owner) is bpy.types.Object:
            gpd = context.scene.grease_pencil
        else:
            gpd = context.gpencil_data

        if gpd is not None:
            if gpd.layers.active_note is not None:
                text = gpd.layers.active_note
                maxw = 25
                if len(text) > maxw:
                    text = text[:maxw - 5] + '..' + text[-3:]
            else:
                text = ""

            layout.label(text="Annotation:")
            gpl = context.active_gpencil_layer
            if gpl is not None:
                sub = layout.row(align=True)
                sub.ui_units_x = 8

                sub.prop(gpl, "color", text="")
                sub.popover(
                    panel="TOPBAR_PT_annotation_layers",
                    text=text,
                )

        tool_settings = context.tool_settings
        space_type = tool.space_type
        if space_type == 'VIEW_3D':
            layout.separator()

            row = layout.row(align=True)
            row.prop(tool_settings, "annotation_stroke_placement_view3d", text="Placement")
            if tool_settings.gpencil_stroke_placement_view3d == 'CURSOR':
                row.prop(tool_settings.gpencil_sculpt, "lockaxis")
            elif tool_settings.gpencil_stroke_placement_view3d in {'SURFACE', 'STROKE'}:
                row.prop(tool_settings, "use_gpencil_stroke_endpoints")

    @ToolDef.from_fn.with_args(draw_settings=draw_settings_common)
    def scribble(*, draw_settings):
        return dict(
            text="Annotate",
            icon="ops.gpencil.draw",
            cursor='PAINT_BRUSH',
            keymap="Generic Tool: Annotate",
            draw_settings=draw_settings,
        )

    @ToolDef.from_fn.with_args(draw_settings=draw_settings_common)
    def line(*, draw_settings):
        return dict(
            text="Annotate Line",
            icon="ops.gpencil.draw.line",
            cursor='CROSSHAIR',
            keymap="Generic Tool: Annotate Line",
            draw_settings=draw_settings,
        )

    @ToolDef.from_fn.with_args(draw_settings=draw_settings_common)
    def poly(*, draw_settings):
        return dict(
            text="Annotate Polygon",
            icon="ops.gpencil.draw.poly",
            cursor='CROSSHAIR',
            keymap="Generic Tool: Annotate Polygon",
            draw_settings=draw_settings,
        )

    @ToolDef.from_fn
    def eraser():
        def draw_settings(context, layout, tool):
            # TODO: Move this setting to tool_settings
            prefs = context.preferences
            layout.prop(prefs.edit, "grease_pencil_eraser_radius", text="Radius")
        return dict(
            text="Annotate Eraser",
            icon="ops.gpencil.draw.eraser",
            cursor='CROSSHAIR',  # XXX: Always show brush circle when enabled
            keymap="Generic Tool: Annotate Eraser",
            draw_settings=draw_settings,
        )


class _defs_transform:

    @ToolDef.from_fn
    def translate():
        def draw_settings(context, layout, tool):
            _template_widget.TRANSFORM_GGT_gizmo.draw_settings_with_index(context, layout, 1)
        return dict(
            text="Move",
            # cursor='SCROLL_XY',
            icon="ops.transform.translate",
            widget="TRANSFORM_GGT_gizmo",
            operator="transform.translate",
            keymap="3D View Tool: Move",
            draw_settings=draw_settings,
        )

    @ToolDef.from_fn
    def rotate():
        def draw_settings(context, layout, tool):
            _template_widget.TRANSFORM_GGT_gizmo.draw_settings_with_index(context, layout, 2)
        return dict(
            text="Rotate",
            # cursor='SCROLL_XY',
            icon="ops.transform.rotate",
            widget="TRANSFORM_GGT_gizmo",
            operator="transform.rotate",
            keymap="3D View Tool: Rotate",
            draw_settings=draw_settings,
        )

    @ToolDef.from_fn
    def scale():
        def draw_settings(context, layout, tool):
            _template_widget.TRANSFORM_GGT_gizmo.draw_settings_with_index(context, layout, 3)
        return dict(
            text="Scale",
            # cursor='SCROLL_XY',
            icon="ops.transform.resize",
            widget="TRANSFORM_GGT_gizmo",
            operator="transform.resize",
            keymap="3D View Tool: Scale",
            draw_settings=draw_settings,
        )

    @ToolDef.from_fn
    def scale_cage():
        def draw_settings(context, layout, tool):
            _template_widget.TRANSFORM_GGT_gizmo.draw_settings_with_index(context, layout, 3)
        return dict(
            text="Scale Cage",
            icon="ops.transform.resize.cage",
            widget="VIEW3D_GGT_xform_cage",
            operator="transform.resize",
            draw_settings=draw_settings,
        )

    @ToolDef.from_fn
    def transform():
        def draw_settings(context, layout, tool):
            layout.label(text="Gizmos:")
            tool_settings = context.tool_settings
            layout.prop(tool_settings, "use_gizmo_mode")

            props = tool.gizmo_group_properties("TRANSFORM_GGT_gizmo")
            layout.prop(props, "drag_action")

            _template_widget.TRANSFORM_GGT_gizmo.draw_settings_with_index(context, layout, 1)

        return dict(
            text="Transform",
            description=(
                "Supports any combination of grab, rotate & scale at once"
            ),
            icon="ops.transform.transform",
            widget="TRANSFORM_GGT_gizmo",
            keymap="3D View Tool: Transform",
            draw_settings=draw_settings,
        )


class _defs_view3d_select:

    @ToolDef.from_fn
    def select():
        def draw_settings(context, layout, tool):
            pass
        return dict(
            text="Select",
            icon="ops.generic.select",
            widget=None,
            keymap="3D View Tool: Select",
            draw_settings=draw_settings,
        )

    @ToolDef.from_fn
    def box():
        def draw_settings(context, layout, tool):
            props = tool.operator_properties("view3d.select_box")
            layout.prop(props, "mode", expand=True)
        return dict(
            text="Select Box",
            icon="ops.generic.select_box",
            widget=None,
            keymap="3D View Tool: Select Box",
            draw_settings=draw_settings,
        )

    @ToolDef.from_fn
    def lasso():
        def draw_settings(context, layout, tool):
            props = tool.operator_properties("view3d.select_lasso")
            layout.prop(props, "mode", expand=True)
        return dict(
            text="Select Lasso",
            icon="ops.generic.select_lasso",
            widget=None,
            keymap="3D View Tool: Select Lasso",
            draw_settings=draw_settings,
        )

    @ToolDef.from_fn
    def circle():
        def draw_settings(context, layout, tool):
            props = tool.operator_properties("view3d.select_circle")
            layout.prop(props, "radius")

        def draw_cursor(context, tool, xy):
            from gpu_extras.presets import draw_circle_2d
            props = tool.operator_properties("view3d.select_circle")
            radius = props.radius
            draw_circle_2d(xy, (1.0,) * 4, radius, 32)

        return dict(
            text="Select Circle",
            icon="ops.generic.select_circle",
            widget=None,
            keymap="3D View Tool: Select Circle",
            draw_settings=draw_settings,
            draw_cursor=draw_cursor,
        )


# -----------------------------------------------------------------------------
# Object Modes (named based on context.mode)


class _defs_edit_armature:

    @ToolDef.from_fn
    def roll():
        return dict(
            text="Roll",
            icon="ops.armature.bone.roll",
            widget=None,
            keymap=(),
        )

    @ToolDef.from_fn
    def bone_envelope():
        return dict(
            text="Bone Envelope",
            icon="ops.transform.bone_envelope",
            widget=None,
            keymap=(),
        )

    @ToolDef.from_fn
    def bone_size():
        return dict(
            text="Bone Size",
            icon="ops.transform.bone_size",
            widget=None,
            keymap=(),
        )

    @ToolDef.from_fn
    def extrude():
        return dict(
            text="Extrude",
            icon="ops.armature.extrude_move",
            widget="VIEW3D_GGT_xform_extrude",
            keymap=(),
            draw_settings=_template_widget.VIEW3D_GGT_xform_extrude.draw_settings,
        )

    @ToolDef.from_fn
    def extrude_cursor():
        return dict(
            text="Extrude to Cursor",
            icon="ops.armature.extrude_cursor",
            widget=None,
            keymap=(),
        )


class _defs_edit_mesh:

    @ToolDef.from_fn
    def cube_add():
        return dict(
            text="Add Cube",
            icon="ops.mesh.primitive_cube_add_gizmo",
            description=(
                "Add cube to mesh interactively"
            ),
            widget=None,
            keymap=(),
        )

    @ToolDef.from_fn
    def rip_region():
        def draw_settings(context, layout, tool):
            props = tool.operator_properties("mesh.rip_move")
            props_macro = props.MESH_OT_rip
            layout.prop(props_macro, "use_fill")

        return dict(
            text="Rip Region",
            icon="ops.mesh.rip",
            widget=None,
            keymap=(),
            draw_settings=draw_settings,
        )

    @ToolDef.from_fn
    def rip_edge():
        return dict(
            text="Rip Edge",
            icon="ops.mesh.rip_edge",
            widget=None,
            keymap=(),
        )

    @ToolDef.from_fn
    def poly_build():
        return dict(
            text="Poly Build",
            icon="ops.mesh.polybuild_hover",
            widget="VIEW3D_GGT_mesh_preselect_elem",
            keymap=(),
        )

    @ToolDef.from_fn
    def edge_slide():
        def draw_settings(context, layout, tool):
            props = tool.operator_properties("transform.edge_slide")
            layout.prop(props, "correct_uv")

        return dict(
            text="Edge Slide",
            icon="ops.transform.edge_slide",
            widget=None,
            keymap=(),
            draw_settings=draw_settings,
        )

    @ToolDef.from_fn
    def vert_slide():
        def draw_settings(context, layout, tool):
            props = tool.operator_properties("transform.vert_slide")
            layout.prop(props, "correct_uv")

        return dict(
            text="Vertex Slide",
            icon="ops.transform.vert_slide",
            widget=None,
            keymap=(),
            draw_settings=draw_settings,
        )

    @ToolDef.from_fn
    def spin():
        def draw_settings(context, layout, tool):
            props = tool.operator_properties("mesh.spin")
            layout.prop(props, "steps")
            props = tool.gizmo_group_properties("MESH_GGT_spin")
            layout.prop(props, "axis")

        return dict(
            text="Spin",
            icon="ops.mesh.spin",
            widget="MESH_GGT_spin",
            keymap=(),
            draw_settings=draw_settings,
        )

    @ToolDef.from_fn
    def spin_duplicate():
        def draw_settings(context, layout, tool):
            props = tool.operator_properties("mesh.spin")
            layout.prop(props, "steps")
            props = tool.gizmo_group_properties("MESH_GGT_spin")
            layout.prop(props, "axis")

        return dict(
            text="Spin (Duplicate)",
            icon="ops.mesh.spin.duplicate",
            widget="MESH_GGT_spin",
            keymap=(),
            draw_settings=draw_settings,
        )

    @ToolDef.from_fn
    def inset():
        def draw_settings(context, layout, tool):
            props = tool.operator_properties("mesh.inset")
            layout.prop(props, "use_outset")
            layout.prop(props, "use_individual")
            layout.prop(props, "use_even_offset")
            layout.prop(props, "use_relative_offset")

        return dict(
            text="Inset Faces",
            icon="ops.mesh.inset",
            widget=None,
            keymap=(),
            draw_settings=draw_settings,
        )

    @ToolDef.from_fn
    def bevel():
        def draw_settings(context, layout, tool):
            props = tool.operator_properties("mesh.bevel")
            layout.prop(props, "offset_type")
            layout.prop(props, "segments")
            layout.prop(props, "profile", slider=True)
            layout.prop(props, "vertex_only")

        return dict(
            text="Bevel",
            icon="ops.mesh.bevel",
            widget=None,
            keymap=(),
            draw_settings=draw_settings,
        )

    @ToolDef.from_fn
    def extrude():
        return dict(
            text="Extrude Region",
            # The operator description isn't useful in this case, give our own.
            description=(
                "Extrude freely or along an axis"
            ),
            icon="ops.mesh.extrude_region_move",
            widget="VIEW3D_GGT_xform_extrude",
            # Important to use same operator as 'E' key.
            operator="view3d.edit_mesh_extrude_move_normal",
            keymap=(),
            draw_settings=_template_widget.VIEW3D_GGT_xform_extrude.draw_settings,
        )

    @ToolDef.from_fn
    def extrude_normals():
        def draw_settings(context, layout, tool):
            props = tool.operator_properties("mesh.extrude_region_shrink_fatten")
            props_macro = props.TRANSFORM_OT_shrink_fatten
            layout.prop(props_macro, "use_even_offset")
        return dict(
            text="Extrude Along Normals",
            icon="ops.mesh.extrude_region_shrink_fatten",
            widget=None,
            operator="mesh.extrude_region_shrink_fatten",
            keymap=(),
            draw_settings=draw_settings,
        )

    @ToolDef.from_fn
    def extrude_individual():
        return dict(
            text="Extrude Individual",
            icon="ops.mesh.extrude_faces_move",
            widget=None,
            keymap=(),
        )

    @ToolDef.from_fn
    def extrude_cursor():
        def draw_settings(context, layout, tool):
            props = tool.operator_properties("mesh.dupli_extrude_cursor")
            layout.prop(props, "rotate_source")

        return dict(
            text="Extrude to Cursor",
            icon="ops.mesh.dupli_extrude_cursor",
            widget=None,
            keymap=(),
            draw_settings=draw_settings,
        )

    @ToolDef.from_fn
    def loopcut_slide():

        def draw_settings(context, layout, tool):
            props = tool.operator_properties("mesh.loopcut_slide")
            props_macro = props.MESH_OT_loopcut
            layout.prop(props_macro, "number_cuts")
            props_macro = props.TRANSFORM_OT_edge_slide
            layout.prop(props_macro, "correct_uv")

        return dict(
            text="Loop Cut",
            icon="ops.mesh.loopcut_slide",
            widget="VIEW3D_GGT_mesh_preselect_edgering",
            keymap=(),
            draw_settings=draw_settings,
        )

    @ToolDef.from_fn
    def offset_edge_loops_slide():
        return dict(
            text="Offset Edge Loop Cut",
            icon="ops.mesh.offset_edge_loops_slide",
            widget=None,
            keymap=(),
        )

    @ToolDef.from_fn
    def vertex_smooth():
        def draw_settings(context, layout, tool):
            props = tool.operator_properties("mesh.vertices_smooth")
            layout.prop(props, "repeat")
        return dict(
            text="Smooth",
            icon="ops.mesh.vertices_smooth",
            widget="WM_GGT_value_operator_redo",
            keymap=(),
            draw_settings=draw_settings,
        )

    @ToolDef.from_fn
    def vertex_randomize():
        def draw_settings(context, layout, tool):
            props = tool.operator_properties("transform.vertex_random")
            layout.prop(props, "uniform")
            layout.prop(props, "normal")
            layout.prop(props, "seed")
        return dict(
            text="Randomize",
            icon="ops.transform.vertex_random",
            widget="WM_GGT_value_operator_redo",
            keymap=(),
            draw_settings=draw_settings,
        )

    @ToolDef.from_fn
    def shear():
        def draw_settings(context, layout, tool):
            props = tool.operator_properties("transform.shear")
            layout.label(text="View Axis:")
            layout.prop(props, "shear_axis", expand=True)
            _template_widget.TRANSFORM_GGT_gizmo.draw_settings_with_index(context, layout, 2)
        return dict(
            text="Shear",
            icon="ops.transform.shear",
            widget="VIEW3D_GGT_xform_shear",
            keymap=(),
            draw_settings=draw_settings,
        )

    @ToolDef.from_fn
    def tosphere():
        return dict(
            text="To Sphere",
            icon="ops.transform.tosphere",
            widget=None,
            keymap=(),
        )

    @ToolDef.from_fn
    def shrink_fatten():
        def draw_settings(context, layout, tool):
            props = tool.operator_properties("transform.shrink_fatten")
            layout.prop(props, "use_even_offset")

        return dict(
            text="Shrink/Fatten",
            icon="ops.transform.shrink_fatten",
            widget=None,
            keymap=(),
            draw_settings=draw_settings,
        )

    @ToolDef.from_fn
    def push_pull():
        return dict(
            text="Push/Pull",
            icon="ops.transform.push_pull",
            widget=None,
            keymap=(),
        )

    @ToolDef.from_fn
    def knife():
        def draw_settings(context, layout, tool):
            props = tool.operator_properties("mesh.knife_tool")
            layout.prop(props, "use_occlude_geometry")
            layout.prop(props, "only_selected")

        return dict(
            text="Knife",
            icon="ops.mesh.knife_tool",
            widget=None,
            keymap=(),
            draw_settings=draw_settings,
        )

    @ToolDef.from_fn
    def bisect():
        def draw_settings(context, layout, tool):
            props = tool.operator_properties("mesh.bisect")
            layout.prop(props, "use_fill")
            layout.prop(props, "clear_inner")
            layout.prop(props, "clear_outer")
            layout.prop(props, "threshold")
        return dict(
            text="Bisect",
            icon="ops.mesh.bisect",
            widget=None,
            keymap=(),
            draw_settings=draw_settings,
        )


class _defs_edit_curve:

    @ToolDef.from_fn
    def draw():
        def draw_settings(context, layout, tool):
            # Tool settings initialize operator options.
            tool_settings = context.tool_settings
            cps = tool_settings.curve_paint_settings

            col = layout.row()

            col.prop(cps, "curve_type")

            if cps.curve_type == 'BEZIER':
                col.prop(cps, "error_threshold")
                col.prop(cps, "fit_method")
                col.prop(cps, "use_corners_detect")

                col = layout.row()
                col.active = cps.use_corners_detect
                col.prop(cps, "corner_angle")

        return dict(
            text="Draw",
            cursor='PAINT_BRUSH',
            icon="ops.curve.draw",
            widget=None,
            keymap=(),
            draw_settings=draw_settings,
        )

    @ToolDef.from_fn
    def extrude():
        return dict(
            text="Extrude",
            icon="ops.curve.extrude_move",
            widget="VIEW3D_GGT_xform_extrude",
            keymap=(),
            draw_settings=_template_widget.VIEW3D_GGT_xform_extrude.draw_settings,
        )

    @ToolDef.from_fn
    def extrude_cursor():
        return dict(
            text="Extrude Cursor",
            icon="ops.curve.extrude_cursor",
            widget=None,
            keymap=(),
        )

    @ToolDef.from_fn
    def tilt():
        return dict(
            text="Tilt",
            icon="ops.transform.tilt",
            widget=None,
            keymap=(),
        )


class _defs_pose:

    @ToolDef.from_fn
    def breakdown():
        return dict(
            text="Breakdowner",
            icon="ops.pose.breakdowner",
            widget=None,
            keymap=(),
        )

    @ToolDef.from_fn
    def push():
        return dict(
            text="Push",
            icon="ops.pose.push",
            widget=None,
            keymap=(),
        )

    @ToolDef.from_fn
    def relax():
        return dict(
            text="Relax",
            icon="ops.pose.relax",
            widget=None,
            keymap=(),
        )


class _defs_particle:

    @staticmethod
    def generate_from_brushes(context):
        return generate_from_enum_ex(
            context,
            icon_prefix="brush.particle.",
            type=bpy.types.ParticleEdit,
            attr="tool",
        )


class _defs_sculpt:

    @staticmethod
    def generate_from_brushes(context):
        return generate_from_enum_ex(
            context,
            icon_prefix="brush.sculpt.",
            type=bpy.types.Brush,
            attr="sculpt_tool",
        )

    @ToolDef.from_fn
    def hide_border():
        return dict(
            text="Box Hide",
            icon="ops.sculpt.border_hide",
            widget=None,
            keymap=(),
        )

    @ToolDef.from_fn
    def mask_border():
        return dict(
            text="Box Mask",
            icon="ops.sculpt.border_mask",
            widget=None,
            keymap=(),
        )


class _defs_vertex_paint:

    @staticmethod
    def poll_select_mask(context):
        ob = context.active_object
        return (ob.type == 'MESH' and
                (ob.data.use_paint_mask or
                 ob.data.use_paint_mask_vertex))

    @staticmethod
    def generate_from_brushes(context):
        return generate_from_enum_ex(
            context,
            icon_prefix="brush.paint_vertex.",
            type=bpy.types.Brush,
            attr="vertex_tool",
        )


class _defs_texture_paint:

    @staticmethod
    def generate_from_brushes(context):
        return generate_from_enum_ex(
            context,
            icon_prefix="brush.paint_texture.",
            type=bpy.types.Brush,
            attr="image_tool",
        )


class _defs_weight_paint:

    @staticmethod
    def poll_select_mask(context):
        ob = context.active_object
        return (ob.type == 'MESH' and
                (ob.data.use_paint_mask or
                 ob.data.use_paint_mask_vertex))

    @staticmethod
    def generate_from_brushes(context):
        return generate_from_enum_ex(
            context,
            icon_prefix="brush.paint_weight.",
            type=bpy.types.Brush,
            attr="weight_tool",
        )

    @ToolDef.from_fn
    def sample_weight():
        return dict(
            text="Sample Weight",
            icon="ops.paint.weight_sample",
            widget=None,
            keymap=(),
        )

    @ToolDef.from_fn
    def sample_weight_group():
        return dict(
            text="Sample Vertex Group",
            icon="ops.paint.weight_sample_group",
            widget=None,
            keymap=(),
        )

    @ToolDef.from_fn
    def gradient():
        def draw_settings(context, layout, tool):
            brush = context.tool_settings.weight_paint.brush
            if brush is not None:
                from .properties_paint_common import UnifiedPaintPanel
                UnifiedPaintPanel.prop_unified_weight(
                    layout, context, brush, "weight", slider=True, text="Weight")
                UnifiedPaintPanel.prop_unified_strength(
                    layout, context, brush, "strength", slider=True, text="Strength")
            props = tool.operator_properties("paint.weight_gradient")
            layout.prop(props, "type")

        return dict(
            text="Gradient",
            icon="ops.paint.weight_gradient",
            widget=None,
            keymap=(),
            draw_settings=draw_settings,
        )


class _defs_image_generic:

    @staticmethod
    def poll_uvedit(context):
        ob = context.edit_object
        if ob is not None:
            data = ob.data
            if data is not None:
                return bool(getattr(data, "uv_layers", False))
        return False

    @ToolDef.from_fn
    def cursor():
        return dict(
            text="Cursor",
            description=(
                "Set the cursor location, drag to transform"
            ),
            icon="ops.generic.cursor",
            keymap=(),
        )


class _defs_image_uv_transform:

    @ToolDef.from_fn
    def transform():
        return dict(
            text="Transform",
            description=(
                "Supports any combination of grab, rotate & scale at once"
            ),
            icon="ops.transform.transform",
            widget="IMAGE_GGT_gizmo2d",
            # No keymap default action, only for gizmo!
        )


class _defs_image_uv_select:

    @ToolDef.from_fn
    def select():
        def draw_settings(context, layout, tool):
            pass
        return dict(
            text="Select",
            icon="ops.generic.select",
            widget=None,
            keymap=(),
            draw_settings=draw_settings,
        )

    @ToolDef.from_fn
    def box():
        def draw_settings(context, layout, tool):
            props = tool.operator_properties("uv.select_box")
            layout.prop(props, "mode", expand=True)
        return dict(
            text="Select Box",
            icon="ops.generic.select_box",
            widget=None,
            keymap=(),
            draw_settings=draw_settings,
        )

    @ToolDef.from_fn
    def lasso():
        def draw_settings(context, layout, tool):
            props = tool.operator_properties("uv.select_lasso")
            layout.prop(props, "mode", expand=True)
        return dict(
            text="Select Lasso",
            icon="ops.generic.select_lasso",
            widget=None,
            keymap=(),
            draw_settings=draw_settings,
        )

    @ToolDef.from_fn
    def circle():
        def draw_settings(context, layout, tool):
            props = tool.operator_properties("uv.select_circle")
            layout.prop(props, "radius")
        return dict(
            text="Select Circle",
            icon="ops.generic.select_circle",
            widget=None,
            keymap=(),
            draw_settings=draw_settings,
        )


class _defs_image_uv_sculpt:

    @staticmethod
    def generate_from_brushes(context):
        return generate_from_enum_ex(
            context,
            icon_prefix="brush.uv_sculpt.",
            type=bpy.types.ToolSettings,
            attr="uv_sculpt_tool",
        )


class _defs_gpencil_paint:

    @staticmethod
    def generate_from_brushes(context):
        return generate_from_enum_ex(
            context,
            icon_prefix="brush.gpencil_draw.",
            type=bpy.types.Brush,
            attr="gpencil_tool",
            tooldef_keywords=dict(
                operator="gpencil.draw",
            ),
        )

    @ToolDef.from_fn
    def cutter():
        return dict(
            text="Cutter",
            icon="ops.gpencil.stroke_cutter",
            cursor='KNIFE',
            widget=None,
            keymap=(),
        )

    @ToolDef.from_fn
    def line():
        return dict(
            text="Line",
            icon="ops.gpencil.primitive_line",
            cursor='CROSSHAIR',
            widget=None,
            keymap=(),
        )

    @ToolDef.from_fn
    def box():
        return dict(
            text="Box",
            icon="ops.gpencil.primitive_box",
            cursor='CROSSHAIR',
            widget=None,
            keymap=(),
        )

    @ToolDef.from_fn
    def circle():
        return dict(
            text="Circle",
            icon="ops.gpencil.primitive_circle",
            cursor='CROSSHAIR',
            widget=None,
            keymap=(),
        )

    @ToolDef.from_fn
    def arc():
        return dict(
            text="Arc",
            icon="ops.gpencil.primitive_arc",
            cursor='CROSSHAIR',
            widget=None,
            keymap=(),
        )

    @ToolDef.from_fn
    def curve():
        return dict(
            text="Curve",
            icon="ops.gpencil.primitive_curve",
            cursor='CROSSHAIR',
            widget=None,
            keymap=(),
        )


class _defs_gpencil_edit:
    @ToolDef.from_fn
    def bend():
        return dict(
            text="Bend",
            icon="ops.gpencil.edit_bend",
            widget=None,
            keymap=(),
        )

    @ToolDef.from_fn
    def select():
        def draw_settings(context, layout, tool):
            layout.prop(context.tool_settings.gpencil_sculpt, "intersection_threshold")
        return dict(
            text="Select",
            icon="ops.generic.select",
            widget=None,
            keymap=(),
            draw_settings=draw_settings,
        )

    @ToolDef.from_fn
    def box_select():
        def draw_settings(context, layout, tool):
            props = tool.operator_properties("gpencil.select_box")
            layout.prop(props, "mode", expand=True)
            layout.prop(context.tool_settings.gpencil_sculpt, "intersection_threshold")
        return dict(
            text="Select Box",
            icon="ops.generic.select_box",
            widget=None,
            keymap=(),
            draw_settings=draw_settings,
        )

    @ToolDef.from_fn
    def lasso_select():
        def draw_settings(context, layout, tool):
            props = tool.operator_properties("gpencil.select_lasso")
            layout.prop(props, "mode", expand=True)
            layout.prop(context.tool_settings.gpencil_sculpt, "intersection_threshold")
        return dict(
            text="Select Lasso",
            icon="ops.generic.select_lasso",
            widget=None,
            keymap=(),
            draw_settings=draw_settings,
        )

    @ToolDef.from_fn
    def circle_select():
        def draw_settings(context, layout, tool):
            layout.prop(context.tool_settings.gpencil_sculpt, "intersection_threshold")
        return dict(
            text="Select Circle",
            icon="ops.generic.select_circle",
            widget=None,
            keymap=(),
            draw_settings=draw_settings,
        )

    @ToolDef.from_fn
    def shear():
        return dict(
            text="Shear",
            icon="ops.gpencil.edit_shear",
            widget=None,
            keymap=(),
        )

    @ToolDef.from_fn
    def tosphere():
        return dict(
            text="To Sphere",
            icon="ops.transform.tosphere",
            widget=None,
            keymap=(),
        )


class _defs_gpencil_sculpt:

    @staticmethod
    def generate_from_brushes(context):
        return generate_from_enum_ex(
            context,
            icon_prefix="ops.gpencil.sculpt_",
            type=bpy.types.GPencilSculptSettings,
            attr="sculpt_tool",
        )


class _defs_gpencil_weight:

    @staticmethod
    def generate_from_brushes(context):
        return generate_from_enum_ex(
            context,
            icon_prefix="ops.gpencil.sculpt_",
            type=bpy.types.GPencilSculptSettings,
            attr="weight_tool",
        )


class _defs_node_select:

    @ToolDef.from_fn
    def select():
        def draw_settings(context, layout, tool):
            pass
        return dict(
            text="Select",
            icon="ops.generic.select",
            widget=None,
            keymap="Node Tool: Select",
            draw_settings=draw_settings,
        )

    @ToolDef.from_fn
    def box():
        def draw_settings(context, layout, tool):
            props = tool.operator_properties("node.select_box")
            layout.prop(props, "deselect")
            pass
        return dict(
            text="Select Box",
            icon="ops.generic.select_box",
            widget=None,
            keymap="Node Tool: Select Box",
            draw_settings=draw_settings,
        )

    @ToolDef.from_fn
    def lasso():
        def draw_settings(context, layout, tool):
            props = tool.operator_properties("node.select_lasso")
            layout.prop(props, "deselect")
            pass
        return dict(
            text="Select Lasso",
            icon="ops.generic.select_lasso",
            widget=None,
            keymap="Node Tool: Select Lasso",
            draw_settings=draw_settings,
        )


class _defs_node_edit:

    @ToolDef.from_fn
    def links_cut():
        return dict(
            text="Links Cut",
            icon="ops.mesh.knife_tool",
            widget=None,
            keymap="Node Tool: Links Cut",
        )


class IMAGE_PT_tools_active(ToolSelectPanelHelper, Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'TOOLS'
    bl_label = "Tools"  # not visible
    bl_options = {'HIDE_HEADER'}

    # Satisfy the 'ToolSelectPanelHelper' API.
    keymap_prefix = "Image Editor Tool:"

    @classmethod
    def tools_from_context(cls, context, mode=None):
        if mode is None:
            if context.space_data is None:
                mode = 'VIEW'
            else:
                mode = context.space_data.mode
        for tools in (cls._tools[None], cls._tools.get(mode, ())):
            for item in tools:
                if not (type(item) is ToolDef) and callable(item):
                    yield from item(context)
                else:
                    yield item

    @classmethod
    def tools_all(cls):
        yield from cls._tools.items()

    # for reuse
    _tools_transform = (
        _defs_image_uv_transform.transform,
    )

    _tools_select = (
        (
            _defs_image_uv_select.select,
            _defs_image_uv_select.box,
            _defs_image_uv_select.circle,
            _defs_image_uv_select.lasso,
        ),
    )

    _tools_annotate = (
        (
            _defs_annotate.scribble,
            _defs_annotate.line,
            _defs_annotate.poly,
            _defs_annotate.eraser,
        ),
    )

    _tools = {
        None: [
            # for all modes
        ],
        'VIEW': [
        ],
        'UV': [
            *_tools_select,
            _defs_image_generic.cursor,
            None,
            *_tools_transform,
            None,
            *_tools_annotate,
            None,
            lambda context: (
                _defs_image_uv_sculpt.generate_from_brushes(context)
                if _defs_image_generic.poll_uvedit(context)
                else ()
            ),
        ],
        'MASK': [
            None,
        ],
        'PAINT': [
            _defs_texture_paint.generate_from_brushes,
        ],
    }


class NODE_PT_tools_active(ToolSelectPanelHelper, Panel):
    bl_space_type = 'NODE_EDITOR'
    bl_region_type = 'TOOLS'
    bl_label = "Tools"  # not visible
    bl_options = {'HIDE_HEADER'}

    # Satisfy the 'ToolSelectPanelHelper' API.
    keymap_prefix = "Node Editor Tool:"

    @classmethod
    def tools_from_context(cls, context, mode=None):
        if mode is None:
            if context.space_data is None:
                mode = None
            else:
                mode = context.space_data.tree_type
        for tools in (cls._tools[None], cls._tools.get(mode, ())):
            for item in tools:
                if not (type(item) is ToolDef) and callable(item):
                    yield from item(context)
                else:
                    yield item

    @classmethod
    def tools_all(cls):
        yield from cls._tools.items()

    _tools_select = (
        (
            _defs_node_select.select,
            _defs_node_select.box,
            _defs_node_select.lasso,
        ),
    )

    _tools_annotate = (
        (
            _defs_annotate.scribble,
            _defs_annotate.line,
            _defs_annotate.poly,
            _defs_annotate.eraser,
        ),
    )

    _tools = {
        None: [
            *_tools_select,
            None,
            *_tools_annotate,
            None,
            _defs_node_edit.links_cut,
        ],
    }


class VIEW3D_PT_tools_active(ToolSelectPanelHelper, Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'TOOLS'
    bl_label = "Tools"  # not visible
    bl_options = {'HIDE_HEADER'}

    # Satisfy the 'ToolSelectPanelHelper' API.
    keymap_prefix = "3D View Tool:"

    @classmethod
    def tools_from_context(cls, context, mode=None):
        if mode is None:
            mode = context.mode
        for tools in (cls._tools[None], cls._tools.get(mode, ())):
            for item in tools:
                if not (type(item) is ToolDef) and callable(item):
                    yield from item(context)
                else:
                    yield item

    @classmethod
    def tools_all(cls):
        yield from cls._tools.items()

    # for reuse
    _tools_transform = (
        _defs_transform.transform,
        _defs_transform.translate,
        _defs_transform.rotate,
        (
            _defs_transform.scale,
            _defs_transform.scale_cage,
        ),
    )

    _tools_select = (
        (
            _defs_view3d_select.select,
            _defs_view3d_select.box,
            _defs_view3d_select.circle,
            _defs_view3d_select.lasso,
        ),
    )

    _tools_annotate = (
        (
            _defs_annotate.scribble,
            _defs_annotate.line,
            _defs_annotate.poly,
            _defs_annotate.eraser,
        ),
        _defs_view3d_generic.ruler,
    )

    _tools_gpencil_select = (
        (
            _defs_gpencil_edit.select,
            _defs_gpencil_edit.box_select,
            _defs_gpencil_edit.circle_select,
            _defs_gpencil_edit.lasso_select,
        ),
    )

    _tools_default = (
        *_tools_select,
        _defs_view3d_generic.cursor,
        None,
        *_tools_transform,
        None,
        *_tools_annotate,
    )

    _tools = {
        None: [
            # Don't use this! because of paint modes.
            # _defs_view3d_generic.cursor,
            # End group.
        ],
        'OBJECT': [
            *_tools_default,
        ],
        'POSE': [
            *_tools_default,
            None,
            (
                _defs_pose.breakdown,
                _defs_pose.push,
                _defs_pose.relax,
            ),
        ],
        'EDIT_ARMATURE': [
            *_tools_default,
            None,
            _defs_edit_armature.roll,
            (
                _defs_edit_armature.bone_size,
                _defs_edit_armature.bone_envelope,
            ),
            None,
            (
                _defs_edit_armature.extrude,
                _defs_edit_armature.extrude_cursor,
            ),
        ],
        'EDIT_MESH': [
            *_tools_default,
            None,
            _defs_edit_mesh.cube_add,
            None,
            (
                _defs_edit_mesh.extrude,
                _defs_edit_mesh.extrude_normals,
                _defs_edit_mesh.extrude_individual,
                _defs_edit_mesh.extrude_cursor,
            ),
            _defs_edit_mesh.inset,
            _defs_edit_mesh.bevel,
            (
                _defs_edit_mesh.loopcut_slide,
                _defs_edit_mesh.offset_edge_loops_slide,
            ),
            (
                _defs_edit_mesh.knife,
                _defs_edit_mesh.bisect,
            ),
            _defs_edit_mesh.poly_build,
            (
                _defs_edit_mesh.spin,
                _defs_edit_mesh.spin_duplicate,
            ),
            (
                _defs_edit_mesh.vertex_smooth,
                _defs_edit_mesh.vertex_randomize,
            ),
            (
                _defs_edit_mesh.edge_slide,
                _defs_edit_mesh.vert_slide,
            ),
            (
                _defs_edit_mesh.shrink_fatten,
                _defs_edit_mesh.push_pull,
            ),
            (
                _defs_edit_mesh.shear,
                _defs_edit_mesh.tosphere,
            ),
            (
                _defs_edit_mesh.rip_region,
                _defs_edit_mesh.rip_edge,
            ),
        ],
        'EDIT_CURVE': [
            *_tools_default,
            None,
            _defs_edit_curve.tilt,
            _defs_edit_curve.draw,
            (
                _defs_edit_curve.extrude,
                _defs_edit_curve.extrude_cursor,
            ),
        ],
        'EDIT_SURFACE': [
            *_tools_default,
        ],
        'EDIT_METABALL': [
            *_tools_default,
        ],
        'EDIT_LATTICE': [
            *_tools_default,
        ],
        'EDIT_TEXT': [
            _defs_view3d_generic.cursor,
            None,
            *_tools_annotate,
        ],
        'PARTICLE': [
            _defs_view3d_generic.cursor,
            _defs_particle.generate_from_brushes,
        ],
        'SCULPT': [
            _defs_sculpt.generate_from_brushes,
            None,
            _defs_sculpt.hide_border,
            _defs_sculpt.mask_border,
        ],
        'PAINT_TEXTURE': [
            _defs_texture_paint.generate_from_brushes,
        ],
        'PAINT_VERTEX': [
            _defs_vertex_paint.generate_from_brushes,
            None,
            lambda context: (
                VIEW3D_PT_tools_active._tools_select
                if _defs_vertex_paint.poll_select_mask(context)
                else ()
            ),
        ],
        'PAINT_WEIGHT': [
            # TODO, check for mixed pose mode
            _defs_view3d_generic.cursor,
            _defs_weight_paint.generate_from_brushes,
            None,
            _defs_weight_paint.sample_weight,
            _defs_weight_paint.sample_weight_group,
            None,
            lambda context: (
                VIEW3D_PT_tools_active._tools_select
                if _defs_weight_paint.poll_select_mask(context)
                else ()
            ),
            None,
            _defs_weight_paint.gradient,
        ],
        'PAINT_GPENCIL': [
            _defs_view3d_generic.cursor,
            None,
            _defs_gpencil_paint.generate_from_brushes,
            _defs_gpencil_paint.cutter,
            None,
            _defs_gpencil_paint.line,
            _defs_gpencil_paint.arc,
            _defs_gpencil_paint.curve,
            _defs_gpencil_paint.box,
            _defs_gpencil_paint.circle,
        ],
        'EDIT_GPENCIL': [
            *_tools_gpencil_select,
            _defs_view3d_generic.cursor,
            None,
            *_tools_transform,
            None,
            _defs_gpencil_edit.bend,
            _defs_gpencil_edit.shear,
            _defs_gpencil_edit.tosphere,
        ],
        'SCULPT_GPENCIL': [
            *_tools_gpencil_select,
            None,
            _defs_gpencil_sculpt.generate_from_brushes,
        ],
        'WEIGHT_GPENCIL': [
            _defs_gpencil_weight.generate_from_brushes,
        ],
    }


classes = (
    IMAGE_PT_tools_active,
    NODE_PT_tools_active,
    VIEW3D_PT_tools_active,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
