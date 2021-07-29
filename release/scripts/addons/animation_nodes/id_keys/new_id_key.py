import bpy
from bpy.props import *
from . data_types import keyDataTypeItems
from .. utils.blender_ui import getDpiFactor, redrawAll
from . existing_keys import updateIdKeysList, IDKey, findsIDKeys, removesIDKey

createdIDKeys = set()

@findsIDKeys(removable = True)
def getCreatedIDKeys():
    return createdIDKeys

@removesIDKey
def removeIDKey(idKey):
    if idKey in createdIDKeys:
        createdIDKeys.remove(idKey)

class NewIDKey(bpy.types.Operator):
    bl_idname = "an.new_id_key"
    bl_label = "New ID Key"

    keyName = StringProperty(name = "Key Name")
    keyDataType = EnumProperty(name = "Key Data Type", items = keyDataTypeItems)

    def invoke(self, context, event):
        return context.window_manager.invoke_props_dialog(self, width = 250 * getDpiFactor())

    def draw(self, context):
        layout = self.layout
        layout.prop(self, "keyName", text = "Name")
        layout.prop(self, "keyDataType", text = "Data Type")

    def check(self, context):
        return True

    def execute(self, context):
        if self.keyName == "":
            self.report({"ERROR"}, "Cannot create Id Key with empty name")
            return {"CANCELLED"}

        idKey = IDKey(self.keyDataType, self.keyName)
        createdIDKeys.add(idKey)
        updateIdKeysList()
        redrawAll()
        return {"FINISHED"}
