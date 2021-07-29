import bpy
from ... base_types import AnimationNode

class GetObjectsFromGroupNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_GetObjectsFromGroupNode"
    bl_label = "Objects from Group"

    def create(self):
        self.newInput("Object Group", "Group", "group", defaultDrawType = "PROPERTY_ONLY")
        self.newOutput("Object List", "Objects", "objects")

    def execute(self, group):
        return list(getattr(group, "objects", []))
