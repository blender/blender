import bpy
from bpy.props import *
from ... base_types import AnimationNode

class ProjectOnSplineNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_ProjectOnSplineNode"
    bl_label = "Project on Spline"

    extended = BoolProperty(
        name = "Extended Spline",
        description = "Project point on extended spline. If this is turned on the parameter is not computable.",
        update = AnimationNode.refresh)

    def create(self):
        self.newInput("Spline", "Spline", "spline", defaultDrawType = "PROPERTY_ONLY")
        self.newInput("Vector", "Location", "location")

        self.newOutput("Vector", "Position", "position")
        self.newOutput("Vector", "Tangent", "tangent")
        self.newOutput("Float", "Distance", "distance")
        if not self.extended:
            self.newOutput("Float", "Parameter", "parameter")

    def draw(self, layout):
        layout.prop(self, "extended", text = "Extended")

    def getExecutionCode(self):
        yield "if spline.isEvaluable():"
        if self.extended:
            yield "    position, tangent = spline.projectExtended(location)"
        else:
            yield "    parameter = spline.project(location)"
            yield "    position = spline.evaluate(parameter)"
            yield "    tangent = spline.evaluateTangent(parameter)"
        yield "    distance = (position - location).length"
        yield "else:"
        yield "    position = Vector((0, 0, 0))"
        yield "    tangent = Vector((0, 0, 0))"
        yield "    parameter = 0.0"
        yield "    distance = 0.0"
