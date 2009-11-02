import bpy
class_obj = bpy.types.Object

class_obj.getChildren = lambda ob: [child for child in bpy.data.objects if child.parent == ob]