import bpy
from bpy.props import *
from ... events import propertyChanged
from ... base_types import AnimationNode

class BMeshTriangulateNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_BMeshTriangulateNode"
    bl_label = "Triangulate BMesh"
    bl_width_default = 160

    quad = IntProperty(name = "Quad Method", max = 3, min = 0,
        description = "Select a quad triangulation method.",
        update = propertyChanged)

    ngon = IntProperty(name = "Ngon Method", max = 1, min = 0,
        description = "Select a ngon triangulation method.",
        update = propertyChanged)

    def create(self):
        self.newInput("BMesh", "BMesh", "bm", dataIsModified = True)
        self.newOutput("BMesh", "BMesh", "bm")
        self.newOutput("Integer List", "Source Ngon Indices", "ngonIndices", hide = True)

    def drawAdvanced(self, layout):
        layout.prop(self, "quad")
        layout.prop(self, "ngon")

    def getExecutionCode(self):
        isLinked = self.getLinkedOutputsDict()
        if not any(isLinked.values()): return

        yield "faces = bm.faces"

        # do not invert isLinked order, this needs original faces
        if isLinked["ngonIndices"]:
            yield "ngonIndices = list(range(len(faces)))"
            yield "for face in faces:"
            yield "    indf = face.index"
            yield "    lenf = len(face.verts) - 3"
            yield "    if lenf > 0: ngonIndices.extend(indf for i in range(lenf))"

        if isLinked["bm"]:
            yield "bmesh.ops.triangulate(bm, faces = faces,"
            yield "    quad_method = self.quad, ngon_method = self.ngon)"

    def getUsedModules(self):
        return ["bmesh"]
