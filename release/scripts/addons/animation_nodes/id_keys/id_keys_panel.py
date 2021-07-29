import bpy
from . existing_keys import getAllIDKeys, IDKey, getUnremovableIDKeys
from .. utils.layout import splitAlignment
from .. utils.operators import makeOperator

hiddenIDKeys = set()

class IDKeyPanel(bpy.types.Panel):
    bl_idname = "an_id_keys_panel"
    bl_label = "ID Keys"
    bl_space_type = "VIEW_3D"
    bl_region_type = "TOOLS"
    bl_category = "AN"

    def draw(self, context):
        object = context.active_object
        self.drawHeader(self.layout)
        if object is not None:
            self.drawForObject(self.layout, object)

    def drawHeader(self, layout):
        col = layout.column(align = True)
        row = col.row(align = True)
        row.operator("an.new_id_key", text = "New", icon = "NEW")
        row.operator("an.update_id_keys_list", text = "Update", icon = "FILE_REFRESH")
        self.drawIDKeyList(col)

    def drawIDKeyList(self, layout):
        col = layout.box().column(align = True)
        unremovableIDKeys = getUnremovableIDKeys()
        for idKey in getAllIDKeys():
            row = col.row(align = True)
            icon = "RESTRICT_VIEW_ON" if idKey in hiddenIDKeys else "RESTRICT_VIEW_OFF"
            props = row.operator("an.toggle_id_key_visibility", text = "",
                                 icon = icon, emboss = False)
            props.dataType = idKey.type
            props.propertyName = idKey.name
            row.label(idKey.name)

            if idKey not in unremovableIDKeys:
                props = row.operator("an.remove_id_key", text = "", icon = "X", emboss = False)
                props.dataType = idKey.type
                props.propertyName = idKey.name

    def drawForObject(self, layout, object):
        for idKey in getAllIDKeys():
            if idKey not in hiddenIDKeys:
                self.drawIdKey(layout, object, idKey)

    def drawIdKey(self, layout, object, idKey):
        box = layout.box()
        exists = object.id_keys.exists(*idKey)
        self.drawIDKeyHeader(box, object, idKey, exists)
        if exists: object.id_keys.drawProperty(box, *idKey)
        object.id_keys.drawExtras(box, *idKey)

    def drawIDKeyHeader(self, layout, object, idKey, exists):
        left, right = splitAlignment(layout)
        left.label(idKey.name)

        if exists:
            props = right.operator("an.open_copy_id_key_menu",
                                   text = "", icon = "GHOST", emboss = False)
            props.keyDataType = idKey.type
            props.keyName = idKey.name

            props = right.operator("an.remove_id_key_on_selected_objects",
                                   text = "", icon = "X", emboss = False)
            props.dataType = idKey.type
            props.propertyName = idKey.name
        else:
            props = right.operator("an.create_id_key_on_selected_objects",
                                   text = "", icon = "PLUS", emboss = False)
            props.dataType = idKey.type
            props.propertyName = idKey.name

@makeOperator("an.create_id_key_on_selected_objects",
              "Create ID Key", arguments = ["String", "String"],
              description = "Create this ID Key on selected objects.")
def createIDKeyOnSelectedObjects(dataType, propertyName):
    for object in bpy.context.selected_objects:
        object.id_keys.create(dataType, propertyName)

@makeOperator("an.remove_id_key_on_selected_objects",
              "Remove ID Key", arguments = ["String", "String"], confirm = True,
              description = "Remove this ID Key on selected objects.")
def createIDKeyOnSelectedObjects(dataType, propertyName):
    for object in bpy.context.selected_objects:
        object.id_keys.remove(dataType, propertyName)

@makeOperator("an.toggle_id_key_visibility",
              "Toogle ID Key Visibility", arguments = ["String", "String"])
def toggleIDKeyVisibility(dataType, propertyName):
    idKey = IDKey(dataType, propertyName)
    if idKey in hiddenIDKeys:
        hiddenIDKeys.remove(idKey)
    else:
        hiddenIDKeys.add(idKey)
