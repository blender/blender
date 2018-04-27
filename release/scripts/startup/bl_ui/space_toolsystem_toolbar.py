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


class _defs_view3d_generic:

    class cursor(ToolDef):
        text = "Cursor"
        icon = "ops.generic.cursor"
        widget = None

        keymap = (
            ("view3d.cursor3d", dict(), dict(type='ACTIONMOUSE', value='CLICK')),
        )

    class ruler(ToolDef):
        text = "Ruler/Protractor"
        icon = "ops.view3d.ruler"
        widget = "VIEW3D_WGT_ruler"
        keymap = (
            ("view3d.ruler_add", dict(), dict(type='EVT_TWEAK_A', value='ANY')),
        )


class _defs_transform:

    class translate(ToolDef):
        text = "Move"
        icon = "ops.transform.translate"
        widget = "TRANSFORM_WGT_manipulator"
        keymap = (
            ("transform.translate", dict(release_confirm=True), dict(type='EVT_TWEAK_A', value='ANY')),
        )

    class rotate(ToolDef):
        text = "Rotate"
        icon = "ops.transform.rotate"
        widget = "TRANSFORM_WGT_manipulator"
        keymap = (
            ("transform.rotate", dict(release_confirm=True), dict(type='EVT_TWEAK_A', value='ANY')),
        )

    class scale(ToolDef):
        text = "Scale"
        icon = "ops.transform.resize"
        widget = "TRANSFORM_WGT_manipulator"
        keymap = (
            ("transform.resize", dict(release_confirm=True), dict(type='EVT_TWEAK_A', value='ANY')),
        )

    class scale_cage(ToolDef):
        text = "Scale Cage"
        icon = "ops.transform.resize.cage"
        widget = "VIEW3D_WGT_xform_cage"
        keymap = None

    class transform(ToolDef):
        text = "Transform"
        icon = "ops.transform.transform"
        widget = "TRANSFORM_WGT_manipulator"
        # No favorites, only for manipulators!
        keymap = ()


class _defs_view3d_select:

    class border(ToolDef):
        text = "Select Border"
        icon = "ops.generic.select_border"
        widget = None
        keymap = (
            ("view3d.select_border",
             dict(deselect=False),
             dict(type='EVT_TWEAK_A', value='ANY')),
            ("view3d.select_border",
             dict(deselect=True),
             dict(type='EVT_TWEAK_A', value='ANY', ctrl=True)),
        )

    class circle(ToolDef):
        text = "Select Circle"
        icon = "ops.generic.select_circle"
        widget = None
        keymap = (
            ("view3d.select_circle",
             dict(deselect=False),
             dict(type='ACTIONMOUSE', value='PRESS')),
            ("view3d.select_circle",
             dict(deselect=True),
             dict(type='ACTIONMOUSE', value='PRESS', ctrl=True)),
        )

    class lasso(ToolDef):
        text = "Select Lasso"
        icon = "ops.generic.select_lasso"
        widget = None
        keymap = (
            ("view3d.select_lasso",
             dict(deselect=False),
             dict(type='EVT_TWEAK_A', value='ANY')),
            ("view3d.select_lasso",
             dict(deselect=True),
             dict(type='EVT_TWEAK_A', value='ANY', ctrl=True)),
        )

# -----------------------------------------------------------------------------
# Object Modes (named based on context.mode)

class _defs_weight_paint:

    class gradient_linear(ToolDef):
        text = "Linear Gradient"
        icon = None
        widget = None
        keymap = (
            ("paint.weight_gradient", dict(type='LINEAR'),
             dict(type='EVT_TWEAK_A', value='ANY')),
        )

    class gradient_radial(ToolDef):
        text = "Radial Gradient"
        icon = None
        widget = None
        keymap = (
            ("paint.weight_gradient",
             dict(type='RADIAL'),
             dict(type='EVT_TWEAK_A', value='ANY')),
        )


class _defs_edit_armature:

    class roll(ToolDef):
        text = "Roll"
        icon = "ops.armature.bone.roll",
        widget = None
        keymap = (
            ("transform.transform",
             dict(release_confirm=True, mode='BONE_ROLL'),
             dict(type='EVT_TWEAK_A', value='ANY'),),
        )

    class extrude(ToolDef):
        text = "Extrude",
        icon = "ops.armature.extrude_move",
        widget = None,
        keymap = (
            ("armature.click_extrude", dict(), dict(type='ACTIONMOUSE', value='PRESS')),
        )

    class extrude_cursor(ToolDef):
        text = "Extrude to Cursor",
        icon = "ops.armature.extrude_cursor",
        widget = None,
        keymap = (
            ("armature.click_extrude", dict(), dict(type='ACTIONMOUSE', value='PRESS')),
        )

