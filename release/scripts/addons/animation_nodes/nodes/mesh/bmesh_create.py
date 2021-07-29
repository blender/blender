import bpy
import bmesh
from bpy.props import *
from ... base_types import AnimationNode

class CreateBMeshFromMeshData(bpy.types.Node, AnimationNode):
    bl_idname = "an_CreateBMeshFromMeshDataNode"
    bl_label = "Create BMesh"

    errorMessage = StringProperty(default = "")

    def create(self):
        self.newInput("Mesh Data", "Mesh Data", "meshData")
        self.newOutput("BMesh", "BMesh", "bm")

    def draw(self, layout):
        if self.errorMessage != "":
            layout.label(self.errorMessage, icon = "ERROR")

    def execute(self, meshData):
        try:
            bm = getBMeshFromMeshData(meshData)
            self.errorMessage = ""
        except IndexError as e:
            bm = bmesh.new()
            self.errorMessage = "Missing vertices"
        except ValueError as e:
            bm = bmesh.new()
            self.errorMessage = "Multiple identical edges or polygons"
        return bm


def getBMeshFromMeshData(meshData):
    bm = bmesh.new()
    for co in meshData.vertices:
        bm.verts.new(co)

    # for Blender Version >= 2.73
    try: bm.verts.ensure_lookup_table()
    except: pass

    for edgeIndices in meshData.edges:
        bm.edges.new((bm.verts[edgeIndices[0]], bm.verts[edgeIndices[1]]))
    for polygonIndices in meshData.polygons:
        bm.faces.new(tuple(bm.verts[index] for index in polygonIndices))

    bm.normal_update()
    return bm
