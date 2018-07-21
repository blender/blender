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

from bpy.types import Panel

from .space_toolsystem_common import (
    ToolSelectPanelHelper,
    ToolDef,
)


def generate_from_brushes_ex(
        context, *,
        icon_prefix,
        brush_test_attr,
        brush_category_attr,
        brush_category_layout,
):
    # Categories
    brush_categories = {}
    for brush in context.blend_data.brushes:
        if getattr(brush, brush_test_attr):
            category = getattr(brush, brush_category_attr)
            name = brush.name
            brush_categories.setdefault(category, []).append(
                ToolDef.from_dict(
                    dict(
                        text=name,
                        icon=icon_prefix + category.lower(),
                        data_block=name,
                    )
                )
            )

    def tools_from_brush_group(groups):
        assert(type(groups) is tuple)
        if len(groups) == 1:
            tool_defs = tuple(brush_categories.pop(groups[0], ()))
        else:
            tool_defs = tuple(item for g in groups for item in brush_categories.pop(g, ()))
        if len(tool_defs) > 1:
            return (tool_defs,)
        else:
            return tool_defs

    # Each item below is a single toolbar entry:
    # Grouped for multiple or none if no brushes are found.
    tool_defs = tuple(
        tool_def
        for category in brush_category_layout
        for tool_def in tools_from_brush_group(category)
    )
    # Ensure we use all types.
    if brush_categories:
        print(brush_categories)
    assert(len(brush_categories) == 0)
    return tool_defs


class _defs_view3d_generic:
    @ToolDef.from_fn
    def cursor():
        def draw_settings(context, layout, tool):
            wm = context.window_manager
            props = tool.operator_properties("view3d.cursor3d")
            layout.prop(props, "use_depth")
            layout.prop(props, "orientation")

        return dict(
            text="Cursor",
            icon="ops.generic.cursor",
            keymap=(
                ("view3d.cursor3d", dict(), dict(type='ACTIONMOUSE', value='PRESS')),
                ("transform.translate",
                 dict(release_confirm=True, cursor_transform=True),
                 dict(type='EVT_TWEAK_A', value='ANY'),
                 ),
            ),
            draw_settings=draw_settings,
        )

    @ToolDef.from_fn
    def cursor_click():
        return dict(
            text="None",
            icon="ops.generic.cursor",
            keymap=(
                # This is a dummy keymap entry, until particle system is properly working with toolsystem.
                ("view3d.cursor3d", dict(), dict(type='ACTIONMOUSE', value='CLICK', ctrl=True, alt=True, shift=True)),
            ),
        )

    @ToolDef.from_fn
    def ruler():
        return dict(
            text="Ruler",
            icon="ops.view3d.ruler",
            widget="VIEW3D_GGT_ruler",
            keymap=(
                ("view3d.ruler_add", dict(), dict(type='EVT_TWEAK_A', value='ANY')),
            ),
        )


class _defs_transform:

    @ToolDef.from_fn
    def translate():
        return dict(
            text="Grab",
            # cursor='SCROLL_XY',
            icon="ops.transform.translate",
            widget="TRANSFORM_GGT_gizmo",
            operator="transform.translate",
            # TODO, implement as optional fallback gizmo
            # keymap=(
            #     ("transform.translate", dict(release_confirm=True), dict(type='EVT_TWEAK_A', value='ANY')),
            # ),
        )

    @ToolDef.from_fn
    def rotate():
        return dict(
            text="Rotate",
            # cursor='SCROLL_XY',
            icon="ops.transform.rotate",
            widget="TRANSFORM_GGT_gizmo",
            operator="transform.rotate",
            # TODO, implement as optional fallback gizmo
            # keymap=(
            #     ("transform.rotate", dict(release_confirm=True), dict(type='EVT_TWEAK_A', value='ANY')),
            # ),
        )

    @ToolDef.from_fn
    def scale():
        return dict(
            text="Scale",
            # cursor='SCROLL_XY',
            icon="ops.transform.resize",
            widget="TRANSFORM_GGT_gizmo",
            operator="transform.resize",
            # TODO, implement as optional fallback gizmo
            # keymap=(
            #     ("transform.resize", dict(release_confirm=True), dict(type='EVT_TWEAK_A', value='ANY')),
            # ),
        )

    @ToolDef.from_fn
    def scale_cage():
        return dict(
            text="Scale Cage",
            icon="ops.transform.resize.cage",
            widget="VIEW3D_GGT_xform_cage",
            operator="transform.resize",
        )

    @ToolDef.from_fn
    def transform():
        def draw_settings(context, layout, tool):
            tool_settings = context.tool_settings
            layout.prop(tool_settings, "use_gizmo")

        return dict(
            text="Transform",
            icon="ops.transform.transform",
            widget="TRANSFORM_GGT_gizmo",
            # No keymap default action, only for gizmo!
            draw_settings=draw_settings,
        )


