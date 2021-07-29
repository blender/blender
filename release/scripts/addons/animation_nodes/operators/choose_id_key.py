import bpy
from bpy.props import *
from .. id_keys import getAllIDKeys
from .. utils.unicode import toValidString, fromValidString

class IDKeySearch(bpy.types.Operator):
    bl_idname = "an.choose_id_key"
    bl_label = "ID Key Search"
    bl_options = {"REGISTER"}
    bl_property = "item"

    def getSearchItems(self, context):
        items = []
        for dataType, name in getAllIDKeys():
            identifier = toValidString("{} * {}".format(dataType, name))
            items.append((identifier, name, ""))
        return items

    item = EnumProperty(items = getSearchItems)
    callback = StringProperty()

    def invoke(self, context, event):
        bpy.ops.an.update_id_keys_list()
        context.window_manager.invoke_search_popup(self)
        return {"CANCELLED"}

    def execute(self, context):
        item = fromValidString(self.item)
        if item == "NONE": return {"CANCELLED"}
        dataType, name = item.split(" * ")
        self.an_executeCallback(self.callback, dataType, name)
        return {"FINISHED"}
