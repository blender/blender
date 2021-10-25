import bpy
from bpy.props import *
from .... events import executionCodeChanged
from .... base_types import AnimationNode, VectorizedSocket
from .... data_structures import VirtualDoubleList, VirtualLongList
from .... algorithms.mesh_generation.circle import getCircleMesh, getCircleMeshList

class CircleMeshNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_CircleMeshNode"
    bl_label = "Circle Mesh"
    bl_width_default = 160

    def checkedPropertiesChanged(self, context):
        self.updateSocketVisibility()
        executionCodeChanged()

    mergeStartEnd = BoolProperty(name = "Merge Start End", default = True,
        description = "Merge the start and end of the circle.",
        update = checkedPropertiesChanged)

    mergeCenter = BoolProperty(name = "Merge Center", default = True,
        description = "Merge the center of the circle using a triangle fan.",
        update = checkedPropertiesChanged)

    useRadialLoopsList = VectorizedSocket.newProperty()
    useInnerLoopsList = VectorizedSocket.newProperty()
    useOuterRadiusList = VectorizedSocket.newProperty()
    useInnerRadiusList = VectorizedSocket.newProperty()
    useStartAngleList = VectorizedSocket.newProperty()
    useEndAngleList = VectorizedSocket.newProperty()

    def draw(self, layout):
        col = layout.column()
        subcol = col.column(align = True)
        subcol.prop(self, "mergeStartEnd", text = "Merge Start End", icon = "PROP_CON")
        subcol.prop(self, "mergeCenter", text = "Merge Center", icon = "AUTOMERGE_ON")

    def create(self):
        self.newInput(VectorizedSocket("Integer", "useRadialLoopsList",
            ("Radial Loops", "radialLoops", dict(value = 10, minValue = 3)),
            ("Radial Loops", "radialLoops"),
            codeProperties = dict(default = 10)))
        self.newInput(VectorizedSocket("Integer", "useInnerLoopsList",
            ("Inner Loops", "innerLoops", dict(value = 0, minValue = 0)),
            ("Inner Loops", "innerLoops"),
            codeProperties = dict(default = 0)))

        self.newInput(VectorizedSocket("Float", "useOuterRadiusList",
            ("Outer Radius", "outerRadius", dict(value = 1, minValue = 0)),
            ("Outer Radii", "outerRadii"),
            codeProperties = dict(default = 1)))
        self.newInput(VectorizedSocket("Float", "useInnerRadiusList",
            ("Inner Radius", "innerRadii", dict(value = 0.5, minValue = 0)),
            ("Inner Radii", "innerRadii"),
            codeProperties = dict(default = 0.5)))

        self.newInput(VectorizedSocket("Float", "useStartAngleList",
            ("Start Angle", "startAngle", dict(value = 0)),
            ("Start Angles", "startAngles"),
            codeProperties = dict(default = 0)))
        self.newInput(VectorizedSocket("Float", "useEndAngleList",
            ("End Angle", "endAngle", dict(value = 5)),
            ("End Angles", "endAngles"),
            codeProperties = dict(default = 5)))

        props = ["useRadialLoopsList", "useInnerLoopsList",
                 "useOuterRadiusList", "useInnerRadiusList",
                 "useStartAngleList", "useEndAngleList"]

        self.newOutput(VectorizedSocket("Mesh", props,
            ("Mesh", "Mesh"), ("Meshes", "Meshes")))

        self.updateSocketVisibility()

    def updateSocketVisibility(self):
        self.inputs[3].hide = self.mergeCenter
        self.inputs[4].hide = self.mergeStartEnd
        self.inputs[5].hide = self.mergeStartEnd

    def getExecutionFunctionName(self):
        useList = any((self.useRadialLoopsList, self.useInnerLoopsList,
                       self.useOuterRadiusList, self.useInnerRadiusList,
                       self.useStartAngleList, self.useEndAngleList))
        if useList:
            return "execute_List"
        else:
            return "execute_Single"

    def execute_List(self, radialLoops, innerLoops, outerRadii, innerRadii,
                           startAngles, endAngles):
        radialLoops, innerLoops = VirtualLongList.createMultiple(
            (radialLoops, 10), (innerLoops, 0))
        outerRadii, innerRadii = VirtualDoubleList.createMultiple(
            (outerRadii, 1), (innerRadii, 0.5))
        startAngles, endAngles = VirtualDoubleList.createMultiple(
            (startAngles, 0), (endAngles, 5))

        amount = VirtualLongList.getMaxRealLength(radialLoops, innerLoops,
            outerRadii, innerRadii, startAngles, endAngles)

        return getCircleMeshList(amount, radialLoops, innerLoops,
            outerRadii, innerRadii, startAngles, endAngles,
            self.mergeStartEnd, self.mergeCenter)

    def execute_Single(self, radialLoops, innerLoops, outerRadius, innerRadius,
                             startAngle, endAngle):
        return getCircleMesh(radialLoops, innerLoops, outerRadius, innerRadius,
            startAngle, endAngle, self.mergeStartEnd, self.mergeCenter)