class _defs_view3d_select:

    @ToolDef.from_fn
    def border():
        return dict(
            text="Select Border",
            icon="ops.generic.select_border",
            widget=None,
            keymap=(
                ("view3d.select_border",
                 dict(deselect=False),
                 dict(type='EVT_TWEAK_A', value='ANY')),
                ("view3d.select_border",
                 dict(deselect=True),
                 dict(type='EVT_TWEAK_A', value='ANY', ctrl=True)),
            ),
        )

    @ToolDef.from_fn
    def circle():
        return dict(
            text="Select Circle",
            icon="ops.generic.select_circle",
            widget=None,
            keymap=(
                ("view3d.select_circle",
                 dict(deselect=False),
                 dict(type='ACTIONMOUSE', value='PRESS')),
                ("view3d.select_circle",
                 dict(deselect=True),
                 dict(type='ACTIONMOUSE', value='PRESS', ctrl=True)),
            ),
        )

    @ToolDef.from_fn
    def lasso():
        return dict(
            text="Select Lasso",
            icon="ops.generic.select_lasso",
            widget=None,
            keymap=(
                ("view3d.select_lasso",
                 dict(deselect=False),
                 dict(type='EVT_TWEAK_A', value='ANY')),
                ("view3d.select_lasso",
                 dict(deselect=True),
                 dict(type='EVT_TWEAK_A', value='ANY', ctrl=True)),
            ),
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
            keymap=(
                ("transform.transform",
                 dict(release_confirm=True, mode='BONE_ROLL'),
                 dict(type='EVT_TWEAK_A', value='ANY'),),
            ),
        )

    @ToolDef.from_fn
    def bone_envelope():
        return dict(
            text="Bone Envelope",
            icon="ops.transform.bone_envelope",
            widget=None,
            keymap=(
                ("transform.transform",
                 dict(release_confirm=True, mode='BONE_ENVELOPE'),
                 dict(type='ACTIONMOUSE', value='PRESS')),
            ),
        )

    @ToolDef.from_fn
    def bone_size():
        return dict(
            text="Bone Size",
            icon="ops.transform.bone_size",
            widget=None,
            keymap=(
                ("transform.transform",
                 dict(release_confirm=True, mode='BONE_SIZE'),
                 dict(type='ACTIONMOUSE', value='PRESS')),
            ),
        )

    @ToolDef.from_fn
    def extrude():
        return dict(
            text="Extrude",
            icon="ops.armature.extrude_move",
            widget=None,
            keymap=(
                ("armature.click_extrude", dict(), dict(type='ACTIONMOUSE', value='PRESS')),
            ),
        )

    @ToolDef.from_fn
    def extrude_cursor():
        return dict(
            text="Extrude to Cursor",
            icon="ops.armature.extrude_cursor",
            widget=None,
            keymap=(
                ("armature.click_extrude", dict(), dict(type='ACTIONMOUSE', value='PRESS')),
            ),
        )


