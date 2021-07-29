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
# --------------------------- LATTICE ALONG SURFACE -------------------------- #
# -------------------------------- version 0.3 ------------------------------- #
#                                                                              #
# Automatically generate and assign a lattice that follows the active surface. #
#                                                                              #
#                        (c)   Alessandro Zomparelli                           #
#                                    (2017)                                    #
#                                                                              #
# http://www.co-de-it.com/                                                     #
#                                                                              #
# ############################################################################ #

bl_info = {
    "name": "Lattice",
    "author": "Alessandro Zomparelli (Co-de-iT)",
    "version": (0, 3),
    "blender": (2, 7, 8),
    "location": "",
    "description": "Generate a Lattice based on a grid mesh",
    "warning": "",
    "wiki_url": "",
    "category": "Mesh"}


import bpy
import bmesh
from bpy.types import Operator
from bpy.props import (
        BoolProperty,
        FloatProperty,
        )
from mathutils import Vector


def not_in(element, grid):
    output = True
    for loop in grid:
        if element in loop:
            output = False
            break
    return output


def grid_from_mesh(mesh, swap_uv):
    bm = bmesh.new()
    bm.from_mesh(mesh)
    verts_grid = []
    edges_grid = []
    faces_grid = []

    running_grid = True
    while running_grid:
        verts_loop = []
        edges_loop = []
        faces_loop = []

        # storing first point
        verts_candidates = []
        if len(faces_grid) == 0:
            # for first loop check all vertices
            verts_candidates = bm.verts
        else:
            # for other loops start form the vertices of the first face
            # the last loop, skipping already used vertices
            verts_candidates = [v for v in bm.faces[faces_grid[-1][0]].verts if not_in(v.index, verts_grid)]

        # check for last loop
        is_last = False
        for vert in verts_candidates:
            if len(vert.link_faces) == 1:   # check if corner vertex
                vert.select = True
                verts_loop.append(vert.index)
                is_last = True
                break

        if not is_last:
            for vert in verts_candidates:
                new_link_faces = [f for f in vert.link_faces if not_in(f.index, faces_grid)]
                if len(new_link_faces) < 2:  # check if corner vertex
                    vert.select = True
                    verts_loop.append(vert.index)
                    break

        running_loop = len(verts_loop) > 0

        while running_loop:
            bm.verts.ensure_lookup_table()
            id = verts_loop[-1]
            link_edges = bm.verts[id].link_edges
            # storing second point
            if len(verts_loop) == 1:            # only one vertex stored in the loop
                if len(faces_grid) == 0:        # first loop #
                    edge = link_edges[swap_uv]  # chose direction
                    for vert in edge.verts:
                        if vert.index != id:
                            vert.select = True
                            verts_loop.append(vert.index)                # new vertex
                            edges_loop.append(edge.index)                # chosen edge
                            faces_loop.append(edge.link_faces[0].index)  # only one face
                            # edge.link_faces[0].select = True
                else:  # other loops #
                    # start from the edges of the first face of the last loop
                    for edge in bm.faces[faces_grid[-1][0]].edges:
                        # chose an edge starting from the first vertex that is not returning back
                        if bm.verts[verts_loop[0]] in edge.verts and \
                                bm.verts[verts_grid[-1][0]] not in edge.verts:
                            for vert in edge.verts:
                                if vert.index != id:
                                    vert.select = True
                                    verts_loop.append(vert.index)
                            edges_loop.append(edge.index)

                            for face in edge.link_faces:
                                if not_in(face.index, faces_grid):
                                    faces_loop.append(face.index)
            # continuing the loop
            else:
                for edge in link_edges:
                    for vert in edge.verts:
                        store_data = False
                        if not_in(vert.index, verts_grid) and vert.index not in verts_loop:
                            if len(faces_loop) > 0:
                                bm.faces.ensure_lookup_table()
                                if vert not in bm.faces[faces_loop[-1]].verts:
                                    store_data = True
                            else:
                                store_data = True
                            if store_data:
                                vert.select = True
                                verts_loop.append(vert.index)
                                edges_loop.append(edge.index)
                                for face in edge.link_faces:
                                    if not_in(face.index, faces_grid):
                                        faces_loop.append(face.index)
                                break
            # ending condition
            if verts_loop[-1] == id or verts_loop[-1] == verts_loop[0]:
                running_loop = False

        verts_grid.append(verts_loop)
        edges_grid.append(edges_loop)
        faces_grid.append(faces_loop)

        if len(faces_loop) == 0:
            running_grid = False

    return verts_grid, edges_grid, faces_grid


