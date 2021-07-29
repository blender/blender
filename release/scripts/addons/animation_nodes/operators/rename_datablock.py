import bpy
from bpy.props import *
from .. utils.blender_ui import getDpiFactor

class RenameDatablockPopupOperator(bpy.types.Operator):
    bl_idname = "an.rename_datablock_popup"
    bl_label = "Rename Datablock"

    oldName = StringProperty()
    newName = StringProperty()

    icon = StringProperty(default = "NONE")
    path = StringProperty(default = "bpy.data.objects")

    def invoke(self, context, event):
        self.newName = self.oldName
        return context.window_manager.invoke_props_dialog(self, width = 200 * getDpiFactor())

    def check(self, context):
        return True

    def draw(self, context):
        dataBlock = self.getDatablock()
        if dataBlock is None:
            self.layout.label("The datablock does not exist.", icon = "INFO")
        else:
            self.layout.prop(self, "newName", text = "", icon = self.icon)

    def execute(self, context):
        dataBlock = self.getDatablock()
        if dataBlock is not None:
            dataBlock.name = self.newName
        return {"FINISHED"}

    def getDatablock(self):
        try: return eval(self.path + "[{}]".format(repr(self.oldName)))
        except: return None
