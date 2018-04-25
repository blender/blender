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
        return [t for t_list in cls._tools.values() for t in t_list]

    # Internal Data

    # for reuse
    _tools_transform = (
        ("Translate", "ops.transform.translate", "TRANSFORM_WGT_manipulator",
         (("transform.translate", dict(release_confirm=True), dict(type='EVT_TWEAK_A', value='ANY')),)),
        ("Rotate", "ops.transform.rotate", "TRANSFORM_WGT_manipulator",
         (("transform.rotate", dict(release_confirm=True), dict(type='EVT_TWEAK_A', value='ANY')),)),
        (
            ("Scale", "ops.transform.resize", "TRANSFORM_WGT_manipulator",
             (("transform.resize", dict(release_confirm=True), dict(type='EVT_TWEAK_A', value='ANY')),)),
            ("Scale Cage", "ops.transform.resize.cage", "VIEW3D_WGT_xform_cage", None),
        ),
        None,
        ("Ruler/Protractor", "ops.view3d.ruler", "VIEW3D_WGT_ruler",
         (("view3d.ruler_add", dict(), dict(type='EVT_TWEAK_A', value='ANY')),)),

        # DEBUGGING ONLY
        # ("Pixel Test", "tool_icon.pixeltest", None, (("wm.splash", dict(), dict(type='ACTIONMOUSE', value='PRESS')),)),
    )

    _tools = {
        None: [
            ("Cursor", "ops.generic.cursor", None,
             (("view3d.cursor3d", dict(), dict(type='ACTIONMOUSE', value='CLICK')),)),

            # 'Select' Group
            (
                ("Select Border", "ops.generic.select_border", None, (
                    ("view3d.select_border", dict(deselect=False), dict(type='EVT_TWEAK_A', value='ANY')),
                    ("view3d.select_border", dict(deselect=True), dict(type='EVT_TWEAK_A', value='ANY', ctrl=True)),
                )),
                ("Select Circle", "ops.generic.select_circle", None, (
                    ("view3d.select_circle", dict(deselect=False), dict(type='ACTIONMOUSE', value='PRESS')),
                    ("view3d.select_circle", dict(deselect=True), dict(type='ACTIONMOUSE', value='PRESS', ctrl=True)),
                )),
                ("Select Lasso", "ops.generic.select_lasso", None, (
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
        'PAINT_WEIGHT': [
            # TODO, override brush events
            (
                ("Linear Gradient", None, None, (
                    ("paint.weight_gradient", dict(type='LINEAR'),
                     dict(type='EVT_TWEAK_A', value='ANY')),
                )),
                ("Radial Gradient", None, None, (
                    ("paint.weight_gradient", dict(type='RADIAL'),
                     dict(type='EVT_TWEAK_A', value='ANY')),
                )),
            ),
        ],
        'EDIT_ARMATURE': [
            *_tools_transform,
            ("Roll", None, None, (
                ("transform.transform",
                 dict(release_confirm=True, mode='BONE_ROLL'),
                 dict(type='EVT_TWEAK_A', value='ANY')),
            )),
            None,
            ("Extrude Cursor", "ops.armature.extrude", None,
             (("armature.click_extrude", dict(), dict(type='ACTIONMOUSE', value='PRESS')),)),
        ],
        'EDIT_MESH': [
            *_tools_transform,
            None,
            (
                ("Rip Region", "ops.mesh.rip", None, (
                    ("mesh.rip_move", dict(),
                     dict(type='ACTIONMOUSE', value='PRESS')),
                )),
                ("Rip Edge", "ops.mesh.rip_edge", None, (
                    ("mesh.rip_edge_edge_move", dict(),
                     dict(type='ACTIONMOUSE', value='PRESS')),
                )),
            ),

            ("Poly Build", "ops.mesh.polybuild_hover", None, (
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


            # 'Slide' Group
            (
                ("Edge Slide", "ops.transform.edge_slide", None, (
                    ("transform.edge_slide", dict(release_confirm=True),
                     dict(type='ACTIONMOUSE', value='PRESS')),
                )),
                ("Vertex Slide", "ops.transform.edge_slide", None, (
                    ("transform.vert_slide", dict(release_confirm=True),
                     dict(type='ACTIONMOUSE', value='PRESS')),
                )),
            ),
            # End group.

            (
                ("Spin", "ops.mesh.spin", None, (
                    ("mesh.spin", dict(),
                     dict(type='ACTIONMOUSE', value='PRESS')),
                )),
                ("Spin (Duplicate)", "ops.mesh.spin.duplicate", None, (
                    ("mesh.spin", dict(dupli=True),
                     dict(type='ACTIONMOUSE', value='PRESS')),
                )),
            ),


            ("Inset Faces", "ops.mesh.inset", None, (
                ("mesh.inset", dict(),
                 dict(type='ACTIONMOUSE', value='PRESS')),
            )),

            (
                ("Extrude Region", "ops.view3d.edit_mesh_extrude", None, (
                    ("view3d.edit_mesh_extrude", dict(),
                     dict(type='ACTIONMOUSE', value='PRESS')),
                )),
                ("Extrude Individual", "ops.view3d.edit_mesh_extrude_individual", None, (
                    ("mesh.extrude_faces_move", dict(),
                     dict(type='ACTIONMOUSE', value='PRESS')),
                )),
            ),

            (
                ("Randomize", "ops.transform.vertex_random", None, (
                    ("transform.vertex_random", dict(),
                     dict(type='ACTIONMOUSE', value='PRESS')),
                )),
                ("Smooth", "ops.mesh.vertices_smooth", None, (
                    ("mesh.vertices_smooth", dict(),
                     dict(type='ACTIONMOUSE', value='PRESS')),
                )),
            ),

            (
                ("Shrink/Fatten", "ops.transform.shrink_fatten", None, (
                    ("transform.shrink_fatten", dict(release_confirm=True),
                     dict(type='ACTIONMOUSE', value='PRESS')),
                )),
                ("Push/Pull", "ops.transform.push_pull", None, (
                    ("transform.push_pull", dict(release_confirm=True),
                     dict(type='ACTIONMOUSE', value='PRESS')),
                )),
            ),

            # Knife Group
            (
                ("Knife", "ops.mesh.knife_tool", None, (
                    ("mesh.knife_tool",
                     dict(wait_for_input=False, use_occlude_geometry=True, only_selected=False),
                     dict(type='ACTIONMOUSE', value='PRESS')),)),
                None,
                ("Bisect", "ops.mesh.bisect", None, (
                    ("mesh.bisect",
                     dict(),
                     dict(type='EVT_TWEAK_A', value='ANY')),)),
            ),
            # End group.
            ("Extrude Cursor", None, None,
             (("mesh.dupli_extrude_cursor", dict(), dict(type='ACTIONMOUSE', value='PRESS')),)),
        ],
        'EDIT_CURVE': [
            *_tools_transform,
            None,
            ("Draw", None, None,
             (("curve.draw", dict(wait_for_input=False), dict(type='ACTIONMOUSE', value='PRESS')),)),
            ("Extrude Cursor", None, None,
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