class _defs_edit_mesh:

    @ToolDef.from_fn
    def cube_add():
        return dict(
            text="Add Cube",
            icon="ops.mesh.primitive_cube_add_manipulator",
            widget=None,
            keymap=(
                ("view3d.cursor3d", dict(), dict(type='ACTIONMOUSE', value='CLICK')),
                ("mesh.primitive_cube_add_gizmo", dict(), dict(type='EVT_TWEAK_A', value='ANY')),
            ),
        )

    @ToolDef.from_fn
    def rip_region():
        def draw_settings(context, layout, tool):
            wm = context.window_manager
            props = tool.operator_properties("mesh.rip_move")
            props_macro = props.MESH_OT_rip
            layout.prop(props_macro, "use_fill")

        return dict(
            text="Rip Region",
            icon="ops.mesh.rip",
            widget=None,
            keymap=(
                ("mesh.rip_move",
                 dict(TRANSFORM_OT_translate=dict(release_confirm=True)),
                 dict(type='ACTIONMOUSE', value='PRESS')),
            ),
            draw_settings=draw_settings,
        )

    @ToolDef.from_fn
    def rip_edge():
        return dict(
            text="Rip Edge",
            icon="ops.mesh.rip_edge",
            widget=None,
            keymap=(
                ("mesh.rip_edge_edge_move", dict(),
                 dict(type='ACTIONMOUSE', value='PRESS')),
            ),
        )

    @ToolDef.from_fn
    def poly_build():
        return dict(
            text="Poly Build",
            icon="ops.mesh.polybuild_hover",
            widget=None,
            keymap=(
                ("mesh.polybuild_face_at_cursor_move",
                 dict(TRANSFORM_OT_translate=dict(release_confirm=True)),
                 dict(type='ACTIONMOUSE', value='PRESS')),
                ("mesh.polybuild_split_at_cursor_move",
                 dict(TRANSFORM_OT_translate=dict(release_confirm=True)),
                 dict(type='ACTIONMOUSE', value='PRESS', ctrl=True)),
                ("mesh.polybuild_dissolve_at_cursor", dict(), dict(type='ACTIONMOUSE', value='CLICK', alt=True)),
                ("mesh.polybuild_hover", dict(use_boundary=False), dict(type='MOUSEMOVE', value='ANY', alt=True)),
                ("mesh.polybuild_hover", dict(use_boundary=True), dict(type='MOUSEMOVE', value='ANY', any=True)),
            ),
        )

    @ToolDef.from_fn
    def edge_slide():
        return dict(
            text="Edge Slide",
            icon="ops.transform.edge_slide",
            widget=None,
            keymap=(
                ("transform.edge_slide", dict(release_confirm=True),
                 dict(type='ACTIONMOUSE', value='PRESS')
                 ),
            ),
        )

    @ToolDef.from_fn
    def vert_slide():
        return dict(
            text="Vertex Slide",
            icon="ops.transform.vert_slide",
            widget=None,
            keymap=(
                ("transform.vert_slide", dict(release_confirm=True),
                 dict(type='ACTIONMOUSE', value='PRESS')),
            ),
        )

    @ToolDef.from_fn
    def spin():
        return dict(
            text="Spin",
            icon="ops.mesh.spin",
            widget=None,
            keymap=(
                ("mesh.spin", dict(),
                 dict(type='ACTIONMOUSE', value='PRESS')),
            ),
        )

    @ToolDef.from_fn
    def spin_duplicate():
        return dict(
            text="Spin (Duplicate)",
            icon="ops.mesh.spin.duplicate",
            widget=None,
            keymap=(
                ("mesh.spin", dict(dupli=True),
                 dict(type='ACTIONMOUSE', value='PRESS')),
            ),
        )

    @ToolDef.from_fn
    def inset():
        def draw_settings(context, layout, tool):
            wm = context.window_manager
            props = tool.operator_properties("mesh.inset")
            layout.prop(props, "use_outset")
            layout.prop(props, "use_individual")
            layout.prop(props, "use_even_offset")
            layout.prop(props, "use_relative_offset")

        return dict(
            text="Inset Faces",
            icon="ops.mesh.inset",
            widget=None,
            keymap=(
                ("mesh.inset", dict(release_confirm=True),
                 dict(type='ACTIONMOUSE', value='PRESS')),
            ),
            draw_settings=draw_settings,
        )

    @ToolDef.from_fn
    def bevel():
        return dict(
            text="Bevel",
            icon="ops.mesh.bevel",
            widget=None,
            keymap=(
                ("mesh.bevel", dict(),
                 dict(type='ACTIONMOUSE', value='PRESS')),
            ),
        )

    @ToolDef.from_fn
    def extrude():
        return dict(
            text="Extrude Region",
            icon="ops.mesh.extrude_region_move",
            widget="MESH_GGT_extrude",
            operator="view3d.edit_mesh_extrude_move_normal",
            keymap=(
                ("mesh.extrude_context_move", dict(TRANSFORM_OT_translate=dict(release_confirm=True)),
                 dict(type='EVT_TWEAK_A', value='ANY')),
            ),
        )

    @ToolDef.from_fn
    def extrude_individual():
        return dict(
            text="Extrude Individual",
            icon="ops.mesh.extrude_faces_move",
            widget=None,
            keymap=(
                ("mesh.extrude_faces_move", dict(TRANSFORM_OT_shrink_fatten=dict(release_confirm=True)),
                 dict(type='EVT_TWEAK_A', value='ANY')),
            ),
        )

    @ToolDef.from_fn
    def extrude_cursor():
        return dict(
            text="Extrude to Cursor",
            icon="ops.mesh.dupli_extrude_cursor",
            widget=None,
            keymap=(
                ("mesh.dupli_extrude_cursor", dict(), dict(type='ACTIONMOUSE', value='PRESS')),
            ),
        )

    @ToolDef.from_fn
    def loopcut_slide():
        return dict(
            text="Loop Cut",
            icon="ops.mesh.loopcut_slide",
            widget=None,
            keymap=(
                ("mesh.loopcut_slide", dict(), dict(type='ACTIONMOUSE', value='PRESS')),
            ),
        )

    @ToolDef.from_fn
    def offset_edge_loops_slide():
        return dict(
            text="Offset Edge Loop Cut",
            icon="ops.mesh.offset_edge_loops_slide",
            widget=None,
            keymap=(
                ("mesh.offset_edge_loops_slide", dict(), dict(type='ACTIONMOUSE', value='PRESS')),
            ),
        )

    @ToolDef.from_fn
    def vertex_smooth():
        return dict(
            text="Smooth",
            icon="ops.mesh.vertices_smooth",
            widget=None,
            keymap=(
                ("mesh.vertices_smooth", dict(),
                 dict(type='ACTIONMOUSE', value='PRESS')),
            ),
        )

    @ToolDef.from_fn
    def vertex_randomize():
        return dict(
            text="Randomize",
            icon="ops.transform.vertex_random",
            widget=None,
            keymap=(
                ("transform.vertex_random", dict(),
                 dict(type='ACTIONMOUSE', value='PRESS')),
            ),
        )

    @ToolDef.from_fn
    def shrink_fatten():
        def draw_settings(context, layout, tool):
            wm = context.window_manager
            props = tool.operator_properties("transform.shrink_fatten")
            layout.prop(props, "use_even_offset")

        return dict(
            text="Shrink/Fatten",
            icon="ops.transform.shrink_fatten",
            widget=None,
            keymap=(
                ("transform.shrink_fatten", dict(release_confirm=True),
                 dict(type='ACTIONMOUSE', value='PRESS')),
            ),
            draw_settings=draw_settings,
        )

    @ToolDef.from_fn
    def push_pull():
        return dict(
            text="Push/Pull",
            icon="ops.transform.push_pull",
            widget=None,
            keymap=(
                ("transform.push_pull", dict(release_confirm=True),
                 dict(type='ACTIONMOUSE', value='PRESS')),
            ),
        )

    @ToolDef.from_fn
    def knife():
        def draw_settings(context, layout, tool):
            wm = context.window_manager
            props = tool.operator_properties("mesh.knife_tool")
            layout.prop(props, "use_occlude_geometry")
            layout.prop(props, "only_selected")

        return dict(
            text="Knife",
            icon="ops.mesh.knife_tool",
            widget=None,
            keymap=(
                ("mesh.knife_tool",
                 dict(wait_for_input=False),
                 dict(type='ACTIONMOUSE', value='PRESS')),
            ),
            draw_settings=draw_settings,
        )

    @ToolDef.from_fn
    def bisect():
        return dict(
            text="Bisect",
            icon="ops.mesh.bisect",
            widget=None,
            keymap=(
                ("mesh.bisect",
                 dict(),
                 dict(type='EVT_TWEAK_A', value='ANY')),
            ),
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
            icon=None,
            widget=None,
            keymap=(
                ("curve.draw", dict(wait_for_input=False), dict(type='ACTIONMOUSE', value='PRESS')),
            ),
            draw_settings=draw_settings,
        )

    @ToolDef.from_fn
    def extrude_cursor():
        return dict(
            text="Extrude Cursor",
            icon=None,
            widget=None,
            keymap=(
                ("curve.vertex_add", dict(), dict(type='ACTIONMOUSE', value='PRESS')),
            ),
        )


