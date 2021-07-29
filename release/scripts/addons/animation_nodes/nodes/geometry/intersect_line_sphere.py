import bpy
from bpy.props import *
from ... events import propertyChanged
from ... base_types import AnimationNode

class IntersectLineSphereNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_IntersectLineSphereNode"
    bl_label = "Intersect Line Sphere"
    bl_width_default = 160
    
    clip = BoolProperty(name = "Clip Inside Line", default = False, update = propertyChanged,
        description = "Only consider intersections inside the line, otherwise line is infinite")
        
    def create(self):
        self.newInput("Vector", "Line Start", "lineStart")
        self.newInput("Vector", "Line End", "lineEnd", value = (0, 0, 1))
        self.newInput("Vector", "Sphere Center", "center")
        self.newInput("Float", "Sphere Radius", "radius", value = 1)
        
        self.newOutput("Vector", "Intersection 1", "intersection1")
        self.newOutput("Vector", "Intersection 2", "intersection2")
        self.newOutput("Boolean", "Is Valid 1", "isValid1", hide = True)
        self.newOutput("Boolean", "Is Valid 2", "isValid2", hide = True)
        
    def draw(self, layout):
        layout.prop(self, "clip")
        
    def getExecutionCode(self):
        isLinked = self.getLinkedOutputsDict()
        if not any(isLinked.values()): return ""
    
        intersection1  = isLinked["intersection1"]
        intersection2  = isLinked["intersection2"]
        isValid1 = isLinked["isValid1"]
        isValid2 = isLinked["isValid2"]
        
        yield "if lineStart == lineEnd:"
        if intersection1 : yield "    intersection1 = center"
        if intersection2 : yield "    intersection2 = center"
        if isValid1: yield "    isValid1 = False"
        if isValid2: yield "    isValid2 = False"
        
        yield "else:"
        yield "    inters1, inters2 = mathutils.geometry.intersect_line_sphere(lineStart, lineEnd, center, radius, self.clip)"
        if intersection1 or isValid1:
            yield "    intersection1, isValid1 = (inters1, True) if inters1 != None else (center, False)"
        if intersection2 or isValid2:
            yield "    intersection2, isValid2 = (inters2, True) if inters2 != None else (center, False)"

    def getUsedModules(self):
        return ["mathutils"]