import bpy
from ... base_types import AnimationNode

class ObjectGroupOperationsNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_ObjectGroupOperationsNode"
    bl_label = "Object Group Operations"

    def create(self):
        self.newInput("Object Group", "Group", "group", defaultDrawType = "PROPERTY_ONLY")
        self.newInput("Object", "Object", "object", defaultDrawType = "PROPERTY_ONLY")
        self.newInput("Boolean", "Linked", "linked")
        self.newOutput("Object Group", "Group", "group")

    def execute(self, group, object, linked):
        if group is None: return group
        if object is None: return group

        if object.name in group.objects:
            if not linked: group.objects.unlink(object)
        else:
            if linked: group.objects.link(object)

        return group
