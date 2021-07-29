import bpy
from bpy.props import *
from math import isclose
from ... events import propertyChanged
from ... base_types import AnimationNode

class ViewportColorNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_ViewportColorNode"
    bl_label = "Viewport Color"

    materialName = StringProperty(update = propertyChanged)

    def create(self):
        self.newInput("Color", "Color", "color")

    def draw(self, layout):
        layout.prop_search(self, "materialName", bpy.data, "materials", text = "", icon = "MATERIAL_DATA")

    def execute(self, color):
        material = bpy.data.materials.get(self.materialName)
        if material is None: return

        newColor = color[:3]
        oldColor = list(material.diffuse_color)
        if not all(isclose(a, b) for a, b in zip(oldColor, newColor)):
            material.diffuse_color = newColor
