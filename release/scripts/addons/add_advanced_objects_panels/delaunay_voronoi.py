# -*- coding:utf-8 -*-

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

bl_info = {
    "name": "Delaunay Voronoi",
    "description": "Points cloud Delaunay triangulation in 2.5D "
                   "(suitable for terrain modelling) or Voronoi diagram in 2D",
    "author": "Domlysz, Oscurart",
    "version": (1, 3),
    "blender": (2, 7, 0),
    "location": "3D View > Toolshelf > Create > Delaunay Voronoi",
    "warning": "",
    "wiki_url": "https://github.com/domlysz/BlenderGIS/wiki",
    "category": "Add Mesh"
    }

import bpy
from .DelaunayVoronoi import (
        computeVoronoiDiagram,
        computeDelaunayTriangulation,
        )
from bpy.types import (
        Operator,
        Panel,
        )
from bpy.props import EnumProperty

try:
    from scipy.spatial import Delaunay
    import bmesh
    import numpy as np
    HAS_SCIPY = True
except:
    HAS_SCIPY = False
    pass


# Globals
# set to True to enable debug_prints
DEBUG = False


def debug_prints(text=""):
    global DEBUG
    if DEBUG and text:
        print(text)


class Point:
    def __init__(self, x, y, z):
        self.x, self.y, self.z = x, y, z


def unique(L):
    """Return a list of unhashable elements in s, but without duplicates.
    [[1, 2], [2, 3], [1, 2]] >>> [[1, 2], [2, 3]]"""
    # For unhashable objects, you can sort the sequence and
    # then scan from the end of the list, deleting duplicates as you go
    nDupli = 0
    nZcolinear = 0
    # sort() brings the equal elements together; then duplicates
    # are easy to weed out in a single pass
    L.sort()
    last = L[-1]
    for i in range(len(L) - 2, -1, -1):
        if last[:2] == L[i][:2]:    # XY coordinates compararison
            if last[2] == L[i][2]:  # Z coordinates compararison
                nDupli += 1         # duplicates vertices
            else:  # Z colinear
                nZcolinear += 1
            del L[i]
        else:
            last = L[i]
    # list data type is mutable, input list will automatically update
    # and doesn't need to be returned
    return (nDupli, nZcolinear)


def checkEqual(lst):
    return lst[1:] == lst[:-1]


class ToolsPanelDelaunay(Panel):
    bl_category = "Create"
    bl_label = "Delaunay Voronoi"
    bl_space_type = "VIEW_3D"
    bl_context = "objectmode"
    bl_region_type = "TOOLS"
    bl_options = {"DEFAULT_CLOSED"}

    def draw(self, context):
        layout = self.layout

        box = layout.box()
        col = box.column(align=True)
        col.label("Point Cloud:")
        col.operator("delaunay.triangulation")
        col.operator("voronoi.tesselation")


class OBJECT_OT_TriangulateButton(Operator):
    bl_idname = "delaunay.triangulation"
    bl_label = "Triangulation"
    bl_description = ("Terrain points cloud Delaunay triangulation in 2.5D\n"
                      "Needs an existing Active Mesh Object")
    bl_options = {"REGISTER", "UNDO"}

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        return (obj is not None and obj.type == "MESH")

    def execute(self, context):
        # move the check into the poll
        obj = context.active_object

        if HAS_SCIPY:
            # Use scipy when present (~18 x faster)
            bpy.ops.object.mode_set(mode='EDIT')
            bm = bmesh.from_edit_mesh(obj.data)
            points_3D = [list(v.co) for v in bm.verts]
            points_2D = np.array([[v[0], v[1]] for v in points_3D])
            print("Triangulate " + str(len(points_3D)) + " points...")
            # Triangulate
            tri = Delaunay(points_2D)
            faces = tri.simplices.tolist()
            # Create new mesh structure
            print("Create mesh...")
            bpy.ops.object.mode_set(mode='OBJECT')
            mesh = bpy.data.meshes.new("TIN")
            mesh.from_pydata(points_3D, [], faces)
            mesh.update(calc_edges=True)
            my = bpy.data.objects.new("TIN", mesh)
            context.scene.objects.link(my)
            my.matrix_world = obj.matrix_world.copy()
            obj.select = False
            my.select = True
            context.scene.objects.active = my
            self.report({'INFO'}, "Mesh created (" + str(len(faces)) + " triangles)")
            print("Total :%s faces  %s verts" % (len(faces), len(points_3D)))
            return {'FINISHED'}

        # Get points coodinates
        r = obj.rotation_euler
        s = obj.scale
        mesh = obj.data
        vertsPts = [vertex.co for vertex in mesh.vertices]

        # Remove duplicate
        verts = [[vert.x, vert.y, vert.z] for vert in vertsPts]
        nDupli, nZcolinear = unique(verts)
        nVerts = len(verts)

        debug_prints(text=str(nDupli) + " duplicate points ignored")
        debug_prints(str(nZcolinear) + " z colinear points excluded")

        if nVerts < 3:
            self.report({"WARNING"},
                        "Not enough points to continue. Operation Cancelled")

            return {"CANCELLED"}

        # Check colinear
        xValues = [pt[0] for pt in verts]
        yValues = [pt[1] for pt in verts]

        if checkEqual(xValues) or checkEqual(yValues):
            self.report({'ERROR'}, "Points are colinear")
            return {'FINISHED'}

        # Triangulate
        debug_prints(text="Triangulate " + str(nVerts) + " points...")

        vertsPts = [Point(vert[0], vert[1], vert[2]) for vert in verts]
        triangles = computeDelaunayTriangulation(vertsPts)
        # reverse point order --> if all triangles are specified anticlockwise then all faces up
        triangles = [tuple(reversed(tri)) for tri in triangles]

        debug_prints(text=str(len(triangles)) + " triangles")

        # Create new mesh structure
        debug_prints(text="Create mesh...")
        tinMesh = bpy.data.meshes.new("TIN")        # create a new mesh
        tinMesh.from_pydata(verts, [], triangles)   # Fill the mesh with triangles
        tinMesh.update(calc_edges=True)             # Update mesh with new data

        # Create an object with that mesh
        tinObj = bpy.data.objects.new("TIN", tinMesh)

        # Place object
        tinObj.location = obj.location.copy()
        tinObj.rotation_euler = r
        tinObj.scale = s

        # Update scene
        bpy.context.scene.objects.link(tinObj)  # Link object to scene
        bpy.context.scene.objects.active = tinObj
        tinObj.select = True
        obj.select = False

        self.report({"INFO"},
                     "Mesh created (" + str(len(triangles)) + " triangles)")

        return {'FINISHED'}


