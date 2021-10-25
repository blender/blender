import bpy
from bpy.props import *
from mathutils import Matrix
from . c_utils import replicateMesh
from ... data_structures import VirtualPyList, Mesh
from ... base_types import AnimationNode, VectorizedSocket

transformationTypeItems = [
    ("MATRIX", "Matrix", "", "NONE", 0),
    ("VECTOR", "Vector", "", "NONE", 1)
]

class TransformMeshNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_TransformMeshNode"
    bl_label = "Transform Mesh"

    transformationType = EnumProperty(name = "Transformation Type", default = "MATRIX",
        items = transformationTypeItems, update = AnimationNode.refresh)

    joinMeshes = BoolProperty(name = "Join Meshes", default = True,
        update = AnimationNode.refresh)

    useMeshList = VectorizedSocket.newProperty()
    useTransformationList = VectorizedSocket.newProperty()

    def create(self):
        self.newInput(VectorizedSocket("Mesh", "useMeshList",
            ("Mesh", "inMesh"), ("Meshes", "inMeshes")))
        self.inputs[0].dataIsModified = self.inputMeshesAreModified

        if self.transformationType == "VECTOR":
            self.newInput(VectorizedSocket("Vector", "useTransformationList",
                ("Translation", "translation"), ("Translations", "translations")))
        elif self.transformationType == "MATRIX":
            self.newInput(VectorizedSocket("Matrix", "useTransformationList",
                ("Transformation", "transformation"), ("Transformations", "transformations")))

        if self.hasListInput and not self.joinMeshes:
            self.newOutput("Mesh List", "Meshes", "outMeshes")
        else:
            self.newOutput("Mesh", "Mesh", "outMesh")

    def draw(self, layout):
        layout.prop(self, "transformationType", text = "")
        if self.hasListInput:
            layout.prop(self, "joinMeshes")

    def getExecutionFunctionName(self):
        if self.transformationType == "MATRIX":
            if self.useMeshList:
                if self.useTransformationList:
                    return "execute_MultipleMeshes_MultipleMatrices"
                else:
                    return "execute_MultipleMeshes_SingleMatrix"
            else:
                if self.useTransformationList:
                    if self.joinMeshes:
                        return "execute_SingleMesh_MultipleMatrices_Joined"
                    else:
                        return "execute_SingleMesh_MultipleMatrices_Separated"
                else:
                    return "execute_Single_Matrix"
        else:
            if self.useMeshList:
                if self.useTransformationList:
                    return "execute_MultipleMeshes_MultipleVectors"
                else:
                    return "execute_MultipleMeshes_SingleVector"
            else:
                if self.useTransformationList:
                    if self.joinMeshes:
                        return "execute_SingleMesh_MultipleVectors_Joined"
                    else:
                        return "execute_SingleMesh_MultipleVectors_Separated"
                else:
                    return "execute_Single_Vector"

    def execute_Single_Vector(self, mesh, vector):
        mesh.move(vector)
        return mesh

    def execute_Single_Matrix(self, mesh, matrix):
        mesh.transform(matrix)
        return mesh

    def execute_SingleMesh_MultipleVectors_Joined(self, mesh, vectors):
        return replicateMesh(mesh, vectors)

    def execute_SingleMesh_MultipleMatrices_Joined(self, mesh, matrices):
        return replicateMesh(mesh, matrices)

    def execute_SingleMesh_MultipleVectors_Separated(self, mesh, vectors):
        outMeshes = []
        for vector in vectors:
            m = mesh.copy()
            m.move(vector)
            outMeshes.append(m)
        return outMeshes

    def execute_SingleMesh_MultipleMatrices_Separated(self, mesh, matrices):
        outMeshes = []
        for matrix in matrices:
            m = mesh.copy()
            m.transform(matrix)
            outMeshes.append(m)
        return outMeshes

    def execute_MultipleMeshes_SingleVector(self, meshes, vector):
        for mesh in meshes:
            mesh.move(vector)
        return self.joinWhenNecessary(meshes)

    def execute_MultipleMeshes_SingleMatrix(self, meshes, matrix):
        for mesh in meshes:
            mesh.transform(matrix)
        return self.joinWhenNecessary(meshes)

    def execute_MultipleMeshes_MultipleVectors(self, meshes, vectors):
        _meshes = VirtualPyList.create(meshes, Mesh())
        _vectors = VirtualPyList.create(list(vectors), (0, 0, 0))
        amount = VirtualPyList.getMaxRealLength(_meshes, _vectors)

        outMeshes = []
        for i in range(amount):
            m = _meshes[i].copy()
            m.move(_vectors[i])
            outMeshes.append(m)

        return self.joinWhenNecessary(outMeshes)

    def execute_MultipleMeshes_MultipleMatrices(self, meshes, matrices):
        _meshes = VirtualPyList.create(meshes, Mesh())
        _matrices = VirtualPyList.create(list(matrices), Matrix())
        amount = VirtualPyList.getMaxRealLength(_meshes, _matrices)

        outMeshes = []
        for i in range(amount):
            m = _meshes[i].copy()
            m.transform(_matrices[i])
            outMeshes.append(m)

        return self.joinWhenNecessary(outMeshes)

    def joinWhenNecessary(self, meshes):
        if self.joinMeshes:
            return Mesh.join(*meshes)
        else:
            return meshes

    @property
    def hasListInput(self):
        return self.useMeshList or self.useTransformationList

    @property
    def inputMeshesAreModified(self):
        return (not self.hasListInput or
               (self.useMeshList and not self.useTransformationList and not self.joinMeshes))