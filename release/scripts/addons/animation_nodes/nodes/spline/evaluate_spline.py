import bpy
from bpy.props import *
from mathutils import Vector
from ... base_types import AnimationNode, VectorizedSocket
from . spline_evaluation_base import SplineEvaluationBase

class EvaluateSplineNode(bpy.types.Node, AnimationNode, SplineEvaluationBase):
    bl_idname = "an_EvaluateSplineNode"
    bl_label = "Evaluate Spline"

    evaluateRange = BoolProperty(name = "Evaluate Range", default = False,
        description = "Evaluate automatically distributed parameters on the spline",
        update = AnimationNode.refresh)

    useParameterList = VectorizedSocket.newProperty()

    def create(self):
        self.newInput("Spline", "Spline", "spline", defaultDrawType = "PROPERTY_ONLY")
        if self.evaluateRange:
            self.newInput("Integer", "Amount", "amount", value = 50)
            self.newInput("Float", "Start", "start", value = 0.0).setRange(0.0, 1.0)
            self.newInput("Float", "End", "end", value = 1.0).setRange(0.0, 1.0)
            self.newOutput("Vector List", "Locations", "locations")
            self.newOutput("Vector List", "Tangents", "tangents")
            self.newOutput("Vector List", "Normals", "normals")
            self.newOutput("Float List", "Radii", "radii")
            self.newOutput("Float List", "Tilts", "tilts", hide = True)
        else:
            self.newInput(VectorizedSocket("Float", "useParameterList",
                ("Parameter", "parameter", dict(minValue = 0, maxValue = 1)),
                ("Parameters", "parameters")))

            self.newOutput(VectorizedSocket("Vector", "useParameterList",
                ("Location", "location"),
                ("Locations", "locations")))
            self.newOutput(VectorizedSocket("Vector", "useParameterList",
                ("Tangent", "tangent"),
                ("Tangents", "tangents")))
            self.newOutput(VectorizedSocket("Vector", "useParameterList",
                ("Normal", "normal"),
                ("Normals", "normals")))
            self.newOutput(VectorizedSocket("Float", "useParameterList",
                ("Radius", "radius"),
                ("Radii", "radii")))
            self.newOutput(VectorizedSocket("Float", "useParameterList",
                ("Tilt", "tilt", dict(hide = True)),
                ("Tilts", "tilts", dict(hide = True))))

    def draw(self, layout):
        row = layout.row(align = True)
        row.prop(self, "parameterType", text = "")
        row.prop(self, "evaluateRange", text = "", icon = "ALIGN")

    def drawAdvanced(self, layout):
        col = layout.column()
        col.active = self.parameterType == "UNIFORM"
        col.prop(self, "resolution")

    def getExecutionCode(self, required):
        yield "if spline.isEvaluable():"
        if self.parameterType == "UNIFORM":
            yield "    spline.ensureUniformConverter(self.resolution)"
        if "normal" in required or "normals" in required:
            yield "    spline.ensureNormals()"

        if self.evaluateRange:
            yield from ("    " + c for c in self.getExecutionCode_Range(required))
        else:
            yield from ("    " + c for c in self.getExecutionCode_Parameters(required))

        yield "else:"
        for s in self.outputs:
            yield "    {} = self.outputs['{}'].getDefaultValue()".format(s.identifier, s.name)

    def getExecutionCode_Range(self, required):
        yield "_amount = max(amount, 0)"
        yield "_start = min(max(start, 0), 1)"
        yield "_end = min(max(end, 0), 1)"

        if "locations" in required:
            yield "locations = spline.getDistributedPoints(_amount, _start, _end, self.parameterType)"
        if "tangents" in required:
            yield "tangents = spline.getDistributedTangents(_amount, _start, _end, self.parameterType)"
        if "normals" in required:
            yield "normals = spline.getDistributedNormals(_amount, _start, _end, self.parameterType)"
        if "radii" in required:
            yield "_radii = spline.getDistributedRadii(_amount, _start, _end, self.parameterType)"
            yield "radii = DoubleList.fromValues(_radii)"
        if "tilts" in required:
            yield "_tilts = spline.getDistributedTilts(_amount, _start, _end, self.parameterType)"
            yield "tilts = DoubleList.fromValues(_tilts)"

    def getExecutionCode_Parameters(self, required):
        if self.useParameterList:
            yield from self.getExecutionCode_Parameters_List(required)
        else:
            yield from self.getExecutionCode_Parameters_Single(required)

    def getExecutionCode_Parameters_List(self, required):
        yield "_parameters = FloatList.fromValues(parameters)"
        yield "_parameters.clamp(0, 1)"

        if self.parameterType == "UNIFORM":
            yield "_parameters = spline.toUniformParameters(_parameters)"

        if "locations" in required:
            yield "locations = spline.samplePoints(_parameters, False, 'RESOLUTION')"
        if "tangents" in required:
            yield "tangents = spline.sampleTangents(_parameters, False, 'RESOLUTION')"
        if "normals" in required:
            yield "normals = spline.sampleNormals(_parameters, False, 'RESOLUTION')"
        if "radii" in required:
            yield "_radii = spline.sampleRadii(_parameters, False, 'RESOLUTION')"
            yield "radii = DoubleList.fromValues(_radii)"
        if "tilts" in required:
            yield "_tilts = spline.sampleTilts(_parameters, False, 'RESOLUTION')"
            yield "tilts = DoubleList.fromValues(_tilts)"

    def getExecutionCode_Parameters_Single(self, required):
        yield "_parameter = min(max(parameter, 0), 1)"

        if self.parameterType == "UNIFORM":
            yield "_parameter = spline.toUniformParameter(_parameter)"

        if "location" in required:
            yield "location = spline.evaluatePoint(_parameter)"
        if "tangent" in required:
            yield "tangent = spline.evaluateTangent(_parameter)"
        if "normal" in required:
            yield "normal = spline.evaluateNormal(_parameter)"
        if "radius" in required:
            yield "radius = spline.evaluateRadius(_parameter)"
        if "tilt" in required:
            yield "tilt = spline.evaluateTilt(_parameter)"
