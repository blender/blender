import bpy
from ... data_structures import VirtualVector3DList
from .. vector.offset_vector import offsetVector3DList
from ... base_types import AnimationNode, VectorizedSocket

class OffsetVerticesNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_OffsetVerticesNode"
    bl_label = "Offset Vertices"
    errorHandlingType = "EXCEPTION"

    useVectorList = VectorizedSocket.newProperty()

    def create(self):
        self.newInput("Mesh", "Mesh", "mesh")
        self.newInput("Falloff", "Falloff", "falloff")
        self.newInput(VectorizedSocket("Vector", "useVectorList",
            ("Offset", "offset", dict(value = (0, 0, 1))),
            ("Offsets", "offsets")))
        self.newOutput("Mesh", "Mesh", "mesh")

    def execute(self, mesh, falloff, offsets):
        _offsets = VirtualVector3DList.create(offsets, (0, 0, 0))
        offsetVector3DList(mesh.vertices, _offsets, self.getFalloffEvaluator(falloff))
        mesh.verticesTransformed()
        return mesh

    def getFalloffEvaluator(self, falloff):
        try: return falloff.getEvaluator("Location")
        except: self.raiseErrorMessage("This falloff cannot be evaluated for vectors")
