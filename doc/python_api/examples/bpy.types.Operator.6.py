"""
Enum Search Popup
+++++++++++++++++

You may want to have an operator prompt the user to select an item
from a search field, this can be done using :class:`bpy.types.Operator.invoke_search_popup`.
"""
import bpy
from bpy.props import EnumProperty


class SearchEnumOperator(bpy.types.Operator):
    bl_idname = "object.search_enum_operator"
    bl_label = "Search Enum Operator"
    bl_property = "my_search"

    my_search = EnumProperty(
        name="My Search",
        items=(
            ('FOO', "Foo", ""),
            ('BAR', "Bar", ""),
            ('BAZ', "Baz", ""),
        ),
    )

    def execute(self, context):
        self.report({'INFO'}, "Selected:" + self.my_search)
        return {'FINISHED'}

    def invoke(self, context, event):
        context.window_manager.invoke_search_popup(self)
        return {'RUNNING_MODAL'}


bpy.utils.register_class(SearchEnumOperator)

# test call
bpy.ops.object.search_enum_operator('INVOKE_DEFAULT')
