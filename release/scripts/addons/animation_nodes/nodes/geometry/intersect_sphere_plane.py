import bpy
from ... base_types import AnimationNode

class IntersectSpherePlaneNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_IntersectSpherePlaneNode"
    bl_label = "Intersect Sphere Plane"
    bl_width_default = 160

    def create(self):
        self.newInput("Vector", "Sphere Center", "sphereCenter")
        self.newInput("Float", "Sphere Radius", "sphereRadius", value = 1)
        self.newInput("Vector", "Plane Point", "point")
        self.newInput("Vector", "Plane Normal", "normal", value = (0, 0, 1))

        self.newOutput("Vector", "Circle Center", "center")
        self.newOutput("Float", "Circle Radius", "radius")
        self.newOutput("Boolean", "Is Valid", "isValid", hide = True)

    def getExecutionCode(self):
        isLinked = self.getLinkedOutputsDict()
        if not any(isLinked.values()): return ""

        center  = isLinked["center"]
        radius  = isLinked["radius"]
        isValid = isLinked["isValid"]

        yield "dist = mathutils.geometry.distance_point_to_plane(sphereCenter, point, normal)"

        yield "if abs(dist) <= abs(sphereRadius) and sphereRadius != 0:"
        if center:  yield "    center = (normal * -dist) + sphereCenter"
        if radius:  yield "    radius = math.sqrt((sphereRadius**2) - (dist**2))"
        if isValid: yield "    isValid = True"
        yield "else:"
        if center:  yield "    center = Vector((0,0,0))"
        if radius:  yield "    radius = 0"
        if isValid: yield "    isValid = False"

    def getUsedModules(self):
        return ["mathutils", "math"]
