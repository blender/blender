"""
Custom item in the right click menu
+++++++++++++++++++++++++++++++++++

This example enables you to insert your own menu entry into the common 
right click menu that you get while hovering over a value field, 
color, string, etc.

To make the example work, you have to first select an object
then right click on an user interface element (maybe a color in the 
material properties) and choose "Execute custom action".

Executing the operator will then dump all values directly to a 
console, so make sure to open a terminal by clicking on 
"Help >> Toggle System Console" or execute Blender directly 
from a terminal on your system of choice)

"""

import bpy
from bpy.types import Header, Menu, Panel

def dump(obj, text):
    print('-'*40, text, '-'*40)
    for attr in dir(obj):
        if hasattr( obj, attr ):
            print( "obj.%s = %s" % (attr, getattr(obj, attr)))

class TEST_OT_Rmb(bpy.types.Operator):
    """Right click entry test"""
    bl_idname = "object.rmb_test_op"
    bl_label = "Execute custom action"

    @classmethod
    def poll(cls, context):
        return context.active_object is not None

    def execute(self, context):
        if hasattr(context, 'button_pointer'):
            btn = context.button_pointer 
            dump(btn, 'button_pointer')

        if hasattr(context, 'button_prop'):
            prop = context.button_prop
            dump(prop, 'button_prop')

        if hasattr(context, 'button_operator'):
            op = context.button_operator
            dump(op, 'button_operator')     
            
        return {'FINISHED'}

# This class has to be exactly named like that to insert an entry in the right click menu
class WM_MT_button_context(Menu):
    bl_label = "Add Viddyoze Tag"

    def draw(self, context):
        pass

def menu_func(self, context):
    layout = self.layout
    layout.separator()
    layout.operator("object.rmb_test_op")

classes = (
    TEST_OT_Rmb,
    WM_MT_button_context,
)

def register():
    for cls in classes:
        bpy.utils.register_class(cls)
    bpy.types.WM_MT_button_context.append(menu_func)

def unregister():
    for cls in classes:
        bpy.utils.unregister_class(cls)
    bpy.types.WM_MT_button_context.remove(menu_func)

if __name__ == "__main__":
    register()