class OBJECT_OT_VoronoiButton(Operator):
    bl_idname = "voronoi.tesselation"
    bl_label = "Diagram"
    bl_description = ("Points cloud Voronoi diagram in 2D\n"
                      "Needs an existing Active Mesh Object")
    bl_options = {"REGISTER", "UNDO"}

    meshType = EnumProperty(
            items=[('Edges', "Edges", "Edges Only - do not fill Faces"),
                   ('Faces', "Faces", "Fill Faces in the new Object")],
            name="Mesh type",
            description="Type of geometry to generate"
            )

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        return (obj is not None and obj.type == "MESH")

    def execute(self, context):
        # move the check into the poll
        obj = context.active_object

        # Get points coodinates
        r = obj.rotation_euler
        s = obj.scale
        mesh = obj.data
        vertsPts = [vertex.co for vertex in mesh.vertices]

        # Remove duplicate
        verts = [[vert.x, vert.y, vert.z] for vert in vertsPts]
        nDupli, nZcolinear = unique(verts)
        nVerts = len(verts)

        debug_prints(text=str(nDupli) + " duplicates points ignored")
        debug_prints(text=str(nZcolinear) + " z colinear points excluded")

        if nVerts < 3:
            self.report({"WARNING"},
                        "Not enough points to continue. Operation Cancelled")

            return {"CANCELLED"}

        # Check colinear
        xValues = [pt[0] for pt in verts]
        yValues = [pt[1] for pt in verts]

        if checkEqual(xValues) or checkEqual(yValues):
            self.report({"WARNING"},
                        "Points are colinear. Operation Cancelled")

            return {"CANCELLED"}

        # Create diagram
        debug_prints(text="Tesselation... (" + str(nVerts) + " points)")

        xbuff, ybuff = 5, 5
        zPosition = 0
        vertsPts = [Point(vert[0], vert[1], vert[2]) for vert in verts]

        if self.meshType == "Edges":
            pts, edgesIdx = computeVoronoiDiagram(
                                vertsPts, xbuff, ybuff,
                                polygonsOutput=False, formatOutput=True
                                )
        else:
            pts, polyIdx = computeVoronoiDiagram(
                                vertsPts, xbuff, ybuff, polygonsOutput=True,
                                formatOutput=True, closePoly=False
                                )

        pts = [[pt[0], pt[1], zPosition] for pt in pts]

        # Create new mesh structure
        voronoiDiagram = bpy.data.meshes.new("VoronoiDiagram")  # create a new mesh

        if self.meshType == "Edges":
            # Fill the mesh with triangles
            voronoiDiagram.from_pydata(pts, edgesIdx, [])
        else:
            # Fill the mesh with triangles
            voronoiDiagram.from_pydata(pts, [], list(polyIdx.values()))

        voronoiDiagram.update(calc_edges=True)  # Update mesh with new data
        # create an object with that mesh
        voronoiObj = bpy.data.objects.new("VoronoiDiagram", voronoiDiagram)
        # place object
        voronoiObj.location = obj.location.copy()
        voronoiObj.rotation_euler = r
        voronoiObj.scale = s

        # update scene
        bpy.context.scene.objects.link(voronoiObj)  # Link object to scene
        bpy.context.scene.objects.active = voronoiObj
        voronoiObj.select = True
        obj.select = False

        # Report
        if self.meshType == "Edges":
            self.report({"INFO"}, "Mesh created (" + str(len(edgesIdx)) + " edges)")
        else:
            self.report({"INFO"}, "Mesh created (" + str(len(polyIdx)) + " polygons)")

        return {'FINISHED'}


# Register
def register():
    bpy.utils.register_class(OBJECT_OT_VoronoiButton)
    bpy.utils.register_class(OBJECT_OT_TriangulateButton)
    bpy.utils.register_class(ToolsPanelDelaunay)


def unregister():
    bpy.utils.unregister_class(OBJECT_OT_VoronoiButton)
    bpy.utils.unregister_class(OBJECT_OT_TriangulateButton)
    bpy.utils.unregister_class(ToolsPanelDelaunay)


if __name__ == "__main__":
    register()
