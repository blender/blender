import bpy
import numpy
from bpy.props import *
from mathutils import Color
from ... events import propertyChanged
from ... base_types import AnimationNode

class SetVertexColorNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_SetVertexColorNode"
    bl_label = "Set Vertex Color"

    vertexColorName = StringProperty(name = "Vertex Color Group", default = "Col", update = propertyChanged)
    checkIfColorIsSet = BoolProperty(default = True)
    errorMessage = StringProperty()

    def create(self):
        self.newInput("Object", "Object", "object").defaultDrawType = "PROPERTY_ONLY"
        self.newInput("Color", "Color", "color").defaultDrawType = "PROPERTY_ONLY"
        self.newOutput("Object", "Object", "outObject")

    def draw(self, layout):
        layout.prop(self, "vertexColorName", text = "", icon = "GROUP_VCOL")
        if self.errorMessage != "":
            layout.label(self.errorMessage, icon = "ERROR")

    def drawAdvanced(self, layout):
        layout.prop(self, "checkIfColorIsSet", text = "Check Color")

    def execute(self, object, color):
        self.errorMessage = ""
        if object is None: return object
        if object.type != "MESH": return object
        if object.mode == "EDIT":
            self.errorMessage = "Object is in edit mode"
            return object

        mesh = object.data

        # Vertex Colors are internally stored with 8 bytes
        # I compress the color already here for an easier comparison
        newColor = tuple(min(max(int(x * 255) / 255, 0), 1) for x in color[:3])

        colorLayer = getVertexColorLayer(mesh, self.vertexColorName)
        if len(colorLayer.data) == 0: return object

        if self.checkIfColorIsSet:
            oldColor = colorLayer.data[0].color
            if colorsAreEqual(newColor, oldColor):
                return object

        newColorAsArray = numpy.array(newColor, dtype = "f")
        colors = numpy.tile(newColorAsArray, len(colorLayer.data))
        colorLayer.data.foreach_set("color", colors)

        # without this line the 3d view doesn't update
        colorLayer.data[0].color = newColor

        return object

def getVertexColorLayer(mesh, name):
    try: return mesh.vertex_colors[name]
    except: return mesh.vertex_colors.new(name)

def colorsAreEqual(a, b):
    return abs((a[0] * 100 + a[1] * 10 + a[2])
              -(b[0] * 100 + b[1] * 10 + b[2])) < 0.001
