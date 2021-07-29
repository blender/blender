import bpy
import bmesh
from ... events import isRendering
from ... base_types import AnimationNode

class BMeshFromObjectNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_BMeshFromObjectNode"
    bl_label = "BMesh from Object"

    def create(self):
        self.newInput("Object", "Object", "object", defaultDrawType = "PROPERTY_ONLY")
        self.newInput("Boolean", "Use World Space", "useWorldSpace", value = True)
        self.newInput("Boolean", "Use Modifiers", "useModifiers", value = False)
        self.newInput("Scene", "Scene", "scene", hide = True)
        self.newOutput("BMesh", "BMesh", "bm")

    def execute(self, object, useWorldSpace, useModifiers, scene):
        bm = bmesh.new()
        if getattr(object, "type", "") != "MESH" or scene is None: return bm
        # Seems like the deform and render parameters don't work yet..
        bm.from_object(object, scene, deform = useModifiers, render = isRendering())
        if useWorldSpace: bm.transform(object.matrix_world)
        return bm
