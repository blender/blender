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

# --------------------------------- DUAL MESH -------------------------------- #
# -------------------------------- version 0.3 ------------------------------- #
#                                                                              #
# Convert a generic mesh to its dual. With open meshes it can get some wired   #
# effect on the borders.                                                       #
#                                                                              #
#                        (c)   Alessandro Zomparelli                           #
#                                    (2017)                                    #
#                                                                              #
# http://www.co-de-it.com/                                                     #
#                                                                              #
# ############################################################################ #

bl_info = {
    "name": "Dual Mesh",
    "author": "Alessandro Zomparelli (Co-de-iT)",
    "version": (0, 3),
    "blender": (2, 7, 8),
    "location": "",
    "description": "Convert a generic mesh to its dual",
    "warning": "",
    "wiki_url": "",
    "category": "Mesh"}

import bpy
from bpy.types import Operator
from bpy.props import (
        BoolProperty,
        EnumProperty,
        )
import bmesh


class dual_mesh(Operator):
    bl_idname = "object.dual_mesh"
    bl_label = "Dual Mesh"
    bl_description = ("Convert a generic mesh into a polygonal mesh")
    bl_options = {'REGISTER', 'UNDO'}

    quad_method = EnumProperty(
            items=[('BEAUTY', 'Beauty',
                    'Split the quads in nice triangles, slower method'),
                    ('FIXED', 'Fixed',
                    'Split the quads on the 1st and 3rd vertices'),
                    ('FIXED_ALTERNATE', 'Fixed Alternate',
                    'Split the quads on the 2nd and 4th vertices'),
                    ('SHORTEST_DIAGONAL', 'Shortest Diagonal',
                    'Split the quads based on the distance between the vertices')
                    ],
            name="Quad Method",
            description="Method for splitting the quads into triangles",
            default="FIXED",
            options={'LIBRARY_EDITABLE'}
            )
    polygon_method = EnumProperty(
            items=[
                ('BEAUTY', 'Beauty', 'Arrange the new triangles evenly'),
                ('CLIP', 'Clip',
                 'Split the polygons with an ear clipping algorithm')],
            name="Polygon Method",
            description="Method for splitting the polygons into triangles",
            default="BEAUTY",
            options={'LIBRARY_EDITABLE'}
            )
    preserve_borders = BoolProperty(
            name="Preserve Borders",
            default=True,
            description="Preserve original borders"
            )
    apply_modifiers = BoolProperty(
            name="Apply Modifiers",
            default=True,
            description="Apply object's modifiers"
            )

    def execute(self, context):
        mode = context.mode
        if mode == 'EDIT_MESH':
            mode = 'EDIT'
        act = bpy.context.active_object
        if mode != 'OBJECT':
            sel = [act]
            bpy.ops.object.mode_set(mode='OBJECT')
        else:
            sel = bpy.context.selected_objects
        doneMeshes = []

        '''
        if self.new_object:
            bpy.ops.object.duplicate_move()
            for i in range(len(sel)):
                bpy.context.selected_objects[i].name = sel[i].name + "_dual"
                if sel[i] == act:
                    bpy.context.scene.objects.active = bpy.context.selected_objects[i]
            sel = bpy.context.selected_objects
        '''

        for ob0 in sel:
            if ob0.type != 'MESH':
                continue
            if ob0.data.name in doneMeshes:
                continue
            ob = ob0
            mesh_name = ob0.data.name

            # store linked objects
            clones = []
            n_users = ob0.data.users
            count = 0
            for o in bpy.data.objects:
                if o.type != 'MESH':
                    continue
                if o.data.name == mesh_name:
                    count += 1
                    clones.append(o)
                if count == n_users:
                    break

            if self.apply_modifiers:
                bpy.ops.object.convert(target='MESH')
            ob.data = ob.data.copy()
            bpy.ops.object.select_all(action='DESELECT')
            ob.select = True
            bpy.context.scene.objects.active = ob0
            bpy.ops.object.mode_set(mode='EDIT')

            # prevent borders erosion
            bpy.ops.mesh.select_mode(
                    use_extend=False, use_expand=False, type='EDGE'
                    )
            bpy.ops.mesh.select_non_manifold(
                    extend=False, use_wire=False, use_boundary=True,
                    use_multi_face=False, use_non_contiguous=False,
                    use_verts=False
                    )
            bpy.ops.mesh.extrude_region_move(
                    MESH_OT_extrude_region={"mirror": False},
                    TRANSFORM_OT_translate={"value": (0, 0, 0),
                    "constraint_axis": (False, False, False),
                    "constraint_orientation": 'GLOBAL', "mirror": False,
                    "proportional": 'DISABLED',
                    "proportional_edit_falloff": 'SMOOTH', "proportional_size": 1,
                    "snap": False, "snap_target": 'CLOSEST',
                    "snap_point": (0, 0, 0), "snap_align": False,
                    "snap_normal": (0, 0, 0), "gpencil_strokes": False,
                    "texture_space": False, "remove_on_cancel": False,
                    "release_confirm": False}
                    )

            bpy.ops.mesh.select_mode(
                    use_extend=False, use_expand=False, type='VERT',
                    action='TOGGLE'
                    )
            bpy.ops.mesh.select_all(action='SELECT')
            bpy.ops.mesh.quads_convert_to_tris(
                    quad_method=self.quad_method, ngon_method=self.polygon_method
                    )
            bpy.ops.mesh.select_all(action='DESELECT')
            bpy.ops.object.mode_set(mode='OBJECT')
            bpy.ops.object.modifier_add(type='SUBSURF')
            ob.modifiers[-1].name = "dual_mesh_subsurf"
            while True:
                bpy.ops.object.modifier_move_up(modifier="dual_mesh_subsurf")
                if ob.modifiers[0].name == "dual_mesh_subsurf":
                    break

            bpy.ops.object.modifier_apply(
                    apply_as='DATA', modifier='dual_mesh_subsurf'
                    )
            bpy.ops.object.mode_set(mode='EDIT')
            bpy.ops.mesh.select_all(action='DESELECT')

            verts = ob.data.vertices

            bpy.ops.object.mode_set(mode='OBJECT')
            verts[0].select = True
            bpy.ops.object.mode_set(mode='EDIT')
            bpy.ops.mesh.select_more(use_face_step=False)

            bpy.ops.mesh.select_similar(
                type='EDGE', compare='EQUAL', threshold=0.01)
            bpy.ops.mesh.select_all(action='INVERT')
            bpy.ops.mesh.dissolve_verts()
            bpy.ops.mesh.select_all(action='DESELECT')

            bpy.ops.mesh.select_non_manifold(
                extend=False, use_wire=False, use_boundary=True,
                use_multi_face=False, use_non_contiguous=False, use_verts=False)
            bpy.ops.mesh.select_more()

            # find boundaries
            bpy.ops.object.mode_set(mode='OBJECT')
            bound_v = [v.index for v in ob.data.vertices if v.select]
            bound_e = [e.index for e in ob.data.edges if e.select]
            bound_p = [p.index for p in ob.data.polygons if p.select]
            bpy.ops.object.mode_set(mode='EDIT')

            # select quad faces
            bpy.context.tool_settings.mesh_select_mode = (False, False, True)
            bpy.ops.mesh.select_face_by_sides(number=4, extend=False)

            # deselect boundaries
            bpy.ops.object.mode_set(mode='OBJECT')
            for i in bound_v:
                bpy.context.active_object.data.vertices[i].select = False
            for i in bound_e:
                bpy.context.active_object.data.edges[i].select = False
            for i in bound_p:
                bpy.context.active_object.data.polygons[i].select = False

            bpy.ops.object.mode_set(mode='EDIT')

            bpy.context.tool_settings.mesh_select_mode = (False, False, True)
            bpy.ops.mesh.edge_face_add()
            bpy.context.tool_settings.mesh_select_mode = (True, False, False)
            bpy.ops.mesh.select_all(action='DESELECT')

            # delete boundaries
            bpy.ops.mesh.select_non_manifold(
                    extend=False, use_wire=True, use_boundary=True,
                    use_multi_face=False, use_non_contiguous=False, use_verts=True
                    )
            bpy.ops.mesh.delete(type='VERT')

            # remove middle vertices
            bm = bmesh.from_edit_mesh(ob.data)
            for v in bm.verts:
                if len(v.link_edges) == 2 and len(v.link_faces) < 3:
                    v.select_set(True)

            # dissolve
            bpy.ops.mesh.dissolve_verts()
            bpy.ops.mesh.select_all(action='DESELECT')

            # remove border faces
            if not self.preserve_borders:
                bpy.ops.mesh.select_non_manifold(
                    extend=False, use_wire=False, use_boundary=True,
                    use_multi_face=False, use_non_contiguous=False, use_verts=False
                    )
                bpy.ops.mesh.select_more()
                bpy.ops.mesh.delete(type='FACE')

            # clean wires
            bpy.ops.mesh.select_non_manifold(
                    extend=False, use_wire=True, use_boundary=False,
                    use_multi_face=False, use_non_contiguous=False, use_verts=False
                    )
            bpy.ops.mesh.delete(type='EDGE')

            bpy.ops.object.mode_set(mode='OBJECT')
            ob0.data.name = mesh_name
            doneMeshes.append(mesh_name)

            for o in clones:
                o.data = ob.data

        for o in sel:
            o.select = True

        bpy.context.scene.objects.active = act
        bpy.ops.object.mode_set(mode=mode)

        return {'FINISHED'}


"""
class dual_mesh_panel(bpy.types.Panel):
    bl_label = "Dual Mesh"
    bl_category = "Tools"
    bl_space_type = "VIEW_3D"
    bl_region_type = "TOOLS"
    bl_context = "objectmode"

    def draw(self, context):
        layout = self.layout
        col = layout.column(align=True)
        try:
            if bpy.context.active_object.type == 'MESH':
                col.operator("object.dual_mesh")
        except:
            pass
"""


def register():
    bpy.utils.register_class(dual_mesh)
    # bpy.utils.register_class(dual_mesh_panel)


def unregister():
    bpy.utils.unregister_class(dual_mesh)
    # bpy.utils.unregister_class(dual_mesh_panel)


if __name__ == "__main__":
    register()
