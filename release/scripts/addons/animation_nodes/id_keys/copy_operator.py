import bpy
from bpy.props import *
from .. utils.operators import makeOperator
from . data_types import dataTypeByIdentifier
from .. utils.attributes import setattrRecursive

class CopyIDKeyMenuOpener(bpy.types.Operator):
    bl_idname = "an.open_copy_id_key_menu"
    bl_label = "Open Copy ID Key Menu"
    bl_options = {"INTERNAL"}

    keyDataType = StringProperty()
    keyName = StringProperty()

    @classmethod
    def poll(cls, context):
        return context.active_object is not None

    def invoke(self, context, event):
        context.window_manager.popup_menu(self.drawMenu, title = "Copy ID Key", icon = "GHOST")
        return {"FINISHED"}

    def drawMenu(self, menu, context):
        layout = menu.layout.column()
        props = layout.operator("an.copy_id_key_to_selected_objects", text = "to Selected Objects")
        props.dataType = self.keyDataType
        props.propertyName = self.keyName

        typeClass = dataTypeByIdentifier[self.keyDataType]
        typeClass.drawCopyMenu(layout, context.active_object, self.keyName)


@makeOperator("an.copy_id_key_to_selected_objects", "Copy ID Key",
              arguments = ["String", "String"],
              description = "Copy this ID Key from active to all selected objects.")
def copyIDKeyToSelectedObjects(dataType, propertyName):
    activeObject = bpy.context.active_object
    if activeObject is None: return

    value = activeObject.id_keys.get(dataType, propertyName)
    for object in bpy.context.selected_objects:
        object.id_keys.set(dataType, propertyName, value)

@makeOperator("an.copy_id_key_to_attribute", "Copy ID Key to Attribute",
              arguments = ["String", "String", "String"],
              description = "Copy this ID Key to an attribute.")
def copyIntegerIDKeyToAttribute(dataType, propertyName, attribute):
    for object in bpy.context.selected_objects:
        setattrRecursive(object, attribute, object.id_keys.get(dataType, propertyName))
