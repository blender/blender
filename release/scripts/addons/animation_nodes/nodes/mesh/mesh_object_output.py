import bpy
import bmesh
from bpy.props import *
from ... utils.layout import writeText
from ... base_types import AnimationNode
from ... data_structures import UShortList
from ... events import propertyChanged, executionCodeChanged

meshDataTypeItems = [
    ("MESH_DATA", "Mesh", "Mesh object that contains only vertex locations, edge indices and polygon indices", "", 0),
    ("BMESH", "BMesh", "BMesh object", "", 1),
    ("VERTICES", "Vertices", "A list of vertex locations; The length of this list has to be equal to the amount of vertices the mesh already has", "", 2) ]

class MeshObjectOutputNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_MeshObjectOutputNode"
    bl_label = "Mesh Object Output"
    bl_width_default = 180
    errorHandlingType = "MESSAGE"

    meshDataType = EnumProperty(name = "Mesh Type", default = "MESH_DATA",
        items = meshDataTypeItems, update = AnimationNode.refresh)

    validateMesh = BoolProperty(name = "Validate Mesh", default = False,
        description = "", update = propertyChanged)

    validateMeshVerbose = BoolProperty(name = "Validate Mesh Verbose", default = False,
        description = "Print results from validation in the console.", update = propertyChanged)

    ensureAnimationData = BoolProperty(name = "Ensure Animation Data", default = True,
        description = ("Make sure that the mesh has animation data so that "
                       "it will be exported as animation by exporters (mainly Alembic)"),
        update = propertyChanged)

    def create(self):
        socket = self.newInput("Object", "Object", "object")
        socket.defaultDrawType = "PROPERTY_ONLY"
        socket.objectCreationType = "MESH"

        if self.meshDataType == "MESH_DATA":
            self.newInput("Mesh", "Mesh", "meshData")
        elif self.meshDataType == "BMESH":
            self.newInput("BMesh", "BMesh", "bm")
        elif self.meshDataType == "VERTICES":
            self.newInput("Vector List", "Vertices", "vertices")

        self.newInput("Integer List", "Material Indices", "materialIndices")

        for socket in self.inputs[1:]:
            socket.useIsUsedProperty = True
            socket.isUsed = False

        self.newOutput("Object", "Object", "object")

    def draw(self, layout):
        if not self.meshInputSocket.hide:
            layout.prop(self, "meshDataType", text = "Type")

    @property
    def meshInputSocket(self):
        if self.meshDataType == "MESH_DATA": return self.inputs["Mesh"]
        if self.meshDataType == "BMESH": return self.inputs["BMesh"]
        if self.meshDataType == "VERTICES": return self.inputs["Vertices"]

    def drawAdvanced(self, layout):
        layout.prop(self, "ensureAnimationData")

        col = layout.column(align = True)
        col.prop(self, "validateMesh")
        subcol = col.column(align = True)
        subcol.active = self.validateMesh
        subcol.prop(self, "validateMeshVerbose", text = "Print Validation Info")

    def getExecutionCode(self, required):
        yield "if self.isValidObject(object):"
        yield "    mesh = object.data"

        s = self.inputs

        if self.meshDataType == "MESH_DATA":
            if s["Mesh"].isUsed:    yield "    self.setMesh(mesh, meshData)"
        elif self.meshDataType == "BMESH":
            if s["BMesh"].isUsed:        yield "    self.setBMesh(mesh, bm)"
        elif self.meshDataType == "VERTICES":
            if s["Vertices"].isUsed:     yield "    self.setVertices(mesh, vertices)"

        yield "    if self.ensureAnimationData:"
        yield "        self.ensureThatMeshHasAnimationData(mesh)"

        if s["Material Indices"].isUsed: yield "    self.setMaterialIndices(mesh, materialIndices)"

    def isValidObject(self, object):
        if object is None: return False
        if object.type != "MESH" or object.mode != "OBJECT":
            self.setErrorMessage("Object is not in object mode or is no mesh object")
            return False
        return True

    def setMesh(self, outMesh, mesh):
        # clear existing mesh
        bmesh.new().to_mesh(outMesh)

        # allocate memory
        outMesh.vertices.add(len(mesh.vertices))
        outMesh.edges.add(len(mesh.edges))
        outMesh.loops.add(len(mesh.polygons.indices))
        outMesh.polygons.add(len(mesh.polygons))

        # Vertices
        outMesh.vertices.foreach_set("co", mesh.vertices.asMemoryView())
        outMesh.vertices.foreach_set("normal", mesh.getVertexNormals().asMemoryView())

        # Edges
        outMesh.edges.foreach_set("vertices", mesh.edges.asMemoryView())

        # Polygons
        outMesh.polygons.foreach_set("loop_total", mesh.polygons.polyLengths.asMemoryView())
        outMesh.polygons.foreach_set("loop_start", mesh.polygons.polyStarts.asMemoryView())
        outMesh.loops.foreach_set("vertex_index", mesh.polygons.indices.asMemoryView())
        outMesh.loops.foreach_set("edge_index", mesh.getLoopEdges().asMemoryView())

        # UV Maps
        for name, data in mesh.getUVMaps():
            outMesh.uv_textures.new(name)
            outMesh.uv_layers[name].data.foreach_set("uv", data.asMemoryView())

        if self.validateMesh:
            outMesh.validate(verbose = self.validateMeshVerbose)

    def setBMesh(self, mesh, bm):
        bm.to_mesh(mesh)

    def setVertices(self, mesh, vertices):
        if len(mesh.vertices) != len(vertices):
            self.setErrorMessage("The vertex amounts are not equal")
            return object

        mesh.vertices.foreach_set("co", vertices.asMemoryView())
        mesh.update()

    def setMaterialIndices(self, mesh, materialIndices):
        if len(materialIndices) == 0: return
        if len(mesh.polygons) == 0: return
        if materialIndices.containsValueLowerThan(0):
            self.setErrorMessage("Material indices have to be greater or equal to zero")
            return

        allMaterialIndices = UShortList.fromValues(materialIndices)
        if len(materialIndices) != len(mesh.polygons):
            allMaterialIndices = allMaterialIndices.repeated(length = len(mesh.polygons))

        mesh.polygons.foreach_set("material_index", allMaterialIndices.asMemoryView())
        mesh.polygons[0].material_index = materialIndices[0]

    def ensureThatMeshHasAnimationData(self, mesh):
        if mesh.animation_data is None:
            mesh.animation_data_create()
