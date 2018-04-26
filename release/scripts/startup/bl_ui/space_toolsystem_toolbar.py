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

    # Internal Data

    # for reuse
    _tools_transform = (
        dict(
            text="Translate",
            icon="ops.transform.translate",
            widget="TRANSFORM_WGT_manipulator",
            keymap=(
                ("transform.translate", dict(release_confirm=True), dict(type='EVT_TWEAK_A', value='ANY')),
            ),
        ),
        dict(
            text="Rotate",
            icon="ops.transform.rotate",
            widget="TRANSFORM_WGT_manipulator",
            keymap=(
                ("transform.rotate", dict(release_confirm=True), dict(type='EVT_TWEAK_A', value='ANY')),
            ),
        ),
        (
            dict(
                text="Scale",
                icon="ops.transform.resize",
                widget="TRANSFORM_WGT_manipulator",
                keymap=(
                    ("transform.resize", dict(release_confirm=True), dict(type='EVT_TWEAK_A', value='ANY')),
                ),
            ),
            dict(
                text="Scale Cage",
                icon="ops.transform.resize.cage",
                widget="VIEW3D_WGT_xform_cage",
                keymap=None,
            ),
        ),
        None,
        dict(
            text="Ruler/Protractor",
            icon="ops.view3d.ruler",
            widget="VIEW3D_WGT_ruler",
            keymap=(
                ("view3d.ruler_add", dict(), dict(type='EVT_TWEAK_A', value='ANY')),
            ),
        ),

        # DEBUGGING ONLY
        # ("Pixel Test", "tool_icon.pixeltest", None, (("wm.splash", dict(), dict(type='ACTIONMOUSE', value='PRESS')),)),
    )

    _tools = {
        None: [
            dict(
                text="Cursor",
                icon="ops.generic.cursor",
                widget=None,
                keymap=(
                    ("view3d.cursor3d", dict(), dict(type='ACTIONMOUSE', value='CLICK')),
                ),
            ),

            # 'Select' Group
            (
                dict(
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
                ),
                dict(
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
                ),
                dict(
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
                ),
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
                dict(
                    text="Linear Gradient",
                    icon=None,
                    widget=None,
                    keymap=(
                        ("paint.weight_gradient", dict(type='LINEAR'),
                         dict(type='EVT_TWEAK_A', value='ANY')),
                    ),
                ),
                dict(
                    text="Radial Gradient",
                    icon=None,
                    widget=None,
                    keymap=(
                        ("paint.weight_gradient",
                         dict(type='RADIAL'),
                         dict(type='EVT_TWEAK_A', value='ANY')),
                    ),
                ),
            ),
        ],
        'EDIT_ARMATURE': [
            *_tools_transform,
            dict(
                text="Roll",
                icon=None,
                widget=None,
                keymap=(
                    ("transform.transform",
                     dict(release_confirm=True, mode='BONE_ROLL'),
                     dict(type='EVT_TWEAK_A', value='ANY'),),
                ),
            ),
            None,
            dict(
                text="Extrude Cursor",
                icon="ops.armature.extrude",
                widget=None,
                keymap=(
                    ("armature.click_extrude", dict(), dict(type='ACTIONMOUSE', value='PRESS')),
                ),
            ),
        ],
        'EDIT_MESH': [
            *_tools_transform,
            None,
            (
                dict(
                    text="Rip Region",
                    icon="ops.mesh.rip",
                    widget=None,
                    keymap=(
                        ("mesh.rip_move", dict(),
                         dict(type='ACTIONMOUSE', value='PRESS')),
                    ),
                ),
                dict(
                    text="Rip Edge",
                    icon="ops.mesh.rip_edge",
                    widget=None,
                    keymap=(
                        ("mesh.rip_edge_edge_move", dict(),
                         dict(type='ACTIONMOUSE', value='PRESS')),
                    ),
                ),
            ),

            dict(
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
            ),


            # 'Slide' Group
            (
                dict(
                    text="Edge Slide",
                    icon="ops.transform.edge_slide",
                    widget=None,
                    keymap=(
                        ("transform.edge_slide", dict(release_confirm=True),
                         dict(type='ACTIONMOUSE', value='PRESS')),
                    ),
                ),
                dict(
                    text="Vertex Slide",
                    icon="ops.transform.vert_slide",
                    widget=None,
                    keymap=(
                        ("transform.vert_slide", dict(release_confirm=True),
                         dict(type='ACTIONMOUSE', value='PRESS')),
                    ),
                ),
            ),
            # End group.

            (
                dict(
                    text="Spin",
                    icon="ops.mesh.spin",
                    widget=None,
                    keymap=(
                    ("mesh.spin", dict(),
                     dict(type='ACTIONMOUSE', value='PRESS')),
                    ),
                ),
                dict(
                    text="Spin (Duplicate)",
                    icon="ops.mesh.spin.duplicate",
                    widget=None,
                    keymap=(
                        ("mesh.spin", dict(dupli=True),
                         dict(type='ACTIONMOUSE', value='PRESS')),
                    ),
                ),
            ),


            dict(
                text="Inset Faces",
                icon="ops.mesh.inset",
                widget=None,
                keymap=(
                    ("mesh.inset", dict(),
                     dict(type='ACTIONMOUSE', value='PRESS')),
                ),
            ),

            (
                dict(
                    text="Extrude Region",
                    icon="ops.view3d.edit_mesh_extrude",
                    widget=None,
                    keymap=(
                        ("view3d.edit_mesh_extrude", dict(),
                         dict(type='ACTIONMOUSE', value='PRESS')),
                    ),
                ),
                dict(
                    text="Extrude Individual",
                    icon="ops.view3d.edit_mesh_extrude_individual",
                    widget=None,
                    keymap=(
                        ("mesh.extrude_faces_move", dict(),
                         dict(type='ACTIONMOUSE', value='PRESS')),
                    ),
                ),
            ),

            (
                dict(
                    text="Randomize",
                    icon="ops.transform.vertex_random",
                    widget=None,
                    keymap=(
                        ("transform.vertex_random", dict(),
                         dict(type='ACTIONMOUSE', value='PRESS')),
                    ),
                ),
                dict(
                    text="Smooth",
                    icon="ops.mesh.vertices_smooth",
                    widget=None,
                    keymap=(
                        ("mesh.vertices_smooth", dict(),
                         dict(type='ACTIONMOUSE', value='PRESS')),
                    ),
                ),
            ),

            (
                dict(
                    text="Shrink/Fatten",
                    icon="ops.transform.shrink_fatten",
                    widget=None,
                    keymap=(
                        ("transform.shrink_fatten", dict(release_confirm=True),
                         dict(type='ACTIONMOUSE', value='PRESS')),
                    ),
                ),
                dict(
                    text="Push/Pull",
                    icon="ops.transform.push_pull",
                    widget=None,
                    keymap=(
                        ("transform.push_pull", dict(release_confirm=True),
                         dict(type='ACTIONMOUSE', value='PRESS')),
                    ),
                ),
            ),

            # Knife Group
            (
                dict(
                    text="Knife",
                    icon="ops.mesh.knife_tool",
                    widget=None,
                    keymap=(
                        ("mesh.knife_tool",
                         dict(wait_for_input=False, use_occlude_geometry=True, only_selected=False),
                         dict(type='ACTIONMOUSE', value='PRESS')),
                    ),
                ),
                None,
                dict(
                    text="Bisect",
                    icon="ops.mesh.bisect",
                    widget=None,
                    keymap=(
                        ("mesh.bisect",
                         dict(),
                         dict(type='EVT_TWEAK_A', value='ANY')),
                    ),
                ),
            ),
            # End group.
            dict(
                text="Extrude Cursor",
                icon=None,
                widget=None,
                keymap=(
                    ("mesh.dupli_extrude_cursor", dict(), dict(type='ACTIONMOUSE', value='PRESS')),
                ),
            ),
        ],
        'EDIT_CURVE': [
            *_tools_transform,
            None,
            dict(
                text="Draw",
                icon=None,
                widget=None,
                keymap=(
                    ("curve.draw", dict(wait_for_input=False), dict(type='ACTIONMOUSE', value='PRESS')),
                ),
            ),
            dict(
                text="Extrude Cursor",
                icon=None,
                widget=None,
                keymap=(
                    ("curve.vertex_add", dict(), dict(type='ACTIONMOUSE', value='PRESS')),
                ),
            ),
        ],
    }


classes = (
    VIEW3D_PT_tools_active,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