class _defs_edit_mesh:

    class rip_region(ToolDef):
        text = "Rip Region"
        icon = "ops.mesh.rip"
        widget = None
        keymap = (
            ("mesh.rip_move", dict(),
             dict(type='ACTIONMOUSE', value='PRESS')),
        )

    class rip_edge(ToolDef):
        text = "Rip Edge"
        icon = "ops.mesh.rip_edge"
        widget = None
        keymap = (
            ("mesh.rip_edge_edge_move", dict(),
             dict(type='ACTIONMOUSE', value='PRESS')),
        )

    class poly_build(ToolDef):
        text = "Poly Build"
        icon = "ops.mesh.polybuild_hover"
        widget = None
        keymap = (
            ("mesh.polybuild_face_at_cursor_move",
             dict(TRANSFORM_OT_translate=dict(release_confirm=True)),
             dict(type='ACTIONMOUSE', value='PRESS')),
            ("mesh.polybuild_split_at_cursor_move",
             dict(TRANSFORM_OT_translate=dict(release_confirm=True)),
             dict(type='ACTIONMOUSE', value='PRESS', ctrl=True)),
            ("mesh.polybuild_dissolve_at_cursor", dict(), dict(type='ACTIONMOUSE', value='CLICK', alt=True)),
            ("mesh.polybuild_hover", dict(use_boundary=False), dict(type='MOUSEMOVE', value='ANY', alt=True)),
            ("mesh.polybuild_hover", dict(use_boundary=True), dict(type='MOUSEMOVE', value='ANY', any=True)),
        )

    class edge_slide(ToolDef):
        text = "Edge Slide"
        icon = "ops.transform.edge_slide"
        widget = None
        keymap = (
            ("transform.edge_slide", dict(release_confirm=True),
             dict(type='ACTIONMOUSE', value='PRESS')
             ),
        )

    class vert_slide(ToolDef):
        text = "Vertex Slide"
        icon = "ops.transform.vert_slide"
        widget = None
        keymap = (
            ("transform.vert_slide", dict(release_confirm=True),
             dict(type='ACTIONMOUSE', value='PRESS')),
        )

    class spin(ToolDef):
        text = "Spin"
        icon = "ops.mesh.spin"
        widget = None
        keymap = (
            ("mesh.spin", dict(),
             dict(type='ACTIONMOUSE', value='PRESS')),
        )

    class spin_duplicate(ToolDef):
        text = "Spin (Duplicate)"
        icon = "ops.mesh.spin.duplicate"
        widget = None
        keymap = (
            ("mesh.spin", dict(dupli=True),
             dict(type='ACTIONMOUSE', value='PRESS')),
        )

    class inset(ToolDef):
        text = "Inset Faces"
        icon = "ops.mesh.inset"
        widget = None
        keymap = (
            ("mesh.inset", dict(release_confirm=True),
             dict(type='ACTIONMOUSE', value='PRESS')),
        )

    class bevel(ToolDef):
        text = "Bevel"
        icon = "ops.mesh.bevel"
        widget = None
        keymap = (
            ("mesh.bevel", dict(),
             dict(type='ACTIONMOUSE', value='PRESS')),
        )

    class extrude(ToolDef):
        text = "Extrude Region"
        icon = "ops.mesh.extrude_region_move"
        widget = None
        keymap = (
            ("mesh.extrude_region_move", dict(TRANSFORM_OT_translate=dict(release_confirm=True)),
             dict(type='ACTIONMOUSE', value='PRESS')),
        )

    class extrude_individual(ToolDef):
        text = "Extrude Individual"
        icon = "ops.mesh.extrude_faces_move"
        widget = None
        keymap = (
            ("mesh.extrude_faces_move", dict(TRANSFORM_OT_shrink_fatten=dict(release_confirm=True)),
             dict(type='ACTIONMOUSE', value='PRESS')),
        )

    class extrude_cursor(ToolDef):
        text = "Extrude to Cursor"
        icon = "ops.mesh.dupli_extrude_cursor"
        widget = None
        keymap = (
            ("mesh.dupli_extrude_cursor", dict(), dict(type='ACTIONMOUSE', value='PRESS')),
        )

    class loopcut_slide(ToolDef):
        text = "Loop Cut"
        icon = "ops.mesh.loopcut_slide"
        widget = None
        keymap = (
            ("mesh.loopcut_slide", dict(), dict(type='ACTIONMOUSE', value='PRESS')),
        )

    class offset_edge_loops_slide(ToolDef):
        text = "Offset Edge Loop Cut"
        icon = "ops.mesh.offset_edge_loops_slide"
        widget = None
        keymap = (
            ("mesh.offset_edge_loops_slide", dict(), dict(type='ACTIONMOUSE', value='PRESS')),
        )

    class vertex_smooth(ToolDef):
        text = "Smooth"
        icon = "ops.mesh.vertices_smooth"
        widget = None
        keymap = (
            ("mesh.vertices_smooth", dict(),
             dict(type='ACTIONMOUSE', value='PRESS')),
        )

    class vertex_randomize(ToolDef):
        text = "Randomize"
        icon = "ops.transform.vertex_random"
        widget = None
        keymap = (
            ("transform.vertex_random", dict(),
             dict(type='ACTIONMOUSE', value='PRESS')),
        )

    class shrink_fatten(ToolDef):
        text = "Shrink/Fatten"
        icon = "ops.transform.shrink_fatten"
        widget = None
        keymap = (
            ("transform.shrink_fatten", dict(release_confirm=True),
             dict(type='ACTIONMOUSE', value='PRESS')),
        )

    class push_pull(ToolDef):
        text = "Push/Pull"
        icon = "ops.transform.push_pull"
        widget = None
        keymap = (
            ("transform.push_pull", dict(release_confirm=True),
             dict(type='ACTIONMOUSE', value='PRESS')),
        )

    class knife(ToolDef):
        text = "Knife"
        icon = "ops.mesh.knife_tool"
        widget = None
        keymap = (
            ("mesh.knife_tool",
             dict(wait_for_input=False),
             dict(type='ACTIONMOUSE', value='PRESS')),
        )

        @classmethod
        def draw_settings(cls, context, layout):
            wm = context.window_manager
            props = wm.operator_properties_last("mesh.knife_tool")
            layout.prop(props, "use_occlude_geometry")
            layout.prop(props, "only_selected")

    class bisect(ToolDef):
        text = "Bisect"
        icon = "ops.mesh.bisect"
        widget = None
        keymap = (
            ("mesh.bisect",
             dict(),
             dict(type='EVT_TWEAK_A', value='ANY')),
        )


