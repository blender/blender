import bpy
from bpy.props import *
from . c_utils import tiltSplinePoints
from ... events import propertyChanged
from ... base_types import AnimationNode, VectorizedSocket
from ... data_structures import VirtualFloatList, FloatList

class TiltSplineNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_TiltSplineNode"
    bl_label = "Tilt Spline"

    useTiltList = VectorizedSocket.newProperty()
    accumulateTilts = BoolProperty(name = "Accumulate Tilts", default = False,
        update = propertyChanged)

    def create(self):
        self.newInput("Spline", "Spline", "spline", defaultDrawType = "PROPERTY_ONLY")
        self.newInput(VectorizedSocket("Float", "useTiltList",
            ("Tilt", "tilt"), ("Tilts", "tilts")))
        self.newOutput("Spline", "Spline", "spline")

    def draw(self, layout):
        layout.prop(self, "accumulateTilts", text = "Accumulate")

    def execute(self, spline, tilts):
        if self.useTiltList:
            _tilts = VirtualFloatList.fromList(FloatList.fromValues(tilts), 0)
        else:
            _tilts = VirtualFloatList.fromElement(tilts)
        tiltSplinePoints(spline, _tilts, self.accumulateTilts)
        return spline