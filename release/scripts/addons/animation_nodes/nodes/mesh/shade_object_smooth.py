import bpy
from ... base_types import VectorizedNode

class ShadeObjectSmooth(bpy.types.Node, VectorizedNode):
    bl_idname = "an_ShadeObjectSmoothNode"
    bl_label = "Shade Object Smooth"
    autoVectorizeExecution = True

    useObjectList = VectorizedNode.newVectorizeProperty()
    useSmoothList = VectorizedNode.newVectorizeProperty()

    def create(self):
        self.newVectorizedInput("Object", "useObjectList",
            ("Object", "object", dict(defaultDrawType = "PROPERTY_ONLY")),
            ("Objects", "objects"))

        self.newVectorizedInput("Boolean", "useSmoothList",
            ("Smooth", "smooth"), ("Smooth", "smooth"))

        self.newVectorizedOutput("Object", "useObjectList",
            ("Object", "object"), ("Objects", "objects"))

    def getExecutionCode(self):
        return "object = self.execute_Single(object, smooth)"

    def execute_Single(self, object, smooth):
        if getattr(object, "type", "") == "MESH":
            mesh = object.data
            if len(mesh.polygons) > 0:
                smoothList = [smooth] * len(mesh.polygons)
                mesh.polygons.foreach_set("use_smooth", smoothList)

                # trigger update
                mesh.polygons[0].use_smooth = smooth
        return object