class _defs_pose:

    @ToolDef.from_fn
    def breakdown():
        return dict(
            text="Breakdowner",
            icon="ops.pose.breakdowner",
            widget=None,
            keymap=(
                ("pose.breakdown", dict(), dict(type='ACTIONMOUSE', value='PRESS')),
            ),
        )

    @ToolDef.from_fn
    def push():
        return dict(
            text="Push",
            icon="ops.pose.push",
            widget=None,
            keymap=(
                ("pose.push", dict(), dict(type='ACTIONMOUSE', value='PRESS')),
            ),
        )

    @ToolDef.from_fn
    def relax():
        return dict(
            text="Relax",
            icon="ops.pose.relax",
            widget=None,
            keymap=(
                ("pose.relax", dict(), dict(type='ACTIONMOUSE', value='PRESS')),
            ),
        )


class _defs_sculpt:

    @staticmethod
    def generate_from_brushes(context):
        return generate_from_brushes_ex(
            context,
            icon_prefix="brush.sculpt.",
            brush_test_attr="use_paint_sculpt",
            brush_category_attr="sculpt_tool",
            brush_category_layout=(
                ('DRAW',),
                ('GRAB', 'THUMB'),
                ('SNAKE_HOOK',),
                ('BLOB', 'INFLATE'),
                ('SMOOTH', 'SCRAPE', 'FLATTEN'),
                ('CREASE', 'PINCH'),
                ('CLAY', 'CLAY_STRIPS'),
                ('LAYER',),
                ('NUDGE', 'ROTATE'),
                ('FILL',),
                ('SIMPLIFY',),
                ('MASK',),
            )
        )


