import bpy
from . base import SingleIDKeyDataType
from ... utils.operators import makeOperator

class TextDataType(SingleIDKeyDataType):
    identifier = "Text"
    default = ""

    @classmethod
    def drawExtras(cls, layout, object, name):
        props = layout.operator("an.id_keys_from_text_body", icon = "OUTLINER_DATA_FONT")
        props.name = name

    @classmethod
    def drawCopyMenu(cls, layout, object, name):
        props = layout.operator("an.copy_id_key_to_attribute", text = "to Text Body")
        props.dataType = "Text"
        props.propertyName = name
        props.attribute = "data.body"

@makeOperator("an.id_keys_from_text_body", "From Text Body", arguments = ["String"],
              description = "Assign text ID Keys based on text body.")
def idKeyFromTextBody(name):
    for object in bpy.context.selected_objects:
        if object.type == "FONT":
            object.id_keys.set("Text", name, object.data.body)
        else:
            object.id_keys.set("Text", name, "")
