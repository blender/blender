import bpy
from ... base_types import AnimationNode

class IntersectPlanePlaneNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_IntersectPlanePlaneNode"
    bl_label = "Intersect Plane Plane"
    bl_width_default = 160

    def create(self):
        self.newInput("Vector", "Plane 1 Point", "point1")
        self.newInput("Vector", "Plane 1 Normal", "normal1", value = (1, 0, 0))
        self.newInput("Vector", "Plane 2 Point", "point2")
        self.newInput("Vector", "Plane 2 Normal", "normal2", value = (0, 0, 1))

        self.newOutput("Vector", "Intersection Point", "intersection")
        self.newOutput("Vector", "Direction Vector", "direction")
        self.newOutput("Float", "Angle", "angle")
        self.newOutput("Boolean", "Is Valid", "isValid")

    def getExecutionCode(self):
        isLinked = self.getLinkedOutputsDict()
        if not any(isLinked.values()): return

        intersection  = isLinked["intersection"]
        direction  = isLinked["direction"]
        angle  = isLinked["angle"]
        isValid = isLinked["isValid"]

        yield "if normal1[:] == (0,0,0): normal1 = Vector((0, 0, 1))"
        yield "if normal2[:] == (0,0,0): normal2 = Vector((0, 0, 1))"

        if any([intersection, direction, isValid]):
            yield "intersections = mathutils.geometry.intersect_plane_plane(point1, normal1, point2, normal2)"

            yield "if intersections != (None, None):"
            if intersection: yield "    intersection = intersections[0]"
            if direction:    yield "    direction = intersections[1]"
            if isValid:      yield "    isValid = True"
            yield "else: "
            if intersection: yield "    intersection = Vector((0,0,0))"
            if direction:    yield "    direction = Vector((0,0,0))"
            if isValid:      yield "    isValid = False"

        if angle: yield "angle = math.pi - (normal1.angle(normal2, math.pi))"

    def getUsedModules(self):
        return ["mathutils, math"]