class _defs_edit_curve:

    class draw(ToolDef):
        text = "Draw"
        icon = None
        widget = None
        keymap = (
            ("curve.draw", dict(wait_for_input=False), dict(type='ACTIONMOUSE', value='PRESS')),
        )

    class extrude_cursor(ToolDef):
        text = "Extrude Cursor"
        icon = None
        widget = None
        keymap = (
            ("curve.vertex_add", dict(), dict(type='ACTIONMOUSE', value='PRESS')),
        )


class VIEW3D_PT_tools_active(ToolSelectPanelHelper, Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'TOOLS'
    bl_category = "Tools"
    bl_label = "Tools"  # not visible
    bl_options = {'HIDE_HEADER'}

    # Satisfy the 'ToolSelectPanelHelper' API.
    keymap_prefix = "3D View Tool: "

    @classmethod
    def tools_from_context(cls, context):
        return (cls._tools[None], cls._tools.get(context.mode, ()))

    @classmethod
    def tools_all(cls):
        yield from cls._tools.items()

    # for reuse
    _tools_transform = (
        _defs_transform.translate,
        _defs_transform.rotate,
        (
            _defs_transform.scale,
            _defs_transform.scale_cage,
        ),
        _defs_transform.transform,
        None,
        _defs_view3d_generic.ruler,
    )

    _tools = {
        None: [
            _defs_view3d_generic.cursor,

            # 'Select' Group
            (
                _defs_view3d_select.border,
                _defs_view3d_select.circle,
                _defs_view3d_select.lasso,
            ),
            # End group.
        ],
        'OBJECT': [
            *_tools_transform,
        ],
        'POSE': [
            *_tools_transform,
        ],
        'PAINT_WEIGHT': [
            # TODO, override brush events
            (
                _defs_weight_paint.gradient_linear,
                _defs_weight_paint.gradient_radial,
            ),
        ],
        'EDIT_ARMATURE': [
            *_tools_transform,
            _defs_edit_armature.roll,
            None,
            (
                _defs_edit_armature.extrude,
                _defs_edit_armature.extrude_cursor,
            )
        ],
        'EDIT_MESH': [
            *_tools_transform,
            None,
            (
                _defs_edit_mesh.rip_region,
                _defs_edit_mesh.rip_edge,
            ),
            _defs_edit_mesh.poly_build,

            # 'Slide' Group
            (
                _defs_edit_mesh.edge_slide,
                _defs_edit_mesh.vert_slide,
            ),
            # End group.

            (
                _defs_edit_mesh.spin,
                _defs_edit_mesh.spin_duplicate,
            ),

            _defs_edit_mesh.inset,
            _defs_edit_mesh.bevel,
            (
                _defs_edit_mesh.loopcut_slide,
                _defs_edit_mesh.offset_edge_loops_slide,
            ),
            (
                _defs_edit_mesh.extrude,
                _defs_edit_mesh.extrude_individual,
                _defs_edit_mesh.extrude_cursor,
            ),

            (
                _defs_edit_mesh.vertex_smooth,
                _defs_edit_mesh.vertex_randomize,
            ),

            (
                _defs_edit_mesh.shrink_fatten,
                _defs_edit_mesh.push_pull,
            ),

            (
                _defs_edit_mesh.knife,
                _defs_edit_mesh.bisect,
            ),
        ],
        'EDIT_CURVE': [
            *_tools_transform,
            None,
            _defs_edit_curve.draw,
            _defs_edit_curve.extrude_cursor,
        ],
    }


classes = (
    VIEW3D_PT_tools_active,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
