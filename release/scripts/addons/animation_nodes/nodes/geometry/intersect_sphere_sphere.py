import bpy
from ... base_types import AnimationNode

class IntersectSphereSphereNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_IntersectSphereSphereNode"
    bl_label = "Intersect Sphere Sphere"
    bl_width_default = 160

    def create(self):
        self.newInput("Vector", "Sphere 1 Center", "center1")
        self.newInput("Float", "Sphere 1 Radius", "radius1", value = 1)
        self.newInput("Vector", "Sphere 2 Center", "center2", value = (0, 0, 1))
        self.newInput("Float", "Sphere 2 Radius", "radius2", value = 1)

        self.newOutput("Vector", "Circle Center", "center")
        self.newOutput("Vector", "Circle Normal", "normal")
        self.newOutput("Float", "Circle Radius", "radius")
        self.newOutput("Boolean", "Is Valid", "isValid", hide = True)

    def getExecutionCode(self):
        isLinked = self.getLinkedOutputsDict()
        if not any(isLinked.values()): return ""

        center  = isLinked["center"]
        normal  = isLinked["normal"]
        radius  = isLinked["radius"]
        isValid = isLinked["isValid"]

        yield "if center1 == center2: "
        if center : yield "    center = Vector((0,0,0))"
        if normal : yield "    normal = Vector((0,0,0))"
        if radius : yield "    radius = 0"
        if isValid: yield "    isValid = False"

        yield "else:"
        yield "    dif = (center2 - center1)"
        yield "    dist = dif.length"
        yield "    _, intx = mathutils.geometry.intersect_sphere_sphere_2d( (0, 0), radius1, (dist, 0), radius2)"

        yield "    if intx is not None:"
        if center : yield "        center = center1.lerp(center2, intx[0]/dist)"
        if normal : yield "        normal = dif.normalized()"
        if radius : yield "        radius = intx[1]"
        if isValid: yield "        isValid = True"
        yield "    else:"
        if center : yield "        center = Vector((0,0,0))"
        if normal : yield "        normal = Vector((0,0,0))"
        if radius : yield "        radius = 0"
        if isValid: yield "        isValid = False"

    def getUsedModules(self):
        return ["mathutils"]
