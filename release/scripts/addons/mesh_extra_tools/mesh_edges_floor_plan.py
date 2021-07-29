# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; version 2
#  of the License.
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

# based upon the functionality of Mesh to wall by luxuy_BlenderCN
# thanks to meta-androcto

bl_info = {
    "name": "Edge Floor Plan",
    "author": "lijenstina",
    "version": (0, 2),
    "blender": (2, 78, 0),
    "location": "View3D > EditMode > Mesh",
    "description": "Make a Floor Plan from Edges",
    "wiki_url": "",
    "category": "Mesh"}

import bpy
import bmesh
from bpy.types import Operator
from bpy.props import (
        BoolProperty,
        EnumProperty,
        FloatProperty,
        FloatVectorProperty,
        IntProperty,
        )


# Handle error notifications
def error_handlers(self, error, reports="ERROR"):
    if self and reports:
        self.report({'WARNING'}, reports + " (See Console for more info)")

    print("\n[mesh.edges_floor_plan]\nError: {}\n".format(error))


class MESH_OT_edges_floor_plan(Operator):
    bl_idname = "mesh.edges_floor_plan"
    bl_label = "Edges Floor Plan"
    bl_description = "Top View, Extrude Flat Along Edges"
    bl_options = {'REGISTER', 'UNDO'}

    wid = FloatProperty(
            name="Wall width:",
            description="Set the width of the generated walls\n",
            default=0.1,
            min=0.001, max=30000
            )
    depth = FloatProperty(
            name="Inner height:",
            description="Set the height of the inner wall edges",
            default=0.0,
            min=0, max=10
            )
    connect_ends = BoolProperty(
            name="Connect Ends",
            description="Connect the ends of the boundary Edge loops",
            default=False
            )
    repeat_cleanup = IntProperty(
            name="Recursive Prepare",
            description="Number of times that the preparation phase runs\n"
                        "at the start of the script\n"
                        "If parts of the mesh are not modified, increase this value",
            min=1, max=20,
            default=1
            )
    fill_items = [
            ('EDGE_NET', "Edge Net",
             "Edge Net Method for mesh preparation - Initial Fill\n"
             "The filled in faces will be Inset individually\n"
             "Supports simple 3D objects"),
            ('SINGLE_FACE', "Single Face",
             "Single Face Method for mesh preparation - Initial Fill\n"
             "The produced face will be Triangulated before Inset Region\n"
             "Good for edges forming a circle, avoid 3D objects"),
            ('SOLIDIFY', "Solidify",
             "Extrude and Solidify Method\n"
             "Useful for complex meshes, however works best on flat surfaces\n"
             "as the extrude direction has to be defined")
            ]
    fill_type = EnumProperty(
            name="Fill Type",
            items=fill_items,
            description="Choose the method for creating geometry",
            default='SOLIDIFY'
            )
    keep_faces = BoolProperty(
            name="Keep Faces",
            description="Keep or not the fill faces\n"
                        "Can depend on Remove Ngons state",
            default=False
            )
    tri_faces = BoolProperty(
            name="Triangulate Faces",
            description="Triangulate the created fill faces\n"
                        "Sometimes can lead to unsatisfactory results",
            default=False
            )
    initial_extrude = FloatVectorProperty(
            name="Initial Extrude",
            description="",
            default=(0.0, 0.0, 0.1),
            min=-20.0, max=20.0,
            subtype='XYZ',
            precision=3,
            size=3
            )
    remove_ngons = BoolProperty(
            name="Remove Ngons",
            description="Keep or not the Ngon Faces\n"
                        "Note about limitations:\n"
                        "Sometimes the kept Faces could be Ngons\n"
                        "Removing the Ngons can lead to no geometry created",
            default=True
            )
    offset = FloatProperty(
            name="Wall Offset:",
            description="Set the offset for the Solidify modifier",
            default=0.0,
            min=-1.0, max=1.0
            )
    only_rim = BoolProperty(
            name="Rim Only",
            description="Solidify Fill Rim only option",
            default=False
            )

    @classmethod
    def poll(cls, context):
        ob = context.active_object
        return (ob and ob.type == 'MESH' and context.mode == 'EDIT_MESH')

    def check_edge(self, context):
        bpy.ops.object.mode_set(mode='OBJECT')
        bpy.ops.object.mode_set(mode='EDIT')
        obj = bpy.context.object
        me_check = obj.data
        if len(me_check.edges) < 1:
            return False

        return True

    @staticmethod
    def ensure(bm):
        if bm:
            bm.verts.ensure_lookup_table()
            bm.edges.ensure_lookup_table()
            bm.faces.ensure_lookup_table()

    def solidify_mod(self, context, ob, wid, offset, only_rim):
        try:
            mods = ob.modifiers.new(
                        name="_Mesh_Solidify_Wall", type='SOLIDIFY'
                        )
            mods.thickness = wid
            mods.use_quality_normals = True
            mods.offset = offset
            mods.use_even_offset = True
            mods.use_rim = True
            mods.use_rim_only = only_rim
            mods.show_on_cage = True

            bpy.ops.object.modifier_apply(
                        modifier="_Mesh_Solidify_Wall"
                        )
        except Exception as e:
            error_handlers(self, e,
                           reports="Adding a Solidify Modifier failed")
            pass

    def draw(self, context):
        layout = self.layout

        box = layout.box()
        box.label(text="Choose Method:", icon="SCRIPTWIN")
        box.prop(self, "fill_type")

        col = box.column(align=True)

        if self.fill_type == 'EDGE_NET':
            col.prop(self, "repeat_cleanup")
            col.prop(self, "remove_ngons", toggle=True)

        elif self.fill_type == 'SOLIDIFY':
            col.prop(self, "offset", slider=True)
            col.prop(self, "initial_extrude")

        else:
            col.prop(self, "remove_ngons", toggle=True)
            col.prop(self, "tri_faces", toggle=True)

        box = layout.box()
        box.label(text="Settings:", icon="MOD_BUILD")

        col = box.column(align=True)
        col.prop(self, "wid")

        if self.fill_type != 'SOLIDIFY':
            col.prop(self, "depth")
            col.prop(self, "connect_ends", toggle=True)
            col.prop(self, "keep_faces", toggle=True)
        else:
            col.prop(self, "only_rim", toggle=True)

    def execute(self, context):
        if not self.check_edge(context):
            self.report({'WARNING'},
                        "Operation Cancelled. Needs a Mesh with at least one edge")
            return {'CANCELLED'}

        wid = self.wid * 0.1
        depth = self.depth * 0.1
        offset = self.offset * 0.1
        store_selection_mode = context.tool_settings.mesh_select_mode
        # Note: the remove_doubles called after bmesh creation would make
        # blender crash with certain meshes - keep it in mind for the future
        bpy.ops.mesh.remove_doubles(threshold=0.003)
        bpy.ops.object.mode_set(mode='OBJECT')
        bpy.ops.object.mode_set(mode='EDIT')
        ob = bpy.context.object

        me = ob.data
        bm = bmesh.from_edit_mesh(me)

        bmesh.ops.delete(bm, geom=bm.faces, context=3)
        self.ensure(bm)
        context.tool_settings.mesh_select_mode = (False, True, False)
        original_edges = [edge.index for edge in bm.edges]
        original_verts = [vert.index for vert in bm.verts]
        self.ensure(bm)
        bpy.ops.mesh.select_all(action='DESELECT')

        if self.fill_type == 'EDGE_NET':
            for i in range(self.repeat_cleanup):
                bmesh.ops.edgenet_prepare(bm, edges=bm.edges)
                self.ensure(bm)
            bmesh.ops.edgenet_fill(bm, edges=bm.edges, mat_nr=0, use_smooth=True, sides=0)
            self.ensure(bm)
            if self.remove_ngons:
                ngons = [face for face in bm.faces if len(face.edges) > 4]
                self.ensure(bm)
                bmesh.ops.delete(bm, geom=ngons, context=5)  # 5 - delete faces
                del ngons
                self.ensure(bm)

        elif self.fill_type == 'SOLIDIFY':
            for vert in bm.verts:
                vert.normal_update()
            self.ensure(bm)
            bmesh.ops.extrude_edge_only(
                    bm, edges=bm.edges, use_select_history=False
                    )
            self.ensure(bm)
            verts_extrude = [vert for vert in bm.verts if vert.index in original_verts]
            self.ensure(bm)
            bmesh.ops.translate(
                bm,
                verts=verts_extrude,
                vec=(self.initial_extrude)
                )
            self.ensure(bm)
            del verts_extrude
            self.ensure(bm)

            for edge in bm.edges:
                if edge.is_boundary:
                    edge.select = True

            bm = bmesh.update_edit_mesh(ob.data, 1, 1)

            bpy.ops.object.mode_set(mode='OBJECT')
            self.solidify_mod(context, ob, wid, offset, self.only_rim)

            bpy.ops.object.mode_set(mode='EDIT')

            context.tool_settings.mesh_select_mode = store_selection_mode

            return {'FINISHED'}

        else:
            bm.faces.new(bm.verts)
            self.ensure(bm)

            if self.tri_faces:
                bmesh.ops.triangle_fill(
                        bm, use_beauty=True, use_dissolve=False, edges=bm.edges
                        )
                self.ensure(bm)

        if self.remove_ngons and self.fill_type != 'EDGE_NET':
            ngons = [face for face in bm.faces if len(face.edges) > 4]
            self.ensure(bm)
            bmesh.ops.delete(bm, geom=ngons, context=5)  # 5 - delete faces
            del ngons
            self.ensure(bm)

        del_boundary = [edge for edge in bm.edges if edge.index not in original_edges]
        self.ensure(bm)

        del original_edges
        self.ensure(bm)

        if self.fill_type == 'EDGE_NET':
            extrude_inner = bmesh.ops.inset_individual(
                    bm, faces=bm.faces, thickness=wid, depth=depth,
                    use_even_offset=True, use_interpolate=False,
                    use_relative_offset=False
                    )
        else:
            extrude_inner = bmesh.ops.inset_region(
                    bm, faces=bm.faces, faces_exclude=[], use_boundary=True,
                    use_even_offset=True, use_interpolate=False,
                    use_relative_offset=False, use_edge_rail=False,
                    thickness=wid, depth=depth, use_outset=False
                    )
        self.ensure(bm)

        del_faces = [faces for faces in bm.faces if faces not in extrude_inner["faces"]]
        self.ensure(bm)
        del extrude_inner
        self.ensure(bm)

        if not self.keep_faces:
            bmesh.ops.delete(bm, geom=del_faces, context=5)  # 5 delete faces
        del del_faces
        self.ensure(bm)

        face_del = set()
        for face in bm.faces:
            for edge in del_boundary:
                if isinstance(edge, bmesh.types.BMEdge):
                    if edge in face.edges:
                        face_del.add(face)
        self.ensure(bm)
        face_del = list(face_del)
        self.ensure(bm)

        del del_boundary
        self.ensure(bm)

        if not self.connect_ends:
            bmesh.ops.delete(bm, geom=face_del, context=5)
            self.ensure(bm)

        del face_del
        self.ensure(bm)

        for edge in bm.edges:
            if edge.is_boundary:
                edge.select = True

        bm = bmesh.update_edit_mesh(ob.data, 1, 1)

        context.tool_settings.mesh_select_mode = store_selection_mode

        return {'FINISHED'}


def register():
    bpy.utils.register_class(MESH_OT_edges_floor_plan)


def unregister():
    bpy.utils.unregister_class(MESH_OT_edges_floor_plan)


if __name__ == "__main__":
    register()
