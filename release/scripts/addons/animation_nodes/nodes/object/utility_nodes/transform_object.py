import bpy
from bpy.props import *
from mathutils import Matrix
from .... events import propertyChanged
from .... base_types import AnimationNode

class TransformObjectNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_TransformObjectNode"
    bl_label = "Transform Object"

    useCenter = BoolProperty(name = "Use Center", default = True,
        description = "Use the object location as origin", update = propertyChanged)

    def create(self):
        self.newInput("Object", "Object", "object").defaultDrawType = "PROPERTY_ONLY"
        self.newInput("Matrix", "Matrix", "matrix")
        self.newOutput("Object", "Object", "object")

    def draw(self, layout):
        layout.prop(self, "useCenter")

    def execute(self, object, matrix):
        if object is None: return None
        if self.useCenter:
            offset = Matrix.Translation(object.location)
            transformation = offset * matrix * offset.inverted()
        else:
            transformation = matrix
        object.matrix_world = transformation * object.matrix_world
        return object