class lattice_along_surface(Operator):
    bl_idname = "object.lattice_along_surface"
    bl_label = "Lattice along Surface"
    bl_description = ("Automatically add a Lattice modifier to the selected "
                      "object, adapting it to the active one.\nThe active "
                      "object must be a rectangular grid compatible with the "
                      "Lattice's topology")
    bl_options = {'REGISTER', 'UNDO'}

    set_parent = BoolProperty(
            name="Set Parent",
            default=True,
            description="Automatically set the Lattice as parent"
            )
    flipNormals = BoolProperty(
            name="Flip Normals",
            default=False,
            description="Flip normals direction"
            )
    swapUV = BoolProperty(
            name="Swap UV",
            default=False,
            description="Flip grid's U and V"
            )
    flipU = BoolProperty(
            name="Flip U",
            default=False,
            description="Flip grid's U")

    flipV = BoolProperty(
            name="Flip V",
            default=False,
            description="Flip grid's V"
            )
    flipW = BoolProperty(
            name="Flip W",
            default=False,
            description="Flip grid's W"
            )
    use_groups = BoolProperty(
            name="Vertex Group",
            default=False,
            description="Use active Vertex Group for lattice's thickness"
            )
    high_quality_lattice = BoolProperty(
            name="High quality",
            default=True,
            description="Increase the the subdivisions in normal direction for a "
                        "more correct result"
            )
    hide_lattice = BoolProperty(
            name="Hide Lattice",
            default=True,
            description="Automatically hide the Lattice object"
            )
    scale_x = FloatProperty(
            name="Scale X",
            default=1,
            min=0.001,
            max=1,
            description="Object scale"
            )
    scale_y = FloatProperty(
            name="Scale Y", default=1,
            min=0.001,
            max=1,
            description="Object scale"
            )
    scale_z = FloatProperty(
            name="Scale Z",
            default=1,
            min=0.001,
            max=1,
            description="Object scale"
            )
    thickness = FloatProperty(
            name="Thickness",
            default=1,
            soft_min=0,
            soft_max=5,
            description="Lattice thickness"
            )
    displace = FloatProperty(
            name="Displace",
            default=0,
            soft_min=-1,
            soft_max=1,
            description="Lattice displace"
            )

    def draw(self, context):
        layout = self.layout
        col = layout.column(align=True)
        col.label(text="Thickness:")
        col.prop(
            self, "thickness", text="Thickness", icon='NONE', expand=False,
            slider=True, toggle=False, icon_only=False, event=False,
            full_event=False, emboss=True, index=-1
            )
        col.prop(
            self, "displace", text="Offset", icon='NONE', expand=False,
            slider=True, toggle=False, icon_only=False, event=False,
            full_event=False, emboss=True, index=-1
            )
        row = col.row()
        row.prop(self, "use_groups")
        col.separator()
        col.label(text="Scale:")
        col.prop(
            self, "scale_x", text="U", icon='NONE', expand=False,
            slider=True, toggle=False, icon_only=False, event=False,
            full_event=False, emboss=True, index=-1
            )
        col.prop(
            self, "scale_y", text="V", icon='NONE', expand=False,
            slider=True, toggle=False, icon_only=False, event=False,
            full_event=False, emboss=True, index=-1
            )
        """
        col.prop(
            self, "scale_z", text="W", icon='NONE', expand=False,
            slider=True, toggle=False, icon_only=False, event=False,
            full_event=False, emboss=True, index=-1)
        """
        col.separator()
        col.label(text="Flip:")
        row = col.row()
        row.prop(self, "flipU", text="U")
        row.prop(self, "flipV", text="V")
        row.prop(self, "flipW", text="W")
        col.prop(self, "swapUV")
        col.prop(self, "flipNormals")
        col.separator()
        col.label(text="Lattice Options:")
        col.prop(self, "high_quality_lattice")
        col.prop(self, "hide_lattice")
        col.prop(self, "set_parent")

    def execute(self, context):
        if len(bpy.context.selected_objects) != 2:
            self.report({'ERROR'}, "Please, select two objects")
            return {'CANCELLED'}
        grid_obj = bpy.context.active_object
        if grid_obj.type not in ('MESH', 'CURVE', 'SURFACE'):
            self.report({'ERROR'}, "The surface object is not valid. Only Mesh,"
                        "Curve and Surface objects are allowed.")
            return {'CANCELLED'}
        obj = None
        for o in bpy.context.selected_objects:
            if o.name != grid_obj.name and o.type in \
                    ('MESH', 'CURVE', 'SURFACE', 'FONT'):
                obj = o
                o.select = False
                break
        try:
            obj_dim = obj.dimensions
            obj_me = obj.to_mesh(
                            bpy.context.scene, apply_modifiers=True,
                            settings='PREVIEW'
                            )
        except:
            self.report({'ERROR'}, "The object to deform is not valid. Only "
                        "Mesh, Curve, Surface and Font objects are allowed.")
            return {'CANCELLED'}

        bpy.ops.object.duplicate_move()
        grid_obj = bpy.context.active_object
        bpy.ops.object.convert(target='MESH')
        bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
        grid_mesh = grid_obj.to_mesh(bpy.context.scene, apply_modifiers=True,
                                     settings='PREVIEW')

        if len(grid_mesh.polygons) > 64 * 64:
            bpy.ops.object.delete(use_global=False)
            bpy.context.scene.objects.active = obj
            obj.select = True
            self.report({'ERROR'}, "Maximum resolution allowed for Lattice is 64")
            return {'CANCELLED'}

        # CREATING LATTICE
        min = Vector((0, 0, 0))
        max = Vector((0, 0, 0))
        first = True
        for v in obj_me.vertices:
            vert = obj.matrix_world * v.co
            if vert[0] < min[0] or first:
                min[0] = vert[0]
            if vert[1] < min[1] or first:
                min[1] = vert[1]
            if vert[2] < min[2] or first:
                min[2] = vert[2]
            if vert[0] > max[0] or first:
                max[0] = vert[0]
            if vert[1] > max[1] or first:
                max[1] = vert[1]
            if vert[2] > max[2] or first:
                max[2] = vert[2]
            first = False

        bb = max - min
        lattice_loc = (max + min) / 2
        bpy.ops.object.add(type='LATTICE', view_align=False,
                           enter_editmode=False)
        lattice = bpy.context.active_object
        lattice.location = lattice_loc
        lattice.scale = Vector((bb.x / self.scale_x, bb.y / self.scale_y,
                                bb.z / self.scale_z))

        if bb.x == 0:
            lattice.scale.x = 1
        if bb.y == 0:
            lattice.scale.y = 1
        if bb.z == 0:
            lattice.scale.z = 1

        bpy.context.scene.objects.active = obj
        bpy.ops.object.modifier_add(type='LATTICE')
        obj.modifiers[-1].object = lattice

        # set as parent
        if self.set_parent:
            obj.select = True
            lattice.select = True
            bpy.context.scene.objects.active = lattice
            bpy.ops.object.parent_set(type='LATTICE')

        # reading grid structure
        verts_grid, edges_grid, faces_grid = grid_from_mesh(
                                                grid_mesh,
                                                swap_uv=self.swapUV
                                                )
        nu = len(verts_grid)
        nv = len(verts_grid[0])
        nw = 2
        scale_normal = self.thickness

        try:
            lattice.data.points_u = nu
            lattice.data.points_v = nv
            lattice.data.points_w = nw
            for i in range(nu):
                for j in range(nv):
                    for w in range(nw):
                        if self.use_groups:
                            try:
                                displace = grid_obj.vertex_groups.active.weight(
                                                    verts_grid[i][j]) * scale_normal * bb.z
                            except:
                                displace = scale_normal * bb.z
                        else:
                            displace = scale_normal * bb.z
                        target_point = (grid_mesh.vertices[verts_grid[i][j]].co +
                                        grid_mesh.vertices[verts_grid[i][j]].normal *
                                        (w + self.displace / 2 - 0.5) * displace) - lattice.location
                        if self.flipW:
                            w = 1 - w
                        if self.flipU:
                            i = nu - i - 1
                        if self.flipV:
                            j = nv - j - 1
                        lattice.data.points[i + j * nu + w * nu * nv].co_deform.x = \
                                target_point.x / bpy.data.objects[lattice.name].scale.x
                        lattice.data.points[i + j * nu + w * nu * nv].co_deform.y = \
                                target_point.y / bpy.data.objects[lattice.name].scale.y
                        lattice.data.points[i + j * nu + w * nu * nv].co_deform.z = \
                                target_point.z / bpy.data.objects[lattice.name].scale.z
        except:
            bpy.ops.object.mode_set(mode='OBJECT')
            grid_obj.select = True
            lattice.select = True
            obj.select = False
            bpy.ops.object.delete(use_global=False)
            bpy.context.scene.objects.active = obj
            obj.select = True
            bpy.ops.object.modifier_remove(modifier=obj.modifiers[-1].name)
            if nu > 64 or nv > 64:
                self.report({'ERROR'}, "Maximum resolution allowed for Lattice is 64")
                return {'CANCELLED'}
            else:
                self.report({'ERROR'}, "The grid mesh is not correct")
                return {'CANCELLED'}

        # grid_obj.data = old_grid_data
        # print(old_grid_matrix)
        # grid_obj.matrix_world = old_grid_matrix

        bpy.ops.object.mode_set(mode='OBJECT')
        grid_obj.select = True
        lattice.select = False
        obj.select = False
        bpy.ops.object.delete(use_global=False)
        bpy.context.scene.objects.active = lattice
        lattice.select = True

        if self.high_quality_lattice:
            bpy.context.object.data.points_w = 8
        else:
            bpy.context.object.data.use_outside = True

        if self.hide_lattice:
            bpy.ops.object.hide_view_set(unselected=False)

        bpy.context.scene.objects.active = obj
        obj.select = True
        lattice.select = False

        if self.flipNormals:
            try:
                bpy.ops.object.mode_set(mode='EDIT')
                bpy.ops.mesh.select_all(action='SELECT')
                bpy.ops.mesh.flip_normals()
                bpy.ops.object.mode_set(mode='OBJECT')
            except:
                pass

        return {'FINISHED'}


"""
class lattice_along_surface_panel(bpy.types.Panel):
    bl_label = "Modifiers Tools"
    bl_category = "Tools"
    bl_space_type = "VIEW_3D"
    bl_region_type = "TOOLS"
    bl_context = "objectmode"

    def draw(self, context):
        layout = self.layout
        col = layout.column(align=True)

        try:
            col.operator("object.lattice_along_surface", icon="MOD_LATTICE")
        except:
            pass
"""


def register():
    bpy.utils.register_class(lattice_along_surface)
    # bpy.utils.register_class(lattice_along_surface_panel)


def unregister():
    bpy.utils.unregister_class(lattice_along_surface)
    # bpy.utils.unregister_class(lattice_along_surface_panel)


if __name__ == "__main__":
    register()