class _defs_vertex_paint:

    @staticmethod
    def generate_from_brushes(context):
        return generate_from_brushes_ex(
            context,
            icon_prefix="brush.paint_vertex.",
            brush_test_attr="use_paint_vertex",
            brush_category_attr="vertex_tool",
            brush_category_layout=(
                ('MIX',),
                ('BLUR', 'AVERAGE'),
                ('SMEAR',),
                (
                    'ADD', 'SUB', 'MUL', 'LIGHTEN', 'DARKEN',
                    'COLORDODGE', 'DIFFERENCE', 'SCREEN', 'HARDLIGHT',
                    'OVERLAY', 'SOFTLIGHT', 'EXCLUSION', 'LUMINOCITY',
                    'SATURATION', 'HUE', 'ERASE_ALPHA', 'ADD_ALPHA',
                ),
            )
        )


class _defs_texture_paint:

    @staticmethod
    def generate_from_brushes(context):
        return generate_from_brushes_ex(
            context,
            icon_prefix="brush.paint_texture.",
            brush_test_attr="use_paint_image",
            brush_category_attr="image_tool",
            brush_category_layout=(
                ('DRAW',),
                ('SOFTEN',),
                ('SMEAR',),
                ('CLONE',),
                ('FILL',),
                ('MASK',),
            )
        )


class _defs_weight_paint:

    @staticmethod
    def generate_from_brushes(context):
        return generate_from_brushes_ex(
            context,
            icon_prefix="brush.paint_weight.",
            brush_test_attr="use_paint_weight",
            brush_category_attr="vertex_tool",
            brush_category_layout=(
                ('MIX',),
                ('BLUR', 'AVERAGE'),
                ('SMEAR',),
                (
                    'ADD', 'SUB', 'MUL', 'LIGHTEN', 'DARKEN',
                    'COLORDODGE', 'DIFFERENCE', 'SCREEN', 'HARDLIGHT',
                    'OVERLAY', 'SOFTLIGHT', 'EXCLUSION', 'LUMINOCITY',
                    'SATURATION', 'HUE',
                ),
            )
        )

    @ToolDef.from_fn
    def sample_weight():
        return dict(
            text="Sample Weight",
            icon="ops.paint.weight_sample",
            widget=None,
            keymap=(
                ("paint.weight_sample", dict(), dict(type='ACTIONMOUSE', value='PRESS')),
            ),
        )

    @ToolDef.from_fn
    def sample_weight_group():
        return dict(
            text="Sample Vertex Group",
            icon="ops.paint.weight_sample_group",
            widget=None,
            keymap=(
                ("paint.weight_sample_group", dict(), dict(type='ACTIONMOUSE', value='PRESS')),
            ),
        )

    @ToolDef.from_fn
    def gradient():
        def draw_settings(context, layout, tool):
            wm = context.window_manager
            props = tool.operator_properties("paint.weight_gradient")
            layout.prop(props, "type")

        return dict(
            text="Gradient",
            icon="ops.paint.weight_gradient",
            widget=None,
            keymap=(
                ("paint.weight_gradient", dict(), dict(type='EVT_TWEAK_A', value='ANY')),
            ),
            draw_settings=draw_settings,
        )


