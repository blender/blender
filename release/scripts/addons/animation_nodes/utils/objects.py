import bpy

def enterObjectMode():
    if getattr(bpy.context.active_object, "mode", "OBJECT") != "OBJECT":
        bpy.ops.object.mode_set(mode = "OBJECT")