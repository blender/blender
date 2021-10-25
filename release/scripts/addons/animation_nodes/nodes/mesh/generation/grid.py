import bpy
from bpy.props import *
from .... base_types import AnimationNode
from .... algorithms.mesh_generation.grid import getGridMesh_Step, getGridMesh_Size

modeItems = [
    ("STEP", "Step", "Define the distance between the vertices", 0),
    ("SIZE", "Size", "Define how large the grid will be in total", 1)
]

class GridMeshNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_GridMeshNode"
    bl_label = "Grid Mesh"
    bl_width_default = 160

    mode = EnumProperty(name = "Mode", default = "SIZE",
        update = AnimationNode.refresh, items = modeItems)

    def create(self):
        self.newInput("Integer", "X Divisions", "xDivisions", value = 10, minValue = 2)
        self.newInput("Integer", "Y Divisions", "yDivisions", value = 10, minValue = 2)

        if self.mode == "STEP":
            self.newInput("Float", "X Distance", "xDistance", value = 1)
            self.newInput("Float", "Y Distance", "yDistance", value = 1)
        elif self.mode == "SIZE":
            self.newInput("Float", "Length", "length", value = 10)
            self.newInput("Float", "Width", "width", value = 10)

        self.newOutput("Mesh", "Mesh", "mesh")

    def draw(self, layout):
        layout.prop(self, "mode")

    def getExecutionFunctionName(self):
        if self.mode == "STEP":
            return "execute_Step"
        elif self.mode == "SIZE":
            return "execute_Size"

    def execute_Step(self, xDivisions, yDivisions, xDistance, yDistance):
        xDivisions = max(xDivisions, 2)
        yDivisions = max(yDivisions, 2)
        return getGridMesh_Step(xDistance, yDistance, xDivisions, yDivisions)

    def execute_Size(self, xDivisions, yDivisions, length, width):
        xDivisions = max(xDivisions, 2)
        yDivisions = max(yDivisions, 2)
        return getGridMesh_Size(length, width, xDivisions, yDivisions)