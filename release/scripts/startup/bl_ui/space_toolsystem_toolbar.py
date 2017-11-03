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
    bl_label = "Active Tool (Test)"
    bl_options = {'DEFAULT_CLOSED'}

    # Satisfy the 'ToolSelectPanelHelper' API.
    keymap_prefix = "3D View Tool: "

    @classmethod
    def tools_from_context(cls, context):
        return (cls._tools[None], cls._tools.get(context.mode, ()))

    @classmethod
    def tools_all(cls):
        return [t for t_list in cls._tools.values() for t in t_list]

    # Internal Data

    # for reuse
    _tools_transform = (
        ("Translate", None,
         (("transform.translate", dict(release_confirm=True), dict(type='EVT_TWEAK_A', value='ANY')),)),
        ("Rotate", None,
         (("transform.rotate", dict(release_confirm=True), dict(type='EVT_TWEAK_A', value='ANY')),)),
        ("Scale", None,
         (("transform.resize", dict(release_confirm=True), dict(type='EVT_TWEAK_A', value='ANY')),)),
        ("Scale Cage", "VIEW3D_WGT_xform_cage", None),
    )

    _tools = {
        None: [
            ("Cursor", None,
             (("view3d.cursor3d", dict(), dict(type='ACTIONMOUSE', value='CLICK')),)),

            # 'Select' Group
            (
                ("Select Border", None, (
                    ("view3d.select_border", dict(deselect=False), dict(type='EVT_TWEAK_A', value='ANY')),
                    ("view3d.select_border", dict(deselect=True), dict(type='EVT_TWEAK_A', value='ANY', ctrl=True)),
                )),
                ("Select Circle", None, (
                    ("view3d.select_circle", dict(deselect=False), dict(type='ACTIONMOUSE', value='PRESS')),
                    ("view3d.select_circle", dict(deselect=True), dict(type='ACTIONMOUSE', value='PRESS', ctrl=True)),
                )),
                ("Select Lasso", None, (
                    ("view3d.select_lasso",
                     dict(deselect=False), dict(type='EVT_TWEAK_A', value='ANY')),
                    ("view3d.select_lasso",
                     dict(deselect=True), dict(type='EVT_TWEAK_A', value='ANY', ctrl=True)),
                )),
            ),
            # End group.
        ],
        'OBJECT': [
            *_tools_transform,
        ],
        'POSE': [
            *_tools_transform,
        ],
        'EDIT_ARMATURE': [
            *_tools_transform,
            ("Roll", None, (
                ("transform.transform",
                 dict(release_confirm=True, mode='BONE_ROLL'),
                 dict(type='EVT_TWEAK_A', value='ANY')),
            )),
            None,
            ("Extrude Cursor", None,
             (("armature.click_extrude", dict(), dict(type='ACTIONMOUSE', value='PRESS')),)),
        ],
        'EDIT_MESH': [
            *_tools_transform,
            None,
            ("Rip Region", None, (
                ("mesh.rip_move", dict(TRANSFORM_OT_translate=dict(release_confirm=True)),
                 dict(type='ACTIONMOUSE', value='PRESS')),
            )),
            ("Rip Edge", None, (
                ("mesh.rip_edge_move", dict(TRANSFORM_OT_translate=dict(release_confirm=True)),
                 dict(type='ACTIONMOUSE', value='PRESS')),
            )),

            ("Poly Build", None, (
                ("mesh.polybuild_face_at_cursor_move",
                 dict(TRANSFORM_OT_translate=dict(release_confirm=True)),
                 dict(type='ACTIONMOUSE', value='PRESS')),
                ("mesh.polybuild_split_at_cursor_move",
                 dict(TRANSFORM_OT_translate=dict(release_confirm=True)),
                 dict(type='ACTIONMOUSE', value='PRESS', ctrl=True)),
                ("mesh.polybuild_dissolve_at_cursor", dict(), dict(type='ACTIONMOUSE', value='CLICK', alt=True)),
                ("mesh.polybuild_hover", dict(use_boundary=False), dict(type='MOUSEMOVE', value='ANY', alt=True)),
                ("mesh.polybuild_hover", dict(use_boundary=True), dict(type='MOUSEMOVE', value='ANY', any=True)),
            )),

            # Knife Group
            (
                ("Knife", None, (
                    ("mesh.knife_tool",
                     dict(wait_for_input=False, use_occlude_geometry=True, only_selected=False),
                     dict(type='ACTIONMOUSE', value='PRESS')),)),
                ("Knife (Selected)", None, (
                    ("mesh.knife_tool",
                     dict(wait_for_input=False, use_occlude_geometry=False, only_selected=True),
                     dict(type='ACTIONMOUSE', value='PRESS')),)),
                None,
                ("Bisect", None, (
                    ("mesh.bisect",
                     dict(),
                     dict(type='EVT_TWEAK_A', value='ANY')),)),
            ),
            # End group.
            ("Extrude Cursor", None,
             (("mesh.dupli_extrude_cursor", dict(), dict(type='ACTIONMOUSE', value='PRESS')),)),
        ],
        'EDIT_CURVE': [
            *_tools_transform,
            None,
            ("Draw", None,
             (("curve.draw", dict(wait_for_input=False), dict(type='ACTIONMOUSE', value='PRESS')),)),
            ("Extrude Cursor", None,
             (("curve.vertex_add", dict(), dict(type='ACTIONMOUSE', value='PRESS')),)),
        ],
    }


classes = (
    VIEW3D_PT_tools_active,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
