import bpy
from bpy.props import *
from ... events import propertyChanged
from ... base_types import AnimationNode, VectorizedSocket

directionAxisItems = [(axis, axis, "", "", i)
                      for i, axis in enumerate(("X", "Y", "Z", "-X", "-Y", "-Z"))]

class RotationToDirectionNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_RotationToDirectionNode"
    bl_label = "Rotation to Direction"
    bl_width_default = 160

    useRotationList = VectorizedSocket.newProperty()

    directionAxis = EnumProperty(name = "Direction Axis", default = "Z",
        items = directionAxisItems, update = propertyChanged)

    def create(self):
        self.newInput(VectorizedSocket("Euler", "useRotationList",
            ("Rotation", "rotation"), ("Rotations", "rotations")))

        self.newInput("Float", "Length", "length", value = 1)

        self.newOutput(VectorizedSocket("Vector", "useRotationList",
            ("Direction", "direction"), ("Directions", "directions")))

    def draw(self, layout):
        layout.prop(self, "directionAxis", expand = True)

    def getExecutionCode(self, required):
        if self.useRotationList:
            yield "directions = AN.algorithms.rotations.eulersToDirections(rotations, self.directionAxis)"
            yield "AN.math.scaleVector3DList(directions, length)"
        else:
            yield "direction = AN.algorithms.rotations.eulerToDirection(rotation, self.directionAxis)"
            yield "direction *= length"
