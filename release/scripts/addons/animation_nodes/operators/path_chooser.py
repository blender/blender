import bpy
from bpy.props import *

class ChoosePath(bpy.types.Operator):
    bl_idname = "an.choose_path"
    bl_label = "Choose Path"

    filepath = StringProperty(subtype = "FILE_PATH")
    callback = StringProperty()

    def invoke(self, context, event):
        context.window_manager.fileselect_add(self)
        return {"RUNNING_MODAL"}

    def execute(self, context):
        self.an_executeCallback(self.callback, self.filepath)
        return {"FINISHED"}
