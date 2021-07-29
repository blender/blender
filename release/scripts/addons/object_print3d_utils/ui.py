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

# <pep8-80 compliant>

# Interface for this addon.

import bmesh
from bpy.types import Panel
from . import report


class Print3DToolBar:
    bl_label = "Print3D"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'TOOLS'

    _type_to_icon = {
        bmesh.types.BMVert: 'VERTEXSEL',
        bmesh.types.BMEdge: 'EDGESEL',
        bmesh.types.BMFace: 'FACESEL',
        }

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        return obj and obj.type == 'MESH' and context.mode in {'OBJECT','EDIT_MESH'}

    @staticmethod
    def draw_report(layout, context):
        """Display Reports"""
        info = report.info()
        if info:
            obj = context.edit_object

            layout.label("Output:")
            box = layout.box()
            col = box.column(align=False)
            # box.alert = True
            for i, (text, data) in enumerate(info):
                if obj and data and data[1]:
                    bm_type, bm_array = data
                    col.operator("mesh.print3d_select_report",
                                 text=text,
                                 icon=Print3DToolBar._type_to_icon[bm_type]).index = i
                    layout.operator("mesh.select_non_manifold", text='Non Manifold Extended')
                else:
                    col.label(text)


    def draw(self, context):
        layout = self.layout

        scene = context.scene
        print_3d = scene.print_3d

        # TODO, presets

        row = layout.row()
        row.label("Statistics:")
        rowsub = layout.row(align=True)
        rowsub.operator("mesh.print3d_info_volume", text="Volume")
        rowsub.operator("mesh.print3d_info_area", text="Area")

        row = layout.row()
        row.label("Checks:")
        col = layout.column(align=True)
        col.operator("mesh.print3d_check_solid", text="Solid")
        col.operator("mesh.print3d_check_intersect", text="Intersections")
        rowsub = col.row(align=True)
        rowsub.operator("mesh.print3d_check_degenerate", text="Degenerate")
        rowsub.prop(print_3d, "threshold_zero", text="")
        rowsub = col.row(align=True)
        rowsub.operator("mesh.print3d_check_distort", text="Distorted")
        rowsub.prop(print_3d, "angle_distort", text="")
        rowsub = col.row(align=True)
        rowsub.operator("mesh.print3d_check_thick", text="Thickness")
        rowsub.prop(print_3d, "thickness_min", text="")
        rowsub = col.row(align=True)
        rowsub.operator("mesh.print3d_check_sharp", text="Edge Sharp")
        rowsub.prop(print_3d, "angle_sharp", text="")
        rowsub = col.row(align=True)
        rowsub.operator("mesh.print3d_check_overhang", text="Overhang")
        rowsub.prop(print_3d, "angle_overhang", text="")
        col = layout.column()
        col.operator("mesh.print3d_check_all", text="Check All")

        row = layout.row()
        row.label("Cleanup:")
        col = layout.column(align=True)
        col.operator("mesh.print3d_clean_isolated", text="Isolated")
        rowsub = col.row(align=True)
        rowsub.operator("mesh.print3d_clean_distorted", text="Distorted")
        rowsub.prop(print_3d, "angle_distort", text="")
        col = layout.column()
        col.operator("mesh.print3d_clean_non_manifold", text="Make Manifold")
        # XXX TODO
        # col.operator("mesh.print3d_clean_thin", text="Wall Thickness")

        row = layout.row()
        row.label("Scale To:")
        rowsub = layout.row(align=True)
        rowsub.operator("mesh.print3d_scale_to_volume", text="Volume")
        rowsub.operator("mesh.print3d_scale_to_bounds", text="Bounds")

        col = layout.column()
        rowsub = col.row(align=True)
        rowsub.label("Export Path:")
        rowsub.prop(print_3d, "use_apply_scale", text="", icon='MAN_SCALE')
        rowsub.prop(print_3d, "use_export_texture", text="", icon='FILE_IMAGE')
        rowsub = col.row()
        rowsub.prop(print_3d, "export_path", text="")

        rowsub = col.row(align=True)
        rowsub.prop(print_3d, "export_format", text="")
        rowsub.operator("mesh.print3d_export", text="Export", icon='EXPORT')

        Print3DToolBar.draw_report(layout, context)


# So we can have a panel in both object mode and editmode
class Print3DToolBarObject(Panel, Print3DToolBar):
    bl_category = "3D Printing"
    bl_idname = "MESH_PT_print3d_object"
    bl_context = "objectmode"


class Print3DToolBarMesh(Panel, Print3DToolBar):
    bl_category = "3D Printing"
    bl_idname = "MESH_PT_print3d_mesh"
    bl_context = "mesh_edit"