class _defs_uv_select:

    @ToolDef.from_fn
    def border():
        return dict(
            text="Select Border",
            icon="ops.generic.select_border",
            widget=None,
            keymap=(
                ("uv.select_border",
                 dict(deselect=False),
                 dict(type='EVT_TWEAK_A', value='ANY')),
                # ("uv.select_border",
                #  dict(deselect=True),
                #  dict(type='EVT_TWEAK_A', value='ANY', ctrl=True)),
            ),
        )

    @ToolDef.from_fn
    def circle():
        return dict(
            text="Select Circle",
            icon="ops.generic.select_circle",
            widget=None,
            keymap=(
                ("uv.select_circle",
                 dict(),  # dict(deselect=False),
                 dict(type='ACTIONMOUSE', value='PRESS')),
                # ("uv.select_circle",
                #  dict(deselect=True),
                #  dict(type='ACTIONMOUSE', value='PRESS', ctrl=True)),
            ),
        )

    @ToolDef.from_fn
    def lasso():
        return dict(
            text="Select Lasso",
            icon="ops.generic.select_lasso",
            widget=None,
            keymap=(
                ("uv.select_lasso",
                 dict(deselect=False),
                 dict(type='EVT_TWEAK_A', value='ANY')),
                # ("uv.select_lasso",
                #  dict(deselect=True),
                #  dict(type='EVT_TWEAK_A', value='ANY', ctrl=True)),
            ),
        )


class IMAGE_PT_tools_active(ToolSelectPanelHelper, Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'TOOLS'
    bl_category = "Tools"
    bl_label = "Tools"  # not visible
    bl_options = {'HIDE_HEADER'}

    # Satisfy the 'ToolSelectPanelHelper' API.
    keymap_prefix = "Image Editor Tool:"

    @classmethod
    def tools_from_context(cls, context, mode=None):
        if mode is None:
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
    _tools_select = (
        (
            _defs_uv_select.border,
            _defs_uv_select.circle,
            _defs_uv_select.lasso,
        ),
    )

    _tools = {
        None: [
            # for all modes
        ],
        'VIEW': [
            *_tools_select,

        ],
        'MASK': [
            None,
        ],
        'PAINT': [
            _defs_texture_paint.generate_from_brushes,
        ],
    }


class VIEW3D_PT_tools_active(ToolSelectPanelHelper, Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'TOOLS'
    bl_category = "Tools"
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
        None,
        _defs_view3d_generic.ruler,
    )

    _tools_select = (
        (
            _defs_view3d_select.border,
            _defs_view3d_select.circle,
            _defs_view3d_select.lasso,
        ),
    )

    _tools = {
        None: [
            _defs_view3d_generic.cursor,
            # End group.
        ],
        'OBJECT': [
            *_tools_select,
            None,
            *_tools_transform,
        ],
        'POSE': [
            *_tools_select,
            *_tools_transform,
            None,
            (
                _defs_pose.breakdown,
                _defs_pose.push,
                _defs_pose.relax,
            )
        ],
        'EDIT_ARMATURE': [
            *_tools_select,
            None,
            *_tools_transform,
            _defs_edit_armature.roll,
            (
                _defs_edit_armature.bone_size,
                _defs_edit_armature.bone_envelope,
            ),
            None,
            (
                _defs_edit_armature.extrude,
                _defs_edit_armature.extrude_cursor,
            )
        ],
        'EDIT_MESH': [
            *_tools_select,
            None,
            *_tools_transform,
            None,
            _defs_edit_mesh.cube_add,
            None,
            (
                _defs_edit_mesh.extrude,
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
                _defs_edit_mesh.rip_region,
                _defs_edit_mesh.rip_edge,
            ),
        ],
        'EDIT_CURVE': [
            *_tools_select,
            None,
            *_tools_transform,
            None,
            _defs_edit_curve.draw,
            _defs_edit_curve.extrude_cursor,
        ],
        'PARTICLE': [
            # TODO(campbell): use cursor click tool to allow paint tools to run,
            # we need to integrate particle system tools properly.
            _defs_view3d_generic.cursor_click,
        ],
        'SCULPT': [
            _defs_sculpt.generate_from_brushes,
        ],
        'PAINT_TEXTURE': [
            _defs_texture_paint.generate_from_brushes,
        ],
        'PAINT_VERTEX': [
            _defs_vertex_paint.generate_from_brushes,
        ],
        'PAINT_WEIGHT': [
            _defs_weight_paint.generate_from_brushes,
            None,
            _defs_weight_paint.sample_weight,
            _defs_weight_paint.sample_weight_group,
            None,
            # TODO, override brush events
            *_tools_select,
            None,
            _defs_weight_paint.gradient,
        ],
    }


classes = (
    IMAGE_PT_tools_active,
    VIEW3D_PT_tools_active,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
