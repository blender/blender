import bpy
from .... base_types import AnimationNode

class GetActiveCameraNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_GetActiveCameraNode"
    bl_label = "Get Active Camera"

    def create(self):
       self.newInput("Scene", "Scene", "scene", hide = True)
       self.newOutput("Object", "Active Camera", "activeCamera")

    def execute(self, scene):
       return getattr(scene, "camera", None)
